#include "core/SqlReliabilityCheck.hpp"
#include "core/sqlite.hpp"
#include "misc/sqlite-user.hpp"
#include "tournament/bracket/MatchStore.hpp"
#include "tournament/manage.hpp"
#include "tournament/registration.hpp"
#include "tournament/ruleset.hpp"
#include "tournament/seeding.hpp"
#include <filesystem>
#include <iostream>
#include <set>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace {
	struct CheckContext {
		int passed = 0;
		int failed = 0;

		bool require(bool condition, const std::string& label) {
			if (condition) {
				++passed;
				std::cout << "[PASS] " << label << '\n';
				return true;
			}

			++failed;
			std::cout << "[FAIL] " << label << '\n';
			return false;
		}
	};

	std::string db_sidecar_path(const std::string& db_path, const std::string& suffix) {
		return db_path + suffix;
	}

	bool remove_if_exists(const std::filesystem::path& path) {
		std::error_code ec;
		if (!std::filesystem::exists(path, ec)) {
			return true;
		}

		return std::filesystem::remove(path, ec);
	}

	bool reset_throwaway_db(const std::string& db_path) {
		const std::filesystem::path path(db_path);
		const std::string filename = path.filename().string();
		if (filename.find("sql_reliability_check") == std::string::npos) {
			std::cerr << "Refusing to reset non-check database path: " << db_path << '\n';
			return false;
		}

		return remove_if_exists(path)
			&& remove_if_exists(db_sidecar_path(db_path, "-wal"))
			&& remove_if_exists(db_sidecar_path(db_path, "-shm"))
			&& remove_if_exists(db_sidecar_path(db_path, ".bak"));
	}

	std::set<std::string> table_columns(sqlite3* db, const std::string& table) {
		std::set<std::string> columns;
		sqlite3_stmt* stmt = nullptr;
		const std::string sql = "PRAGMA table_info(" + table + ");";
		if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			return columns;
		}

		while (sqlite3_step(stmt) == SQLITE_ROW) {
			const unsigned char* name = sqlite3_column_text(stmt, 1);
			if (name) {
				columns.insert(reinterpret_cast<const char*>(name));
			}
		}

		sqlite3_finalize(stmt);
		return columns;
	}

	bool has_columns(sqlite3* db, const std::string& table, const std::vector<std::string>& expected) {
		const std::set<std::string> columns = table_columns(db, table);
		for (const std::string& column : expected) {
			if (!columns.contains(column)) {
				std::cout << "       missing column: " << table << "." << column << '\n';
				return false;
			}
		}

		return true;
	}

	bool index_exists(sqlite3* db, const std::string& index_name) {
		sqlite3_stmt* stmt = nullptr;
		const char* sql =
			"SELECT 1 FROM sqlite_master "
			"WHERE type = 'index' AND name = ? LIMIT 1;";
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
			return false;
		}

		sqlite3_bind_text(stmt, 1, index_name.c_str(), -1, SQLITE_TRANSIENT);
		const bool found = sqlite3_step(stmt) == SQLITE_ROW;
		sqlite3_finalize(stmt);
		return found;
	}

	bool scalar_text(sqlite3* db, const std::string& sql, std::string& out) {
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			return false;
		}

		bool ok = false;
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			const unsigned char* value = sqlite3_column_text(stmt, 0);
			out = value ? reinterpret_cast<const char*>(value) : "";
			ok = true;
		}

		sqlite3_finalize(stmt);
		return ok;
	}

	bool scalar_int(sqlite3* db, const std::string& sql, int& out) {
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			return false;
		}

		bool ok = false;
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			out = sqlite3_column_int(stmt, 0);
			ok = true;
		}

		sqlite3_finalize(stmt);
		return ok;
	}

	bool backup_database(const std::string& source_path, const std::string& backup_path) {
		sqlite3* source = nullptr;
		sqlite3* destination = nullptr;
		if (sqlite3_open(source_path.c_str(), &source) != SQLITE_OK) {
			if (source) sqlite3_close(source);
			return false;
		}

		if (sqlite3_open(backup_path.c_str(), &destination) != SQLITE_OK) {
			sqlite3_close(source);
			if (destination) sqlite3_close(destination);
			return false;
		}

		sqlite3_backup* backup = sqlite3_backup_init(destination, "main", source, "main");
		if (!backup) {
			sqlite3_close(destination);
			sqlite3_close(source);
			return false;
		}

		const int step = sqlite3_backup_step(backup, -1);
		const int finish = sqlite3_backup_finish(backup);
		const bool ok = (step == SQLITE_DONE || step == SQLITE_OK) && finish == SQLITE_OK;
		sqlite3_close(destination);
		sqlite3_close(source);
		return ok;
	}

	bool run_init_pass() {
		return GuildConfigManager::init()
			&& ServerSettingsManager::init()
			&& UserSettingsManager::init()
			&& ModerationManager::init()
			&& tournament_manage::init()
			&& tournament_registration::init()
			&& tournament_ruleset::init()
			&& tournament_bracket::init();
	}

	bool run_representative_flow(CheckContext& ctx) {
		const auto single_id = tournament_manage::create_tournament(
			"SQL reliability single elimination",
			"tetrio",
			"single_elimination"
		);
		if (!ctx.require(single_id.has_value(), "create single-elimination tournament")) {
			return false;
		}

		ctx.require(tournament_manage::set_registration_open(*single_id, true), "open registration flag");
		ctx.require(tournament_manage::set_checkin_open(*single_id, true, 2'000'000'000, 600), "open check-in flag");

		for (int i = 1; i <= 4; ++i) {
			tournament_registration::RegistrationRequest request;
			request.tournament_id = *single_id;
			request.discord_id = std::to_string(9000 + i);
			request.display_name = "Player " + std::to_string(i);
			request.provided_username = "player" + std::to_string(i);
			request.registered_at = 100 + i;
			ctx.require(tournament_registration::register_player(request).ok, "register player " + std::to_string(i));

			tournament_registration::CheckInRequest check_in;
			check_in.tournament_id = *single_id;
			check_in.discord_id = request.discord_id;
			check_in.provided_username = request.provided_username;
			check_in.now = 1'900'000'000;
			check_in.checkin_closes_at = 2'000'000'000;
			ctx.require(tournament_registration::check_in_player(check_in).ok, "check in player " + std::to_string(i));
			ctx.require(tournament_registration::set_participant_seed(*single_id, request.discord_id, i), "set seed " + std::to_string(i));
		}

		ctx.require(tournament_bracket::generate_single_elimination(*single_id), "generate single-elimination bracket");
		const auto matches = tournament_bracket::list_current_matches(*single_id);
		if (!ctx.require(!matches.empty(), "list current matches after generation")) {
			return false;
		}

		const int match_id = matches.front().id;
		ctx.require(tournament_bracket::mark_match_opened(*single_id, match_id, 1'900'000'100, 600), "mark match opened");
		ctx.require(tournament_bracket::mark_checked_in(*single_id, match_id, matches.front().player_a_id), "mark player A match check-in");
		ctx.require(tournament_bracket::assign_streamed(*single_id, match_id, true), "assign stream flag");
		ctx.require(tournament_bracket::set_discord_thread(*single_id, match_id, 12345, 67890), "store Discord thread/message IDs");
		ctx.require(tournament_bracket::report_match(*single_id, match_id, 2, 0), "report match score");
		ctx.require(tournament_bracket::correct_match_report(*single_id, match_id, 2, 1), "correct reported match score");

		const auto rating_id = tournament_manage::create_tournament("SQL reliability TE:C rating", "tec", "single_elimination");
		if (!ctx.require(rating_id.has_value(), "create rating tournament")) {
			return false;
		}

		for (int i = 1; i <= 3; ++i) {
			tournament_registration::RegistrationRequest request;
			request.tournament_id = *rating_id;
			request.discord_id = std::to_string(9400 + i);
			request.display_name = "Rating " + std::to_string(i);
			request.provided_username = "rating" + std::to_string(i);
			request.registered_at = 400 + i;
			ctx.require(tournament_registration::register_player(request).ok, "register rating player " + std::to_string(i));

			tournament_registration::CheckInRequest check_in;
			check_in.tournament_id = *rating_id;
			check_in.discord_id = request.discord_id;
			check_in.provided_username = request.provided_username;
			check_in.now = 1'900'000'000;
			check_in.checkin_closes_at = 2'000'000'000;
			ctx.require(tournament_registration::check_in_player(check_in).ok, "check in rating player " + std::to_string(i));
		}

		ctx.require(tournament_registration::set_participant_rating(
			*rating_id,
			"9401",
			"tec_connected_vs",
			1200.0,
			"staff",
			"1",
			1'900'000'000
		), "set rating player 1 points");
		ctx.require(tournament_registration::set_participant_rating(
			*rating_id,
			"9402",
			"tec_connected_vs",
			1800.0,
			"staff",
			"1",
			1'900'000'001
		), "set rating player 2 points");
		ctx.require(tournament_registration::get_participant_rating(*rating_id, "9402", "tec_connected_vs").has_value(), "read rating points");
		ctx.require(!tournament_registration::list_ratings(*rating_id, "tec_connected_vs").empty(), "list rating points");

		auto rating_seed = tournament_seeding::seed_by_rating(
			tournament_registration::list_checked_in_participants(*rating_id),
			"tec_connected_vs"
		);
		ctx.require(rating_seed.seeded.size() == 2 && rating_seed.excluded.size() == 1, "rating seeding excludes missing points");
		ctx.require(!rating_seed.seeded.empty() && rating_seed.seeded.front().discord_id == "9402", "rating seeding sorts highest points first");
		ctx.require(tournament_registration::clear_participant_rating(*rating_id, "9401", "tec_connected_vs"), "clear rating points");

		const auto forfeit_id = tournament_manage::create_tournament("SQL reliability forfeit", "tetrio", "single_elimination");
		if (!ctx.require(forfeit_id.has_value(), "create forfeit tournament")) {
			return false;
		}

		for (int i = 1; i <= 2; ++i) {
			tournament_registration::RegistrationRequest request;
			request.tournament_id = *forfeit_id;
			request.discord_id = std::to_string(9100 + i);
			request.display_name = "Forfeit " + std::to_string(i);
			request.provided_username = "forfeit" + std::to_string(i);
			request.registered_at = 200 + i;
			tournament_registration::register_player(request);

			tournament_registration::CheckInRequest check_in;
			check_in.tournament_id = *forfeit_id;
			check_in.discord_id = request.discord_id;
			check_in.provided_username = request.provided_username;
			check_in.now = 1'900'000'000;
			check_in.checkin_closes_at = 2'000'000'000;
			tournament_registration::check_in_player(check_in);
			tournament_registration::set_participant_seed(*forfeit_id, request.discord_id, i);
		}

		ctx.require(tournament_bracket::generate_single_elimination(*forfeit_id), "generate forfeit bracket");
		const auto forfeit_matches = tournament_bracket::list_current_matches(*forfeit_id);
		if (!ctx.require(!forfeit_matches.empty(), "list forfeit match")) {
			return false;
		}
		ctx.require(tournament_bracket::forfeit_player(
			*forfeit_id,
			forfeit_matches.front().id,
			forfeit_matches.front().player_b_id,
			"sql_reliability_check"
		), "record staff forfeit");

		const auto round_robin_id = tournament_manage::create_tournament(
			"SQL reliability round robin",
			"tetrio",
			"round_robin"
		);
		if (!ctx.require(round_robin_id.has_value(), "create round-robin tournament")) {
			return false;
		}

		for (int i = 1; i <= 3; ++i) {
			tournament_registration::RegistrationRequest request;
			request.tournament_id = *round_robin_id;
			request.discord_id = std::to_string(9200 + i);
			request.display_name = "Round " + std::to_string(i);
			request.provided_username = "round" + std::to_string(i);
			tournament_registration::register_player(request);

			tournament_registration::CheckInRequest check_in;
			check_in.tournament_id = *round_robin_id;
			check_in.discord_id = request.discord_id;
			check_in.provided_username = request.provided_username;
			check_in.now = 1'900'000'000;
			tournament_registration::check_in_player(check_in);
			tournament_registration::set_participant_seed(*round_robin_id, request.discord_id, i);
		}

		ctx.require(tournament_bracket::generate_round_robin(*round_robin_id), "generate round-robin matches");
		const auto round_robin_matches = tournament_bracket::list_matches(*round_robin_id);
		if (!ctx.require(!round_robin_matches.empty(), "list generated round-robin match")) {
			return false;
		}
		ctx.require(tournament_bracket::report_match(*round_robin_id, round_robin_matches.front().id, 2, 0), "report round-robin match");
		ctx.require(!tournament_bracket::list_format_standings(*round_robin_id).empty(), "list round-robin standings");

		const auto swiss_id = tournament_manage::create_tournament("SQL reliability Swiss", "tetrio", "swiss");
		if (!ctx.require(swiss_id.has_value(), "create Swiss tournament")) {
			return false;
		}

		for (int i = 1; i <= 4; ++i) {
			tournament_registration::RegistrationRequest request;
			request.tournament_id = *swiss_id;
			request.discord_id = std::to_string(9300 + i);
			request.display_name = "Swiss " + std::to_string(i);
			request.provided_username = "swiss" + std::to_string(i);
			tournament_registration::register_player(request);

			tournament_registration::CheckInRequest check_in;
			check_in.tournament_id = *swiss_id;
			check_in.discord_id = request.discord_id;
			check_in.provided_username = request.provided_username;
			check_in.now = 1'900'000'000;
			tournament_registration::check_in_player(check_in);
			tournament_registration::set_participant_seed(*swiss_id, request.discord_id, i);
		}

		ctx.require(tournament_bracket::generate_swiss_round(*swiss_id), "generate Swiss round");
		const auto swiss_matches = tournament_bracket::list_matches(*swiss_id);
		if (!ctx.require(!swiss_matches.empty(), "list generated Swiss match")) {
			return false;
		}
		ctx.require(tournament_bracket::report_match(*swiss_id, swiss_matches.front().id, 2, 0), "report Swiss match");
		ctx.require(!tournament_bracket::list_format_standings(*swiss_id).empty(), "list Swiss standings");
		return true;
	}

	bool run_rollback_probe(CheckContext& ctx, Database& db) {
		if (!ctx.require(db.execute(
			"CREATE TABLE IF NOT EXISTS sql_reliability_probe ("
			"id INTEGER PRIMARY KEY, value TEXT"
			");"
		), "create rollback probe table")) {
			return false;
		}

		{
			DatabaseTransaction transaction(db);
			if (!ctx.require(transaction.ok(), "start rollback probe transaction")) {
				return false;
			}

			if (!ctx.require(db.execute("INSERT INTO sql_reliability_probe (id, value) VALUES (1, 'rollback');"), "insert rollback probe row")) {
				return false;
			}
		}

		int count = -1;
		return ctx.require(
			scalar_int(db.get_handle(), "SELECT COUNT(*) FROM sql_reliability_probe WHERE id = 1;", count) && count == 0,
			"uncommitted transaction rolled back"
		);
	}

	bool run_user_db_isolation_probe(CheckContext& ctx, const std::string& db_path) {
		const std::string user_db_path = db_path + ".user";
		if (!ctx.require(reset_throwaway_db(user_db_path), "reset isolated user database")) {
			return false;
		}

		misc_user_sqlite::UserDatabase user_db(user_db_path);
		if (!ctx.require(user_db.ok(), "open isolated user database")) {
			return false;
		}

		int table_count = -1;
		ctx.require(
			scalar_int(
				user_db.get_handle(),
				"SELECT COUNT(*) FROM sqlite_master WHERE type = 'table';",
				table_count
			) && table_count == 0,
			"isolated user database starts with no tables"
		);

		ctx.require(user_db.execute(
			"CREATE TABLE misc_extension_probe ("
			"id INTEGER PRIMARY KEY, "
			"value TEXT NOT NULL"
			");"
		), "create isolated user extension table");
		ctx.require(user_db.add_column_if_missing("misc_extension_probe", "note TEXT DEFAULT ''"), "migrate isolated user extension table");
		ctx.require(user_db.create_index_if_missing(
			"idx_misc_extension_probe_value",
			"misc_extension_probe",
			"value"
		), "create isolated user extension index");

		{
			misc_user_sqlite::UserDatabaseTransaction transaction(user_db);
			if (!ctx.require(transaction.ok(), "start isolated user database transaction")) {
				return false;
			}

			ctx.require(user_db.execute("INSERT INTO misc_extension_probe (id, value) VALUES (1, 'rollback');"), "insert isolated rollback probe row");
		}

		int rollback_count = -1;
		ctx.require(
			scalar_int(
				user_db.get_handle(),
				"SELECT COUNT(*) FROM misc_extension_probe WHERE id = 1;",
				rollback_count
			) && rollback_count == 0,
			"isolated user database transaction rolls back"
		);

		std::string quick_check;
		return ctx.require(
			scalar_text(user_db.get_handle(), "PRAGMA quick_check;", quick_check) && quick_check == "ok",
			"isolated user database quick_check is ok"
		);
	}
}

int sql_reliability::run(const std::string& db_path) {
	CheckContext ctx;
	std::cout << "SQL reliability check DB: " << db_path << '\n';

	if (!reset_throwaway_db(db_path)) {
		return 1;
	}

	if (!ctx.require(run_init_pass(), "initial schema/migration pass")) {
		return 1;
	}

	if (!ctx.require(run_init_pass(), "second schema/migration pass is idempotent")) {
		return 1;
	}

	Database audit_db("db/master.db");
	sqlite3* db = audit_db.get_handle();
	if (!ctx.require(db != nullptr, "open audit connection")) {
		return 1;
	}

	ctx.require(has_columns(db, "player_links", {
		"discord_id", "tetrio_id", "jstris_id", "ppt2_id", "tec_id",
		"tetra_id", "tgm_id", "ctwc_id", "other_id", "last_sync"
	}), "player_links required columns");
	ctx.require(has_columns(db, "server_settings", {
		"guild_id", "owner_id", "admin_role_id", "moderator_role_id",
		"staff_role_id", "language", "secondary_language", "modlog_channel_id"
	}), "server_settings required columns");
	ctx.require(has_columns(db, "user_settings", { "user_id", "language" }), "user_settings required columns");
	ctx.require(has_columns(db, "guild_config", {
		"guild_id", "tournament_staff_role_id", "tournament_admin_role_id",
		"tournament_channel_id", "tournament_log_channel_id"
	}), "guild_config required columns");
	ctx.require(has_columns(db, "tournaments", {
		"id", "name", "game_type", "format", "status", "registration_open",
		"checkin_open", "checkin_closes_at", "checkin_grace_time",
		"tetrio_current_rank_min", "tetrio_current_rank_max",
		"tetrio_top_rank_min", "tetrio_top_rank_max",
		"tetrio_tr_min", "tetrio_tr_max", "tetrio_allow_unranked"
	}), "tournaments required columns");
	ctx.require(has_columns(db, "tournament_participants", {
		"tournament_id", "discord_id", "display_name", "provided_username",
		"tetrio_id", "status", "seed", "registered_at", "checked_in_at"
	}), "tournament_participants required columns");
	ctx.require(has_columns(db, "tournament_participant_ratings", {
		"tournament_id", "discord_id", "rating_bucket", "rating_points",
		"source", "updated_by", "updated_at", "note"
	}), "tournament_participant_ratings required columns");
	ctx.require(has_columns(db, "tournament_matches", {
		"id", "tournament_id", "bracket_match_index", "round", "position",
		"bracket", "player_a_id", "player_b_id", "winner_id", "score_a",
		"score_b", "state", "streamed", "thread_id", "message_id",
		"player_a_checked_in", "player_b_checked_in", "match_opened_at",
		"grace_time", "no_show_resolved", "no_show_reason",
		"pending_auto_dq_player_id", "next_winner_match", "next_winner_slot",
		"next_loser_match", "next_loser_slot"
	}), "tournament_matches required columns");
	ctx.require(has_columns(db, "tournament_rulesets", {
		"tournament_id", "scope", "secondary_trigger", "win_score",
		"win_diff", "score_cap", "deuce_mode", "allow_draw"
	}), "tournament_rulesets required columns");
	ctx.require(has_columns(db, "moderation_cases", {
		"id", "guild_id", "target_id", "actor_id", "action", "reason",
		"duration_seconds", "created_at"
	}), "moderation_cases required columns");

	ctx.require(index_exists(db, "idx_tournaments_status"), "idx_tournaments_status exists");
	ctx.require(index_exists(db, "idx_tournaments_format"), "idx_tournaments_format exists");
	ctx.require(index_exists(db, "idx_tournament_participants_tournament_status"), "participant status index exists");
	ctx.require(index_exists(db, "idx_tournament_participant_ratings_bucket"), "participant ratings bucket index exists");
	ctx.require(index_exists(db, "idx_tournament_matches_tournament_state"), "match state index exists");
	ctx.require(index_exists(db, "idx_tournament_matches_tournament_round"), "match round index exists");

	ctx.require(ServerSettingsManager::set_language(1001, "KO-kr"), "set server language");
	ctx.require(ServerSettingsManager::set_secondary_language(1001, "KO-kr"), "set secondary language");
	ctx.require(ServerSettingsManager::clear_secondary_language(1001), "clear secondary language");
	ctx.require(UserSettingsManager::set_language(2002, "EN-gb"), "set user language");
	ctx.require(UserSettingsManager::clear_language(2002), "clear user language");
	ctx.require(GuildConfigManager::set_tournament_channel(1001, 3003), "set tournament channel");
	ctx.require(GuildConfigManager::set_tournament_log_channel(1001, 3004), "set tournament log channel");
	ctx.require(ModerationManager::create_case(1001, 2002, 3003, "warn", "sql reliability check").has_value(), "create moderation case");
	ctx.require(!ModerationManager::list_cases(1001, 2002, 1).empty(), "read moderation case reason");
	if (ModerationManager::encryption_enabled()) {
		std::string stored_reason;
		ctx.require(
			scalar_text(
				db,
				"SELECT reason FROM moderation_cases WHERE guild_id = 1001 AND target_id = 2002 ORDER BY id DESC LIMIT 1;",
				stored_reason
			) && stored_reason.rfind("enc:v1:", 0) == 0,
			"moderation reason stored encrypted"
		);
	}

	run_representative_flow(ctx);
	run_rollback_probe(ctx, audit_db);
	run_user_db_isolation_probe(ctx, db_path);

	std::string quick_check;
	ctx.require(
		scalar_text(db, "PRAGMA quick_check;", quick_check) && quick_check == "ok",
		"PRAGMA quick_check is ok"
	);

	int foreign_key_violations = -1;
	ctx.require(
		scalar_int(db, "SELECT COUNT(*) FROM pragma_foreign_key_check;", foreign_key_violations)
			&& foreign_key_violations == 0,
		"PRAGMA foreign_key_check has no violations"
	);

	const std::string backup_path = db_path + ".bak";
	ctx.require(backup_database(db_path, backup_path), "sqlite backup API succeeds");

	std::string backup_quick_check;
	sqlite3* backup = nullptr;
	bool backup_ok = false;
	if (sqlite3_open(backup_path.c_str(), &backup) == SQLITE_OK) {
		backup_ok = scalar_text(backup, "PRAGMA quick_check;", backup_quick_check)
			&& backup_quick_check == "ok";
	}
	if (backup) {
		sqlite3_close(backup);
	}
	ctx.require(backup_ok, "backup database quick_check is ok");

	std::cout << "SQL reliability check complete: " << ctx.passed << " passed, " << ctx.failed << " failed." << '\n';
	return ctx.failed == 0 ? 0 : 1;
}
