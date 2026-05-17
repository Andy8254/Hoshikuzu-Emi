#include "core/sqlite.hpp"
#include "core/Log.hpp"
#include "tournament/bracket/MatchStore.hpp"
#include "tournament/bracket/Bracket.hpp"
#include "tournament/registration.hpp"
#include "tournament/matchmaking.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <sqlite3.h>

// MatchStore is the persistent match backend.
//
// It intentionally keeps Discord-specific work out of the match lifecycle:
// commands/buttons call this file, and Discord thread/message creation lives in
// tournament/discord. Read this file in five bands:
//
// 1. SQLite row mapping and small SQL helpers.
// 2. Destination helpers for elimination routing.
// 3. Match generation persistence for elimination, round robin, and Swiss.
// 4. Format/lifecycle rules for report, correction, standings, and no-shows.
// 5. Public API functions used by command handlers and Discord UI helpers.
//
// Bracket names define behavior:
// winners/losers/grand_finals route players, round_robin and swiss update
// standings only, and swiss_bye is an auto-completed non-reportable record.

namespace {
	void log_match_store(const std::string& action, const std::string& detail = "") {
		bot_log::info("match-store", action, detail);
	}

	void log_match_store_error(const std::string& action, const std::string& detail = "") {
		bot_log::error("match-store", action, detail);
	}

	Database& get_db() {
		static Database instance("db/master.db");
		return instance;
	}

	const char* MATCH_SELECT_COLUMNS =
		"id, tournament_id, bracket_match_index, round, position, bracket, "
		"player_a_id, player_b_id, winner_id, score_a, score_b, state, streamed, thread_id, message_id, "
		"player_a_checked_in, player_b_checked_in, match_opened_at, grace_time, no_show_resolved, "
		"no_show_reason, pending_auto_dq_player_id, "
		"next_winner_match, next_winner_slot, next_loser_match, next_loser_slot";

	bool bind_text(sqlite3_stmt* stmt, int index, const std::string& value) {
		return sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
	}

	std::string column_text(sqlite3_stmt* stmt, int column) {
		const unsigned char* value = sqlite3_column_text(stmt, column);
		return value ? reinterpret_cast<const char*>(value) : "";
	}

	tournament_bracket::StoredMatch read_match(sqlite3_stmt* stmt) {
		tournament_bracket::StoredMatch match;
		match.id = sqlite3_column_int(stmt, 0);
		match.tournament_id = sqlite3_column_int(stmt, 1);
		match.bracket_match_index = sqlite3_column_int(stmt, 2);
		match.round = sqlite3_column_int(stmt, 3);
		match.position = sqlite3_column_int(stmt, 4);
		match.bracket = column_text(stmt, 5);
		match.player_a_id = column_text(stmt, 6);
		match.player_b_id = column_text(stmt, 7);
		match.winner_id = column_text(stmt, 8);
		match.score_a = sqlite3_column_int(stmt, 9);
		match.score_b = sqlite3_column_int(stmt, 10);
		match.state = tournament_bracket::state_from_string(column_text(stmt, 11));
		match.streamed = sqlite3_column_int(stmt, 12) != 0;
		match.thread_id = static_cast<dpp::snowflake>(sqlite3_column_int64(stmt, 13));
		match.message_id = static_cast<dpp::snowflake>(sqlite3_column_int64(stmt, 14));
		match.player_a_checked_in = sqlite3_column_int(stmt, 15) != 0;
		match.player_b_checked_in = sqlite3_column_int(stmt, 16) != 0;
		match.match_opened_at = sqlite3_column_int(stmt, 17);
		match.grace_time = sqlite3_column_int(stmt, 18);
		match.no_show_resolved = sqlite3_column_int(stmt, 19) != 0;
		match.no_show_reason = column_text(stmt, 20);
		match.pending_auto_dq_player_id = column_text(stmt, 21);
		match.next_winner_match = sqlite3_column_int(stmt, 22);
		match.next_winner_slot = sqlite3_column_int(stmt, 23);
		match.next_loser_match = sqlite3_column_int(stmt, 24);
		match.next_loser_slot = sqlite3_column_int(stmt, 25);
		return match;
	}

	bool is_completed(tournament_bracket::StoredMatchState state);
	bool can_report_state(tournament_bracket::StoredMatchState state);
	bool is_swiss_bracket(const std::string& bracket);
	bool is_bye_bracket(const std::string& bracket);
	bool supports_routing(const std::string& bracket);
	bool supports_standings(const std::string& bracket);
	bool supports_due_no_show_resolution(const std::string& bracket);
	bool correction_locked_by_later_swiss_round(int tournament_id, const tournament_bracket::StoredMatch& match);

	bool insert_match(int tournament_id, int bracket_index, const Match& source) {
		sqlite3_stmt* stmt = nullptr;
		const char* sql =
			"INSERT INTO tournament_matches "
			"(tournament_id, bracket_match_index, round, position, bracket, player_a_id, player_b_id, winner_id, "
			"score_a, score_b, state, next_winner_match, next_winner_slot, next_loser_match, next_loser_slot) "
			"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

		if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
			return false;
		}

		tournament_bracket::StoredMatchState state = tournament_bracket::StoredMatchState::Pending;
		if (source.state == MatchState::Completed) {
			state = tournament_bracket::StoredMatchState::Completed;
		}
		else if (!source.playerA_id.empty() && !source.playerB_id.empty()) {
			state = tournament_bracket::StoredMatchState::Ready;
		}

		const bool bound =
			sqlite3_bind_int(stmt, 1, tournament_id) == SQLITE_OK
			&& sqlite3_bind_int(stmt, 2, bracket_index) == SQLITE_OK
			&& sqlite3_bind_int(stmt, 3, source.round) == SQLITE_OK
			&& sqlite3_bind_int(stmt, 4, source.position) == SQLITE_OK
			&& bind_text(stmt, 5, source.bracket)
			&& bind_text(stmt, 6, source.playerA_id)
			&& bind_text(stmt, 7, source.playerB_id)
			&& bind_text(stmt, 8, source.winner_id)
			&& sqlite3_bind_int(stmt, 9, source.scoreA) == SQLITE_OK
			&& sqlite3_bind_int(stmt, 10, source.scoreB) == SQLITE_OK
			&& bind_text(stmt, 11, tournament_bracket::state_to_string(state))
			&& sqlite3_bind_int(stmt, 12, source.next_winner_match) == SQLITE_OK
			&& sqlite3_bind_int(stmt, 13, source.next_winner_slot) == SQLITE_OK
			&& sqlite3_bind_int(stmt, 14, source.next_loser_match) == SQLITE_OK
			&& sqlite3_bind_int(stmt, 15, source.next_loser_slot) == SQLITE_OK;

		const bool success = bound && sqlite3_step(stmt) == SQLITE_DONE;
		sqlite3_finalize(stmt);
		return success;
	}

	std::vector<tournament_bracket::StoredMatch> query_matches(const char* sql, int tournament_id, int extra = 0) {
		std::vector<tournament_bracket::StoredMatch> result;
		if (!tournament_bracket::init()) return result;

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
			return result;
		}

		sqlite3_bind_int(stmt, 1, tournament_id);
		if (extra != 0) {
			sqlite3_bind_int(stmt, 2, extra);
		}

		while (sqlite3_step(stmt) == SQLITE_ROW) {
			result.push_back(read_match(stmt));
		}

		sqlite3_finalize(stmt);
		return result;
	}

	bool ensure_ready_if_filled(int tournament_id, int bracket_match_index) {
		sqlite3_stmt* stmt = nullptr;
		const char* sql =
			"UPDATE tournament_matches "
			"SET state = 'ready' "
			"WHERE tournament_id = ? AND bracket_match_index = ? "
			"AND state = 'pending' "
			"AND player_a_id <> '' "
			"AND player_b_id <> '';";

		if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
			return false;
		}

		sqlite3_bind_int(stmt, 1, tournament_id);
		sqlite3_bind_int(stmt, 2, bracket_match_index);
		const bool success = sqlite3_step(stmt) == SQLITE_DONE;
		sqlite3_finalize(stmt);
		return success;
	}

	bool place_player(int tournament_id, int bracket_match_index, int slot, const std::string& player_id) {
		if (bracket_match_index < 0 || player_id.empty() || (slot != 0 && slot != 1)) {
			return true;
		}

		const char* column = slot == 0 ? "player_a_id" : "player_b_id";
		std::string sql = std::string("UPDATE tournament_matches SET ") + column + " = ? "
			"WHERE tournament_id = ? AND bracket_match_index = ?;";

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(get_db().get_handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			return false;
		}

		bind_text(stmt, 1, player_id);
		sqlite3_bind_int(stmt, 2, tournament_id);
		sqlite3_bind_int(stmt, 3, bracket_match_index);
		const bool success = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().get_handle()) > 0;
		sqlite3_finalize(stmt);

		return success && ensure_ready_if_filled(tournament_id, bracket_match_index);
	}

	std::optional<tournament_bracket::StoredMatch> get_match_by_bracket_index(int tournament_id, int bracket_match_index) {
		if (bracket_match_index < 0 || !tournament_bracket::init()) return std::nullopt;

		sqlite3_stmt* stmt = nullptr;
		const std::string sql = std::string("SELECT ") + MATCH_SELECT_COLUMNS +
			" FROM tournament_matches WHERE tournament_id = ? AND bracket_match_index = ? LIMIT 1;";

		if (sqlite3_prepare_v2(get_db().get_handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			return std::nullopt;
		}

		sqlite3_bind_int(stmt, 1, tournament_id);
		sqlite3_bind_int(stmt, 2, bracket_match_index);

		std::optional<tournament_bracket::StoredMatch> result;
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			result = read_match(stmt);
		}

		sqlite3_finalize(stmt);
		return result;
	}

	bool destination_completed(int tournament_id, int bracket_match_index) {
		const auto match = get_match_by_bracket_index(tournament_id, bracket_match_index);
		return match && match->state == tournament_bracket::StoredMatchState::Completed;
	}

	bool clear_destination_slot(int tournament_id, int bracket_match_index, int slot) {
		if (bracket_match_index < 0 || (slot != 0 && slot != 1)) {
			return true;
		}

		const char* column = slot == 0 ? "player_a_id" : "player_b_id";
		std::string sql = std::string("UPDATE tournament_matches SET ") + column + " = '', state = 'pending' "
			"WHERE tournament_id = ? AND bracket_match_index = ? AND state <> 'completed';";

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(get_db().get_handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			return false;
		}

		sqlite3_bind_int(stmt, 1, tournament_id);
		sqlite3_bind_int(stmt, 2, bracket_match_index);
		const bool success = sqlite3_step(stmt) == SQLITE_DONE;
		sqlite3_finalize(stmt);
		return success;
	}

	int participant_seed(int tournament_id, const std::string& discord_id) {
		const auto participant = tournament_registration::get_participant(tournament_id, discord_id);
		if (!participant || participant->seed <= 0) {
			return 1000000;
		}

		return participant->seed;
	}

	bool mark_pending_auto_dq(int tournament_id, int bracket_match_index, const std::string& discord_id) {
		if (bracket_match_index < 0 || discord_id.empty()) {
			return true;
		}

		sqlite3_stmt* stmt = nullptr;
		const char* sql =
			"UPDATE tournament_matches "
			"SET pending_auto_dq_player_id = ? "
			"WHERE tournament_id = ? AND bracket_match_index = ? AND state <> 'completed';";

		if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
			return false;
		}

		bind_text(stmt, 1, discord_id);
		sqlite3_bind_int(stmt, 2, tournament_id);
		sqlite3_bind_int(stmt, 3, bracket_match_index);
		const bool success = sqlite3_step(stmt) == SQLITE_DONE;
		sqlite3_finalize(stmt);
		return success;
	}

	bool mark_no_show_metadata(int tournament_id, int match_id, const std::string& reason) {
		sqlite3_stmt* stmt = nullptr;
		const char* sql =
			"UPDATE tournament_matches "
			"SET no_show_resolved = 1, no_show_reason = ?, pending_auto_dq_player_id = '' "
			"WHERE tournament_id = ? AND id = ?;";

		if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
			return false;
		}

		bind_text(stmt, 1, reason);
		sqlite3_bind_int(stmt, 2, tournament_id);
		sqlite3_bind_int(stmt, 3, match_id);
		const bool success = sqlite3_step(stmt) == SQLITE_DONE;
		sqlite3_finalize(stmt);
		return success;
	}

	bool forfeit_match_player(
		const tournament_bracket::StoredMatch& match,
		const std::string& discord_id,
		const std::string& reason,
		tournament_registration::ParticipantStatus status
	) {
		if (is_completed(match.state)
			|| match.player_a_id.empty()
			|| match.player_b_id.empty()
			|| discord_id.empty()
			|| (discord_id != match.player_a_id && discord_id != match.player_b_id)) {
			return false;
		}

		const int score_a = discord_id == match.player_a_id ? 0 : 1;
		const int score_b = discord_id == match.player_b_id ? 0 : 1;
		if (!tournament_bracket::report_match(match.tournament_id, match.id, score_a, score_b)) {
			return false;
		}

		tournament_registration::set_participant_status(match.tournament_id, discord_id, status);
		return mark_no_show_metadata(match.tournament_id, match.id, reason);
	}

	bool seed_bracket_matches(int tournament_id, bool double_elimination) {
		if (tournament_id <= 0 || !tournament_bracket::init()) return false;
		const auto participants = tournament_registration::list_checked_in_participants(tournament_id);
		if (participants.size() < 2) return false;

		std::vector<std::string> seeded_players;
		for (const auto& participant : participants) {
			seeded_players.push_back(participant.discord_id);
		}

		Bracket bracket;
		if (double_elimination) {
			bracket.generate_double_elimination(seeded_players);
		}
		else {
			bracket.generate_single_elimination(seeded_players);
		}

		if (bracket.matches.empty()) return false;
		if (!tournament_bracket::clear_matches(tournament_id)) return false;

		DatabaseTransaction transaction(get_db());
		if (!transaction.ok()) return false;
		for (int i = 0; i < static_cast<int>(bracket.matches.size()); ++i) {
			if (!insert_match(tournament_id, i, bracket.matches[i])) {
				return false;
			}
		}
		return transaction.commit();
	}

	bool seed_round_robin_matches(int tournament_id) {
		if (tournament_id <= 0 || !tournament_bracket::init()) return false;
		const auto participants = tournament_registration::list_checked_in_participants(tournament_id);
		if (participants.size() < 2) return false;

		std::vector<std::string> seeded_players;
		for (const auto& participant : participants) {
			seeded_players.push_back(participant.discord_id);
		}

		Bracket bracket;
		bracket.generate_round_robin(seeded_players);
		if (bracket.matches.empty()) return false;
		if (!tournament_bracket::clear_matches(tournament_id)) return false;

		DatabaseTransaction transaction(get_db());
		if (!transaction.ok()) return false;
		for (int i = 0; i < static_cast<int>(bracket.matches.size()); ++i) {
			if (!insert_match(tournament_id, i, bracket.matches[i])) {
				return false;
			}
		}
		return transaction.commit();
	}

	struct SwissStanding {
		std::string player_id;
		int score = 0;
		int seed = 1000000;
		bool had_bye = false;
	};

	std::string pair_key(const std::string& a, const std::string& b) {
		return a < b ? a + ":" + b : b + ":" + a;
	}

	bool is_completed(tournament_bracket::StoredMatchState state) {
		return state == tournament_bracket::StoredMatchState::Completed;
	}

	bool can_report_state(tournament_bracket::StoredMatchState state) {
		return state == tournament_bracket::StoredMatchState::Ready
			|| state == tournament_bracket::StoredMatchState::Ongoing;
	}

	bool is_swiss_bracket(const std::string& bracket) {
		return bracket == "swiss" || bracket == "swiss_bye";
	}

	bool is_round_robin_bracket(const std::string& bracket) {
		return bracket == "round_robin";
	}

	bool is_elimination_bracket(const std::string& bracket) {
		return bracket == "winners"
			|| bracket == "losers"
			|| bracket == "grand_finals";
	}

	bool is_bye_bracket(const std::string& bracket) {
		return bracket == "swiss_bye";
	}

	bool supports_routing(const std::string& bracket) {
		return is_elimination_bracket(bracket);
	}

	bool supports_standings(const std::string& bracket) {
		return is_round_robin_bracket(bracket) || is_swiss_bracket(bracket);
	}

	bool supports_due_no_show_resolution(const std::string& bracket) {
		return is_elimination_bracket(bracket);
	}

	bool correction_locked_by_later_swiss_round(int tournament_id, const tournament_bracket::StoredMatch& match) {
		if (!is_swiss_bracket(match.bracket)) {
			return false;
		}

		for (const auto& existing : tournament_bracket::list_matches(tournament_id)) {
			if (is_swiss_bracket(existing.bracket) && existing.round > match.round) {
				return true;
			}
		}

		return false;
	}

	bool has_open_matches(const std::vector<tournament_bracket::StoredMatch>& matches, int round) {
		for (const auto& match : matches) {
			if (match.round == round
				&& match.bracket == "swiss"
				&& !is_completed(match.state)) {
				return true;
			}
		}
		return false;
	}

	bool append_generated_matches(int tournament_id, int start_index, const std::vector<Match>& matches) {
		DatabaseTransaction transaction(get_db());
		if (!transaction.ok()) return false;
		for (int i = 0; i < static_cast<int>(matches.size()); ++i) {
			if (!insert_match(tournament_id, start_index + i, matches[i])) {
				return false;
			}
		}
		return transaction.commit();
	}
}

bool tournament_bracket::init() {
	const char* sql =
		"CREATE TABLE IF NOT EXISTS tournament_matches ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"tournament_id INTEGER NOT NULL,"
		"bracket_match_index INTEGER NOT NULL,"
		"round INTEGER NOT NULL,"
		"position INTEGER NOT NULL,"
		"bracket TEXT DEFAULT 'winners',"
		"player_a_id TEXT,"
		"player_b_id TEXT,"
		"winner_id TEXT,"
		"score_a INTEGER DEFAULT 0,"
		"score_b INTEGER DEFAULT 0,"
		"state TEXT DEFAULT 'pending',"
		"streamed INTEGER DEFAULT 0,"
		"thread_id INTEGER DEFAULT 0,"
		"message_id INTEGER DEFAULT 0,"
		"player_a_checked_in INTEGER DEFAULT 0,"
		"player_b_checked_in INTEGER DEFAULT 0,"
		"match_opened_at INTEGER DEFAULT 0,"
		"grace_time INTEGER DEFAULT 600,"
		"no_show_resolved INTEGER DEFAULT 0,"
		"no_show_reason TEXT DEFAULT '',"
		"pending_auto_dq_player_id TEXT DEFAULT '',"
		"next_winner_match INTEGER DEFAULT -1,"
		"next_winner_slot INTEGER DEFAULT -1,"
		"next_loser_match INTEGER DEFAULT -1,"
		"next_loser_slot INTEGER DEFAULT -1,"
		"UNIQUE(tournament_id, bracket_match_index),"
		"FOREIGN KEY (tournament_id) REFERENCES tournaments(id) ON DELETE CASCADE"
		");";

	if (!get_db().execute(sql)) {
		return false;
	}

	const char* migrations[] = {
		"player_a_checked_in INTEGER DEFAULT 0",
		"player_b_checked_in INTEGER DEFAULT 0",
		"match_opened_at INTEGER DEFAULT 0",
		"grace_time INTEGER DEFAULT 600",
		"no_show_resolved INTEGER DEFAULT 0",
		"no_show_reason TEXT DEFAULT ''",
		"pending_auto_dq_player_id TEXT DEFAULT ''",
		"next_winner_match INTEGER DEFAULT -1",
		"next_winner_slot INTEGER DEFAULT -1",
		"next_loser_match INTEGER DEFAULT -1",
		"next_loser_slot INTEGER DEFAULT -1"
	};

	for (const char* migration : migrations) {
		if (!get_db().add_column_if_missing("tournament_matches", migration)) {
			return false;
		}
	}

	return get_db().create_index_if_missing(
		"idx_tournament_matches_tournament_state",
		"tournament_matches",
		"tournament_id, state"
	)
		&& get_db().create_index_if_missing(
			"idx_tournament_matches_tournament_round",
			"tournament_matches",
			"tournament_id, round"
		)
		&& get_db().create_index_if_missing(
			"idx_tournament_matches_tournament_bracket",
			"tournament_matches",
			"tournament_id, bracket"
		)
		&& get_db().set_schema_version(1);
}

bool tournament_bracket::clear_matches(int tournament_id) {
	if (tournament_id <= 0 || !init()) return false;
	sqlite3_stmt* stmt = nullptr;
	const char* sql = "DELETE FROM tournament_matches WHERE tournament_id = ?;";
	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
	sqlite3_bind_int(stmt, 1, tournament_id);
	const bool success = sqlite3_step(stmt) == SQLITE_DONE;
	sqlite3_finalize(stmt);
	return success;
}

bool tournament_bracket::generate_single_elimination(int tournament_id) {
	log_match_store("generate_single_start", "tournament_id=" + std::to_string(tournament_id));
	const bool ok = seed_bracket_matches(tournament_id, false);
	log_match_store(ok ? "generate_single_ok" : "generate_single_failed", "tournament_id=" + std::to_string(tournament_id));
	return ok;
}

bool tournament_bracket::generate_double_elimination(int tournament_id) {
	log_match_store("generate_double_start", "tournament_id=" + std::to_string(tournament_id));
	const bool ok = seed_bracket_matches(tournament_id, true);
	log_match_store(ok ? "generate_double_ok" : "generate_double_failed", "tournament_id=" + std::to_string(tournament_id));
	return ok;
}

bool tournament_bracket::generate_round_robin(int tournament_id) {
	log_match_store("generate_round_robin_start", "tournament_id=" + std::to_string(tournament_id));
	const bool ok = seed_round_robin_matches(tournament_id);
	log_match_store(ok ? "generate_round_robin_ok" : "generate_round_robin_failed", "tournament_id=" + std::to_string(tournament_id));
	return ok;
}

bool tournament_bracket::generate_swiss_round(int tournament_id) {
	log_match_store("generate_swiss_start", "tournament_id=" + std::to_string(tournament_id));
	if (tournament_id <= 0 || !init()) return false;

	const auto participants = tournament_registration::list_checked_in_participants(tournament_id);
	if (participants.size() < 2) return false;

	std::vector<StoredMatch> existing = list_matches(tournament_id);
	int latest_round = -1;
	int next_index = 0;
	for (const auto& match : existing) {
		next_index = std::max(next_index, match.bracket_match_index + 1);
		if (is_swiss_bracket(match.bracket)) {
			latest_round = std::max(latest_round, match.round);
		}
	}

	if (latest_round >= 0 && has_open_matches(existing, latest_round)) {
		return false;
	}

	std::map<std::string, SwissStanding> standings;
	for (const auto& participant : participants) {
		standings[participant.discord_id] = SwissStanding{
			participant.discord_id,
			0,
			participant.seed > 0 ? participant.seed : 1000000,
			false
		};
	}

	std::set<std::string> previous_pairs;
	for (const auto& match : existing) {
		if (!is_swiss_bracket(match.bracket)) {
			continue;
		}

		if (match.bracket == "swiss_bye") {
			auto standing = standings.find(match.winner_id);
			if (standing != standings.end()) {
				standing->second.score += 1;
				standing->second.had_bye = true;
			}
			continue;
		}

		if (!match.player_a_id.empty() && !match.player_b_id.empty()) {
			previous_pairs.insert(pair_key(match.player_a_id, match.player_b_id));
		}

		if (!is_completed(match.state)) {
			continue;
		}

		auto winner = standings.find(match.winner_id);
		if (winner != standings.end()) {
			winner->second.score += 1;
		}
	}

	std::vector<SwissStanding> players;
	for (const auto& [_, standing] : standings) {
		players.push_back(standing);
	}

	std::stable_sort(
		players.begin(),
		players.end(),
		[](const SwissStanding& a, const SwissStanding& b) {
			if (a.score != b.score) return a.score > b.score;
			if (a.seed != b.seed) return a.seed < b.seed;
			return a.player_id < b.player_id;
		}
	);

	std::vector<Match> new_matches;
	const int round = latest_round + 1;

	if (players.size() % 2 != 0) {
		auto bye_it = players.end();
		for (auto it = players.end(); it != players.begin();) {
			--it;
			if (!it->had_bye) {
				bye_it = it;
				break;
			}
		}
		if (bye_it == players.end()) {
			bye_it = players.end() - 1;
		}

		Match bye;
		bye.bracket = "swiss_bye";
		bye.round = round;
		bye.position = static_cast<int>(new_matches.size());
		bye.playerA_id = bye_it->player_id;
		bye.winner_id = bye_it->player_id;
		bye.scoreA = 1;
		bye.scoreB = 0;
		bye.state = MatchState::Completed;
		new_matches.push_back(bye);
		players.erase(bye_it);
	}

	int position = static_cast<int>(new_matches.size());
	while (!players.empty()) {
		SwissStanding first = players.front();
		players.erase(players.begin());

		auto opponent_it = players.begin();
		for (auto it = players.begin(); it != players.end(); ++it) {
			if (!previous_pairs.count(pair_key(first.player_id, it->player_id))) {
				opponent_it = it;
				break;
			}
		}

		if (opponent_it == players.end()) {
			return false;
		}

		SwissStanding second = *opponent_it;
		players.erase(opponent_it);

		Match match;
		match.bracket = "swiss";
		match.round = round;
		match.position = position++;
		match.playerA_id = first.player_id;
		match.playerB_id = second.player_id;
		match.state = MatchState::Ongoing;
		match.next_winner_match = DEST_NONE;
		match.next_loser_match = DEST_NONE;
		new_matches.push_back(match);
	}

	if (new_matches.empty()) return false;
	if (latest_round < 0) {
		if (!clear_matches(tournament_id)) return false;
		next_index = 0;
	}
	const bool ok = append_generated_matches(tournament_id, next_index, new_matches);
	log_match_store(ok ? "generate_swiss_ok" : "generate_swiss_failed", "tournament_id=" + std::to_string(tournament_id) + " new_matches=" + std::to_string(new_matches.size()));
	return ok;
}

std::optional<tournament_bracket::StoredMatch> tournament_bracket::get_match(int tournament_id, int match_id) {
	if (tournament_id <= 0 || match_id <= 0 || !init()) return std::nullopt;
	sqlite3_stmt* stmt = nullptr;
	const std::string sql = std::string("SELECT ") + MATCH_SELECT_COLUMNS +
		" FROM tournament_matches WHERE tournament_id = ? AND id = ? LIMIT 1;";
	if (sqlite3_prepare_v2(get_db().get_handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;
	sqlite3_bind_int(stmt, 1, tournament_id);
	sqlite3_bind_int(stmt, 2, match_id);

	std::optional<StoredMatch> result;
	if (sqlite3_step(stmt) == SQLITE_ROW) result = read_match(stmt);
	sqlite3_finalize(stmt);
	return result;
}

std::vector<tournament_bracket::StoredMatch> tournament_bracket::list_matches(int tournament_id) {
	const std::string sql = std::string("SELECT ") + MATCH_SELECT_COLUMNS +
		" FROM tournament_matches WHERE tournament_id = ? ORDER BY round ASC, bracket ASC, position ASC;";
	return query_matches(sql.c_str(), tournament_id);
}

std::vector<tournament_bracket::StoredMatch> tournament_bracket::list_current_matches(int tournament_id) {
	const std::string sql = std::string("SELECT ") + MATCH_SELECT_COLUMNS +
		" FROM tournament_matches WHERE tournament_id = ? AND state IN ('ready', 'ongoing') ORDER BY round ASC, bracket ASC, position ASC;";
	return query_matches(sql.c_str(), tournament_id);
}

std::vector<tournament_bracket::StoredMatch> tournament_bracket::list_round_matches(int tournament_id, int round) {
	const std::string sql = std::string("SELECT ") + MATCH_SELECT_COLUMNS +
		" FROM tournament_matches WHERE tournament_id = ? AND round = ? ORDER BY bracket ASC, position ASC;";
	return query_matches(sql.c_str(), tournament_id, round);
}

std::vector<tournament_bracket::StoredMatch> tournament_bracket::list_streamed_matches(int tournament_id) {
	const std::string sql = std::string("SELECT ") + MATCH_SELECT_COLUMNS +
		" FROM tournament_matches WHERE tournament_id = ? AND streamed = 1 ORDER BY round ASC, bracket ASC, position ASC;";
	return query_matches(sql.c_str(), tournament_id);
}

std::vector<tournament_bracket::FormatStanding> tournament_bracket::list_format_standings(int tournament_id) {
	std::map<std::string, FormatStanding> standings;

	for (const auto& participant : tournament_registration::list_checked_in_participants(tournament_id)) {
		standings[participant.discord_id] = FormatStanding{
			participant.discord_id,
			0,
			0,
			0,
			0,
			participant.seed
		};
	}

	for (const auto& match : list_matches(tournament_id)) {
		if (!supports_standings(match.bracket)) {
			continue;
		}

		if (match.bracket == "swiss_bye") {
			auto standing = standings.find(match.winner_id);
			if (standing != standings.end()) {
				standing->second.wins += 1;
				standing->second.byes += 1;
				standing->second.points += 1;
			}
			continue;
		}

		if (!is_completed(match.state) || match.winner_id.empty()) {
			continue;
		}

		const std::string loser = match.winner_id == match.player_a_id ? match.player_b_id : match.player_a_id;
		auto winner = standings.find(match.winner_id);
		if (winner != standings.end()) {
			winner->second.wins += 1;
			winner->second.points += 1;
		}

		auto loser_it = standings.find(loser);
		if (loser_it != standings.end()) {
			loser_it->second.losses += 1;
		}
	}

	std::vector<FormatStanding> result;
	for (const auto& [_, standing] : standings) {
		if (standing.wins || standing.losses || standing.byes) {
			result.push_back(standing);
		}
	}

	std::stable_sort(
		result.begin(),
		result.end(),
		[](const FormatStanding& a, const FormatStanding& b) {
			if (a.points != b.points) return a.points > b.points;
			if (a.wins != b.wins) return a.wins > b.wins;
			if (a.losses != b.losses) return a.losses < b.losses;
			if (a.seed != b.seed) {
				if (a.seed <= 0) return false;
				if (b.seed <= 0) return true;
				return a.seed < b.seed;
			}
			return a.player_id < b.player_id;
		}
	);

	return result;
}

bool tournament_bracket::assign_streamed(int tournament_id, int match_id, bool streamed) {
	if (tournament_id <= 0 || match_id <= 0 || !init()) return false;
	sqlite3_stmt* stmt = nullptr;
	const char* sql = "UPDATE tournament_matches SET streamed = ? WHERE tournament_id = ? AND id = ?;";
	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
	sqlite3_bind_int(stmt, 1, streamed ? 1 : 0);
	sqlite3_bind_int(stmt, 2, tournament_id);
	sqlite3_bind_int(stmt, 3, match_id);
	const bool success = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().get_handle()) > 0;
	sqlite3_finalize(stmt);
	log_match_store(success ? "stream_assign_ok" : "stream_assign_failed", "tournament_id=" + std::to_string(tournament_id) + " match_id=" + std::to_string(match_id) + " streamed=" + std::to_string(streamed ? 1 : 0));
	return success;
}

bool tournament_bracket::set_discord_thread(int tournament_id, int match_id, dpp::snowflake thread_id, dpp::snowflake message_id) {
	if (tournament_id <= 0 || match_id <= 0 || !init()) return false;
	sqlite3_stmt* stmt = nullptr;
	const char* sql = "UPDATE tournament_matches SET thread_id = ?, message_id = ? WHERE tournament_id = ? AND id = ?;";
	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
	sqlite3_bind_int64(stmt, 1, thread_id);
	sqlite3_bind_int64(stmt, 2, message_id);
	sqlite3_bind_int(stmt, 3, tournament_id);
	sqlite3_bind_int(stmt, 4, match_id);
	const bool success = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().get_handle()) > 0;
	sqlite3_finalize(stmt);
	log_match_store(success ? "thread_set_ok" : "thread_set_failed", "tournament_id=" + std::to_string(tournament_id) + " match_id=" + std::to_string(match_id));
	return success;
}

bool tournament_bracket::mark_match_opened(int tournament_id, int match_id, int opened_at, int grace_time) {
	if (tournament_id <= 0 || match_id <= 0 || opened_at <= 0 || !init()) return false;
	if (grace_time <= 0) {
		grace_time = tournament_matchmaking::DEFAULT_MATCH_GRACE_TIME;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"UPDATE tournament_matches "
		"SET match_opened_at = ?, grace_time = ? "
		"WHERE tournament_id = ? AND id = ? AND match_opened_at = 0;";
	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
	sqlite3_bind_int(stmt, 1, opened_at);
	sqlite3_bind_int(stmt, 2, grace_time);
	sqlite3_bind_int(stmt, 3, tournament_id);
	sqlite3_bind_int(stmt, 4, match_id);
	const bool success = sqlite3_step(stmt) == SQLITE_DONE;
	sqlite3_finalize(stmt);
	log_match_store(success ? "match_opened_ok" : "match_opened_failed", "tournament_id=" + std::to_string(tournament_id) + " match_id=" + std::to_string(match_id));
	return success;
}

bool tournament_bracket::mark_checked_in(int tournament_id, int match_id, const std::string& discord_id) {
	auto match = get_match(tournament_id, match_id);
	if (!match || discord_id.empty()) return false;
	if (is_completed(match->state) || match->no_show_resolved) return false;

	const char* column = nullptr;
	if (discord_id == match->player_a_id) column = "player_a_checked_in";
	if (discord_id == match->player_b_id) column = "player_b_checked_in";
	if (!column) return false;

	std::string sql = std::string("UPDATE tournament_matches SET ") + column + " = 1, state = 'ongoing' WHERE tournament_id = ? AND id = ?;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(get_db().get_handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
	sqlite3_bind_int(stmt, 1, tournament_id);
	sqlite3_bind_int(stmt, 2, match_id);
	const bool success = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().get_handle()) > 0;
	sqlite3_finalize(stmt);
	log_match_store(success ? "match_checkin_ok" : "match_checkin_failed", "tournament_id=" + std::to_string(tournament_id) + " match_id=" + std::to_string(match_id) + " discord_id=" + discord_id);
	return success;
}

bool tournament_bracket::forfeit_player(
	int tournament_id,
	int match_id,
	const std::string& discord_id,
	const std::string& reason
) {
	auto match = get_match(tournament_id, match_id);
	if (!match) return false;
	const bool ok = forfeit_match_player(
		*match,
		discord_id,
		reason.empty() ? "forfeit" : reason,
		tournament_registration::ParticipantStatus::Dropped
	);
	log_match_store(ok ? "forfeit_ok" : "forfeit_failed", "tournament_id=" + std::to_string(tournament_id) + " match_id=" + std::to_string(match_id) + " discord_id=" + discord_id + " reason=" + reason);
	return ok;
}

int tournament_bracket::resolve_due_no_shows(int tournament_id, int now) {
	log_match_store("resolve_no_shows_start", "tournament_id=" + std::to_string(tournament_id) + " now=" + std::to_string(now));
	if (tournament_id <= 0 || now <= 0 || !init()) {
		return 0;
	}

	int resolved = 0;
	for (const auto& match : list_current_matches(tournament_id)) {
		if (!supports_due_no_show_resolution(match.bracket)) {
			continue;
		}

		if (!match.pending_auto_dq_player_id.empty()
			&& (match.pending_auto_dq_player_id == match.player_a_id
				|| match.pending_auto_dq_player_id == match.player_b_id)
			&& forfeit_match_player(
				match,
				match.pending_auto_dq_player_id,
				"auto_dq_after_both_absent",
				tournament_registration::ParticipantStatus::NoShow
			)) {
			++resolved;
		}
	}

	for (const auto& match : list_current_matches(tournament_id)) {
		if (!supports_due_no_show_resolution(match.bracket)
			|| is_completed(match.state)
			|| match.no_show_resolved
			|| match.match_opened_at <= 0
			|| now <= match.match_opened_at + match.grace_time
			|| match.player_a_id.empty()
			|| match.player_b_id.empty()) {
			continue;
		}

		if (match.player_a_checked_in && !match.player_b_checked_in) {
			if (forfeit_match_player(
				match,
				match.player_b_id,
				"match_no_show",
				tournament_registration::ParticipantStatus::NoShow
			)) {
				++resolved;
			}
			continue;
		}

		if (match.player_b_checked_in && !match.player_a_checked_in) {
			if (forfeit_match_player(
				match,
				match.player_a_id,
				"match_no_show",
				tournament_registration::ParticipantStatus::NoShow
			)) {
				++resolved;
			}
			continue;
		}

		if (!match.player_a_checked_in && !match.player_b_checked_in) {
			const int seed_a = participant_seed(tournament_id, match.player_a_id);
			const int seed_b = participant_seed(tournament_id, match.player_b_id);
			const std::string upper_seed_player = seed_a <= seed_b ? match.player_a_id : match.player_b_id;
			const std::string lower_seed_player = upper_seed_player == match.player_a_id ? match.player_b_id : match.player_a_id;

			if (mark_pending_auto_dq(tournament_id, match.next_winner_match, upper_seed_player)
				&& forfeit_match_player(
					match,
					lower_seed_player,
					"both_absent_lower_seed_eliminated",
					tournament_registration::ParticipantStatus::NoShow
				)) {
				tournament_registration::set_participant_status(
					tournament_id,
					upper_seed_player,
					tournament_registration::ParticipantStatus::NoShow
				);
				++resolved;
			}
		}
	}

	log_match_store("resolve_no_shows_done", "tournament_id=" + std::to_string(tournament_id) + " resolved=" + std::to_string(resolved));
	return resolved;
}

bool tournament_bracket::report_match(int tournament_id, int match_id, int score_a, int score_b) {
	log_match_store("report_start", "tournament_id=" + std::to_string(tournament_id) + " match_id=" + std::to_string(match_id) + " score_a=" + std::to_string(score_a) + " score_b=" + std::to_string(score_b));
	auto match = get_match(tournament_id, match_id);
	if (!match
		|| match->player_a_id.empty()
		|| match->player_b_id.empty()
		|| !can_report_state(match->state)
		|| is_bye_bracket(match->bracket)
		|| score_a < 0
		|| score_b < 0
		|| score_a == score_b) {
		log_match_store_error("report_rejected", "tournament_id=" + std::to_string(tournament_id) + " match_id=" + std::to_string(match_id));
		return false;
	}

	const std::string winner = score_a > score_b ? match->player_a_id : match->player_b_id;
	DatabaseTransaction transaction(get_db());
	if (!transaction.ok()) return false;

	sqlite3_stmt* stmt = nullptr;
	const char* report_sql =
		"UPDATE tournament_matches SET score_a = ?, score_b = ?, winner_id = ?, state = 'completed' "
		"WHERE tournament_id = ? AND id = ?;";
	if (sqlite3_prepare_v2(get_db().get_handle(), report_sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}
	sqlite3_bind_int(stmt, 1, score_a);
	sqlite3_bind_int(stmt, 2, score_b);
	bind_text(stmt, 3, winner);
	sqlite3_bind_int(stmt, 4, tournament_id);
	sqlite3_bind_int(stmt, 5, match_id);
	const bool reported = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().get_handle()) > 0;
	sqlite3_finalize(stmt);

	if (!reported) {
		return false;
	}

	const bool loser_side_won_grand_final =
		match->bracket == "grand_finals"
		&& match->position == 0
		&& score_b > score_a
		&& match->next_winner_match < 0;

	if (supports_routing(match->bracket)) {
		const std::string loser = score_a > score_b ? match->player_b_id : match->player_a_id;
		if (loser_side_won_grand_final) {
			if (!place_player(tournament_id, match->bracket_match_index + 1, 0, match->player_a_id)
				|| !place_player(tournament_id, match->bracket_match_index + 1, 1, match->player_b_id)) {
				return false;
			}
		}
		else {
			if (!place_player(tournament_id, match->next_winner_match, match->next_winner_slot, winner)
				|| !place_player(tournament_id, match->next_loser_match, match->next_loser_slot, loser)) {
				return false;
			}
		}
	}

	const bool ok = transaction.commit();
	log_match_store(ok ? "report_ok" : "report_failed", "tournament_id=" + std::to_string(tournament_id) + " match_id=" + std::to_string(match_id) + " winner_id=" + winner);
	return ok;
}

bool tournament_bracket::correct_match_report(int tournament_id, int match_id, int score_a, int score_b) {
	log_match_store("correct_report_start", "tournament_id=" + std::to_string(tournament_id) + " match_id=" + std::to_string(match_id));
	auto match = get_match(tournament_id, match_id);
	if (!match
		|| !is_completed(match->state)
		|| is_bye_bracket(match->bracket)
		|| score_a < 0
		|| score_b < 0
		|| score_a == score_b) {
		return false;
	}

	if (correction_locked_by_later_swiss_round(tournament_id, *match)) {
		return false;
	}

	const bool grand_final_reset_destination =
		match->bracket == "grand_finals"
		&& match->position == 0
		&& match->next_winner_match < 0;

	if (supports_routing(match->bracket)
		&& (destination_completed(tournament_id, match->next_winner_match)
			|| destination_completed(tournament_id, match->next_loser_match)
			|| (grand_final_reset_destination && destination_completed(tournament_id, match->bracket_match_index + 1)))) {
		return false;
	}

	DatabaseTransaction transaction(get_db());
	if (!transaction.ok()) return false;

	if (supports_routing(match->bracket)) {
		if (!clear_destination_slot(tournament_id, match->next_winner_match, match->next_winner_slot)
			|| !clear_destination_slot(tournament_id, match->next_loser_match, match->next_loser_slot)
			|| (grand_final_reset_destination && !clear_destination_slot(tournament_id, match->bracket_match_index + 1, 0))
			|| (grand_final_reset_destination && !clear_destination_slot(tournament_id, match->bracket_match_index + 1, 1))) {
			return false;
		}
	}

	sqlite3_stmt* stmt = nullptr;
	const char* reset_sql =
		"UPDATE tournament_matches "
		"SET score_a = 0, score_b = 0, winner_id = '', state = 'ready' "
		"WHERE tournament_id = ? AND id = ?;";

	if (sqlite3_prepare_v2(get_db().get_handle(), reset_sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	sqlite3_bind_int(stmt, 1, tournament_id);
	sqlite3_bind_int(stmt, 2, match_id);
	const bool reset = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().get_handle()) > 0;
	sqlite3_finalize(stmt);

	if (!reset) {
		return false;
	}

	if (!transaction.commit()) {
		return false;
	}

	const bool ok = report_match(tournament_id, match_id, score_a, score_b);
	log_match_store(ok ? "correct_report_ok" : "correct_report_failed", "tournament_id=" + std::to_string(tournament_id) + " match_id=" + std::to_string(match_id));
	return ok;
}

std::string tournament_bracket::state_to_string(StoredMatchState state) {
	switch (state) {
	case StoredMatchState::Ready: return "ready";
	case StoredMatchState::Ongoing: return "ongoing";
	case StoredMatchState::Completed: return "completed";
	case StoredMatchState::Pending: return "pending";
	}
	return "pending";
}

tournament_bracket::StoredMatchState tournament_bracket::state_from_string(const std::string& state) {
	if (state == "ready") return StoredMatchState::Ready;
	if (state == "ongoing") return StoredMatchState::Ongoing;
	if (state == "completed") return StoredMatchState::Completed;
	return StoredMatchState::Pending;
}

std::string tournament_bracket::player_mention(const std::string& discord_id) {
	return discord_id.empty() ? "TBD" : "<@" + discord_id + ">";
}

std::string tournament_bracket::describe_match(const StoredMatch& match) {
	std::ostringstream out;
	out << "M" << match.id << " " << match.bracket << " R" << (match.round + 1) << "." << (match.position + 1)
		<< ": " << player_mention(match.player_a_id)
		<< " vs " << player_mention(match.player_b_id)
		<< " [" << state_to_string(match.state) << "]";
	if (match.streamed) out << " [stream]";
	if (match.thread_id) out << " <#" << match.thread_id << ">";
	return out.str();
}
