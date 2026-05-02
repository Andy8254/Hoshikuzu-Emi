#include "tournament/bracket/MatchStore.hpp"
#include "tournament/bracket/Bracket.hpp"
#include "tournament/registration.hpp"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <sqlite3.h>

namespace {
	class MatchDatabase {
	public:
		MatchDatabase() {
			std::filesystem::path path("db/master.db");
			if (path.has_parent_path() && !std::filesystem::exists(path.parent_path())) {
				std::filesystem::create_directories(path.parent_path());
			}

			if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) {
				std::cerr << "CRITICAL: SQLITE Open Failed: " << sqlite3_errmsg(db) << std::endl;
				sqlite3_close(db);
				db = nullptr;
				return;
			}

			sqlite3_busy_timeout(db, 5000);
			execute("PRAGMA foreign_keys = ON;");
		}

		~MatchDatabase() {
			if (db) sqlite3_close(db);
		}

		MatchDatabase(const MatchDatabase&) = delete;
		MatchDatabase& operator=(const MatchDatabase&) = delete;

		bool execute(const std::string& sql) {
			if (!db) return false;
			char* error = nullptr;
			const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
			if (rc != SQLITE_OK) {
				std::cerr << "SQL Error: " << (error ? error : "unknown") << std::endl;
				sqlite3_free(error);
				return false;
			}
			return true;
		}

		sqlite3* handle() { return db; }

	private:
		sqlite3* db = nullptr;
	};

	MatchDatabase& get_db() {
		static MatchDatabase instance;
		return instance;
	}

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
		return match;
	}

	bool insert_match(int tournament_id, int bracket_index, const Match& source) {
		sqlite3_stmt* stmt = nullptr;
		const char* sql =
			"INSERT INTO tournament_matches "
			"(tournament_id, bracket_match_index, round, position, bracket, player_a_id, player_b_id, winner_id, score_a, score_b, state) "
			"VALUES (?, ?, ?, ?, 'winners', ?, ?, ?, ?, ?, ?);";

		if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
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
			&& bind_text(stmt, 5, source.playerA_id)
			&& bind_text(stmt, 6, source.playerB_id)
			&& bind_text(stmt, 7, source.winner_id)
			&& sqlite3_bind_int(stmt, 8, source.scoreA) == SQLITE_OK
			&& sqlite3_bind_int(stmt, 9, source.scoreB) == SQLITE_OK
			&& bind_text(stmt, 10, tournament_bracket::state_to_string(state));

		const bool success = bound && sqlite3_step(stmt) == SQLITE_DONE;
		sqlite3_finalize(stmt);
		return success;
	}

	std::vector<tournament_bracket::StoredMatch> query_matches(const char* sql, int tournament_id, int extra = 0) {
		std::vector<tournament_bracket::StoredMatch> result;
		if (!tournament_bracket::init()) return result;

		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
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
		"UNIQUE(tournament_id, bracket_match_index),"
		"FOREIGN KEY (tournament_id) REFERENCES tournaments(id) ON DELETE CASCADE"
		");";

	return get_db().execute(sql);
}

bool tournament_bracket::clear_matches(int tournament_id) {
	if (tournament_id <= 0 || !init()) return false;
	sqlite3_stmt* stmt = nullptr;
	const char* sql = "DELETE FROM tournament_matches WHERE tournament_id = ?;";
	if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
	sqlite3_bind_int(stmt, 1, tournament_id);
	const bool success = sqlite3_step(stmt) == SQLITE_DONE;
	sqlite3_finalize(stmt);
	return success;
}

bool tournament_bracket::generate_single_elimination(int tournament_id) {
	if (tournament_id <= 0 || !init()) return false;
	const auto participants = tournament_registration::list_checked_in_participants(tournament_id);
	if (participants.size() < 2) return false;

	std::vector<std::string> seeded_players;
	for (const auto& participant : participants) {
		seeded_players.push_back(participant.discord_id);
	}

	Bracket bracket;
	bracket.generate_single_elimination(seeded_players);
	if (bracket.matches.empty()) return false;

	if (!clear_matches(tournament_id)) return false;

	get_db().execute("BEGIN TRANSACTION;");
	for (int i = 0; i < static_cast<int>(bracket.matches.size()); ++i) {
		if (!insert_match(tournament_id, i, bracket.matches[i])) {
			get_db().execute("ROLLBACK;");
			return false;
		}
	}
	return get_db().execute("COMMIT;");
}

std::optional<tournament_bracket::StoredMatch> tournament_bracket::get_match(int tournament_id, int match_id) {
	if (tournament_id <= 0 || match_id <= 0 || !init()) return std::nullopt;
	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"SELECT id, tournament_id, bracket_match_index, round, position, bracket, player_a_id, player_b_id, winner_id, score_a, score_b, state, streamed, thread_id, message_id "
		"FROM tournament_matches WHERE tournament_id = ? AND id = ? LIMIT 1;";
	if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;
	sqlite3_bind_int(stmt, 1, tournament_id);
	sqlite3_bind_int(stmt, 2, match_id);

	std::optional<StoredMatch> result;
	if (sqlite3_step(stmt) == SQLITE_ROW) result = read_match(stmt);
	sqlite3_finalize(stmt);
	return result;
}

std::vector<tournament_bracket::StoredMatch> tournament_bracket::list_matches(int tournament_id) {
	return query_matches(
		"SELECT id, tournament_id, bracket_match_index, round, position, bracket, player_a_id, player_b_id, winner_id, score_a, score_b, state, streamed, thread_id, message_id "
		"FROM tournament_matches WHERE tournament_id = ? ORDER BY round ASC, position ASC;",
		tournament_id
	);
}

std::vector<tournament_bracket::StoredMatch> tournament_bracket::list_current_matches(int tournament_id) {
	return query_matches(
		"SELECT id, tournament_id, bracket_match_index, round, position, bracket, player_a_id, player_b_id, winner_id, score_a, score_b, state, streamed, thread_id, message_id "
		"FROM tournament_matches WHERE tournament_id = ? AND state IN ('ready', 'ongoing') ORDER BY round ASC, position ASC;",
		tournament_id
	);
}

std::vector<tournament_bracket::StoredMatch> tournament_bracket::list_round_matches(int tournament_id, int round) {
	return query_matches(
		"SELECT id, tournament_id, bracket_match_index, round, position, bracket, player_a_id, player_b_id, winner_id, score_a, score_b, state, streamed, thread_id, message_id "
		"FROM tournament_matches WHERE tournament_id = ? AND round = ? ORDER BY position ASC;",
		tournament_id,
		round
	);
}

std::vector<tournament_bracket::StoredMatch> tournament_bracket::list_streamed_matches(int tournament_id) {
	return query_matches(
		"SELECT id, tournament_id, bracket_match_index, round, position, bracket, player_a_id, player_b_id, winner_id, score_a, score_b, state, streamed, thread_id, message_id "
		"FROM tournament_matches WHERE tournament_id = ? AND streamed = 1 ORDER BY round ASC, position ASC;",
		tournament_id
	);
}

bool tournament_bracket::assign_streamed(int tournament_id, int match_id, bool streamed) {
	if (tournament_id <= 0 || match_id <= 0 || !init()) return false;
	sqlite3_stmt* stmt = nullptr;
	const char* sql = "UPDATE tournament_matches SET streamed = ? WHERE tournament_id = ? AND id = ?;";
	if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
	sqlite3_bind_int(stmt, 1, streamed ? 1 : 0);
	sqlite3_bind_int(stmt, 2, tournament_id);
	sqlite3_bind_int(stmt, 3, match_id);
	const bool success = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().handle()) > 0;
	sqlite3_finalize(stmt);
	return success;
}

bool tournament_bracket::set_discord_thread(int tournament_id, int match_id, dpp::snowflake thread_id, dpp::snowflake message_id) {
	if (tournament_id <= 0 || match_id <= 0 || !init()) return false;
	sqlite3_stmt* stmt = nullptr;
	const char* sql = "UPDATE tournament_matches SET thread_id = ?, message_id = ? WHERE tournament_id = ? AND id = ?;";
	if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
	sqlite3_bind_int64(stmt, 1, thread_id);
	sqlite3_bind_int64(stmt, 2, message_id);
	sqlite3_bind_int(stmt, 3, tournament_id);
	sqlite3_bind_int(stmt, 4, match_id);
	const bool success = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().handle()) > 0;
	sqlite3_finalize(stmt);
	return success;
}

bool tournament_bracket::mark_checked_in(int tournament_id, int match_id, const std::string& discord_id) {
	auto match = get_match(tournament_id, match_id);
	if (!match || discord_id.empty()) return false;

	const char* column = nullptr;
	if (discord_id == match->player_a_id) column = "player_a_checked_in";
	if (discord_id == match->player_b_id) column = "player_b_checked_in";
	if (!column) return false;

	std::string sql = std::string("UPDATE tournament_matches SET ") + column + " = 1, state = 'ongoing' WHERE tournament_id = ? AND id = ?;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(get_db().handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
	sqlite3_bind_int(stmt, 1, tournament_id);
	sqlite3_bind_int(stmt, 2, match_id);
	const bool success = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().handle()) > 0;
	sqlite3_finalize(stmt);
	return success;
}

bool tournament_bracket::report_match(int tournament_id, int match_id, int score_a, int score_b) {
	auto match = get_match(tournament_id, match_id);
	if (!match || match->player_a_id.empty() || match->player_b_id.empty() || score_a == score_b) return false;

	const std::string winner = score_a > score_b ? match->player_a_id : match->player_b_id;
	get_db().execute("BEGIN TRANSACTION;");

	sqlite3_stmt* stmt = nullptr;
	const char* report_sql =
		"UPDATE tournament_matches SET score_a = ?, score_b = ?, winner_id = ?, state = 'completed' "
		"WHERE tournament_id = ? AND id = ?;";
	if (sqlite3_prepare_v2(get_db().handle(), report_sql, -1, &stmt, nullptr) != SQLITE_OK) {
		get_db().execute("ROLLBACK;");
		return false;
	}
	sqlite3_bind_int(stmt, 1, score_a);
	sqlite3_bind_int(stmt, 2, score_b);
	bind_text(stmt, 3, winner);
	sqlite3_bind_int(stmt, 4, tournament_id);
	sqlite3_bind_int(stmt, 5, match_id);
	const bool reported = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().handle()) > 0;
	sqlite3_finalize(stmt);

	if (!reported) {
		get_db().execute("ROLLBACK;");
		return false;
	}

	const int next_round = match->round + 1;
	const int next_position = match->position / 2;
	stmt = nullptr;
	const char* next_sql =
		"UPDATE tournament_matches "
		"SET player_a_id = CASE WHEN ? = 0 THEN ? ELSE player_a_id END, "
		"player_b_id = CASE WHEN ? = 1 THEN ? ELSE player_b_id END, "
		"state = CASE "
		"WHEN (CASE WHEN ? = 0 THEN ? ELSE player_a_id END) <> '' "
		"AND (CASE WHEN ? = 1 THEN ? ELSE player_b_id END) <> '' "
		"AND state = 'pending' THEN 'ready' ELSE state END "
		"WHERE tournament_id = ? AND round = ? AND position = ?;";

	if (sqlite3_prepare_v2(get_db().handle(), next_sql, -1, &stmt, nullptr) == SQLITE_OK) {
		const int side = match->position % 2;
		sqlite3_bind_int(stmt, 1, side);
		bind_text(stmt, 2, winner);
		sqlite3_bind_int(stmt, 3, side);
		bind_text(stmt, 4, winner);
		sqlite3_bind_int(stmt, 5, side);
		bind_text(stmt, 6, winner);
		sqlite3_bind_int(stmt, 7, side);
		bind_text(stmt, 8, winner);
		sqlite3_bind_int(stmt, 9, tournament_id);
		sqlite3_bind_int(stmt, 10, next_round);
		sqlite3_bind_int(stmt, 11, next_position);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	return get_db().execute("COMMIT;");
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
	out << "M" << match.id << " R" << (match.round + 1) << "." << (match.position + 1)
		<< ": " << player_mention(match.player_a_id)
		<< " vs " << player_mention(match.player_b_id)
		<< " [" << state_to_string(match.state) << "]";
	if (match.streamed) out << " [stream]";
	if (match.thread_id) out << " <#" << match.thread_id << ">";
	return out.str();
}
