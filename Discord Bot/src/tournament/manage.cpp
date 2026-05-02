#include "tournament/manage.hpp"
#include <filesystem>
#include <iostream>
#include <sqlite3.h>

namespace {
	class TournamentDatabase {
	public:
		TournamentDatabase() {
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

		~TournamentDatabase() {
			if (db) {
				sqlite3_close(db);
			}
		}

		TournamentDatabase(const TournamentDatabase&) = delete;
		TournamentDatabase& operator=(const TournamentDatabase&) = delete;

		bool execute(const std::string& sql) {
			if (!db) {
				return false;
			}

			char* error = nullptr;
			const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
			if (rc != SQLITE_OK) {
				std::cerr << "SQL Error: " << (error ? error : "unknown") << std::endl;
				sqlite3_free(error);
				return false;
			}

			return true;
		}

		sqlite3* handle() {
			return db;
		}

	private:
		sqlite3* db = nullptr;
	};

	TournamentDatabase& get_db() {
		static TournamentDatabase instance;
		return instance;
	}

	std::string column_text(sqlite3_stmt* stmt, int column) {
		const unsigned char* value = sqlite3_column_text(stmt, column);
		if (!value) {
			return "";
		}

		return reinterpret_cast<const char*>(value);
	}

	tournament_manage::TournamentRecord read_tournament(sqlite3_stmt* stmt) {
		tournament_manage::TournamentRecord record;
		record.id = sqlite3_column_int(stmt, 0);
		record.name = column_text(stmt, 1);
		record.game_type = column_text(stmt, 2);
		record.status = column_text(stmt, 3);
		record.registration_open = sqlite3_column_int(stmt, 4) != 0;
		record.checkin_open = sqlite3_column_int(stmt, 5) != 0;
		record.checkin_closes_at = sqlite3_column_int(stmt, 6);
		record.checkin_grace_time = sqlite3_column_int(stmt, 7);
		return record;
	}

	bool bind_text(sqlite3_stmt* stmt, int index, const std::string& value) {
		return sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
	}
}

bool tournament_manage::init() {
	const char* sql =
		"CREATE TABLE IF NOT EXISTS tournaments ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT, "
		"name TEXT NOT NULL, "
		"game_type TEXT, "
		"status TEXT DEFAULT 'open', "
		"registration_open INTEGER DEFAULT 0, "
		"checkin_open INTEGER DEFAULT 0, "
		"checkin_closes_at INTEGER DEFAULT 0, "
		"checkin_grace_time INTEGER DEFAULT 600"
		");";

	if (!get_db().execute(sql)) {
		return false;
	}

	const char* migrations[] = {
		"ALTER TABLE tournaments ADD COLUMN registration_open INTEGER DEFAULT 0;",
		"ALTER TABLE tournaments ADD COLUMN checkin_open INTEGER DEFAULT 0;",
		"ALTER TABLE tournaments ADD COLUMN checkin_closes_at INTEGER DEFAULT 0;",
		"ALTER TABLE tournaments ADD COLUMN checkin_grace_time INTEGER DEFAULT 600;"
	};

	for (const char* migration : migrations) {
		char* error = nullptr;
		const int rc = sqlite3_exec(get_db().handle(), migration, nullptr, nullptr, &error);
		if (rc != SQLITE_OK) {
			const std::string message = error ? error : "";
			sqlite3_free(error);
			if (message.find("duplicate column name") == std::string::npos) {
				return false;
			}
		}
	}

	return true;
}

std::optional<int> tournament_manage::create_tournament(
	const std::string& name,
	const std::string& game_type,
	const std::string& status
) {
	if (name.empty()) {
		return std::nullopt;
	}

	if (!init()) {
		return std::nullopt;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"INSERT INTO tournaments (name, game_type, status) "
		"VALUES (?, ?, ?);";

	if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return std::nullopt;
	}

	const bool bound =
		bind_text(stmt, 1, name)
		&& bind_text(stmt, 2, game_type)
		&& bind_text(stmt, 3, status);

	const bool success = bound && sqlite3_step(stmt) == SQLITE_DONE;
	sqlite3_finalize(stmt);

	if (!success) {
		return std::nullopt;
	}

	return static_cast<int>(sqlite3_last_insert_rowid(get_db().handle()));
}

bool tournament_manage::update_tournament(int tournament_id, const TournamentUpdate& update) {
	if (tournament_id <= 0) {
		return false;
	}

	if (!update.name && !update.game_type && !update.status) {
		return false;
	}

	if (update.name && update.name->empty()) {
		return false;
	}

	if (!init()) {
		return false;
	}

	std::string sql = "UPDATE tournaments SET ";
	std::vector<std::string> values;

	if (update.name) {
		sql += "name = ?";
		values.push_back(*update.name);
	}

	if (update.game_type) {
		if (!values.empty()) sql += ", ";
		sql += "game_type = ?";
		values.push_back(*update.game_type);
	}

	if (update.status) {
		if (!values.empty()) sql += ", ";
		sql += "status = ?";
		values.push_back(*update.status);
	}

	sql += " WHERE id = ?;";

	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(get_db().handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	bool bound = true;
	for (int i = 0; i < static_cast<int>(values.size()); ++i) {
		bound = bound && bind_text(stmt, i + 1, values[i]);
	}

	bound = bound && sqlite3_bind_int(stmt, static_cast<int>(values.size()) + 1, tournament_id) == SQLITE_OK;

	const bool success = bound && sqlite3_step(stmt) == SQLITE_DONE;
	const int changed = sqlite3_changes(get_db().handle());
	sqlite3_finalize(stmt);

	return success && changed > 0;
}

bool tournament_manage::delete_tournament(int tournament_id) {
	if (tournament_id <= 0) {
		return false;
	}

	if (!init()) {
		return false;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql = "DELETE FROM tournaments WHERE id = ?;";

	if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	sqlite3_bind_int(stmt, 1, tournament_id);

	const bool success = sqlite3_step(stmt) == SQLITE_DONE;
	const int changed = sqlite3_changes(get_db().handle());
	sqlite3_finalize(stmt);

	return success && changed > 0;
}

bool tournament_manage::clear_all_tournament_data() {
	if (!init()) {
		return false;
	}

	const char* sql =
		"BEGIN TRANSACTION;"
		"DELETE FROM tournament_matches;"
		"DELETE FROM tournament_rulesets;"
		"DELETE FROM tournament_participants;"
		"DELETE FROM participants;"
		"DELETE FROM match_history;"
		"DELETE FROM tournaments;"
		"UPDATE guild_config SET "
		"tournament_staff_role_id = NULL, "
		"tournament_admin_role_id = NULL, "
		"tournament_channel_id = NULL;"
		"DELETE FROM sqlite_sequence WHERE name IN ("
		"'tournaments', 'tournament_matches', 'match_history'"
		");"
		"COMMIT;";

	if (get_db().execute(sql)) {
		return true;
	}

	get_db().execute("ROLLBACK;");
	return false;
}

bool tournament_manage::set_registration_open(int tournament_id, bool is_open) {
	if (tournament_id <= 0 || !init()) {
		return false;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"UPDATE tournaments "
		"SET registration_open = ?, status = CASE WHEN ? THEN 'open' ELSE status END "
		"WHERE id = ?;";

	if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	sqlite3_bind_int(stmt, 1, is_open ? 1 : 0);
	sqlite3_bind_int(stmt, 2, is_open ? 1 : 0);
	sqlite3_bind_int(stmt, 3, tournament_id);

	const bool success = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().handle()) > 0;
	sqlite3_finalize(stmt);
	return success;
}

bool tournament_manage::set_checkin_open(int tournament_id, bool is_open, int closes_at, int grace_time) {
	if (tournament_id <= 0 || grace_time < 0 || !init()) {
		return false;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"UPDATE tournaments "
		"SET checkin_open = ?, checkin_closes_at = ?, checkin_grace_time = ?, "
		"status = CASE WHEN ? THEN 'checkin' ELSE status END "
		"WHERE id = ?;";

	if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	sqlite3_bind_int(stmt, 1, is_open ? 1 : 0);
	sqlite3_bind_int(stmt, 2, is_open ? closes_at : 0);
	sqlite3_bind_int(stmt, 3, grace_time);
	sqlite3_bind_int(stmt, 4, is_open ? 1 : 0);
	sqlite3_bind_int(stmt, 5, tournament_id);

	const bool success = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().handle()) > 0;
	sqlite3_finalize(stmt);
	return success;
}

std::optional<tournament_manage::TournamentRecord> tournament_manage::get_tournament(int tournament_id) {
	if (tournament_id <= 0) {
		return std::nullopt;
	}

	if (!init()) {
		return std::nullopt;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"SELECT id, name, game_type, status, registration_open, checkin_open, checkin_closes_at, checkin_grace_time "
		"FROM tournaments "
		"WHERE id = ? "
		"LIMIT 1;";

	if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return std::nullopt;
	}

	sqlite3_bind_int(stmt, 1, tournament_id);

	std::optional<TournamentRecord> result;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		result = read_tournament(stmt);
	}

	sqlite3_finalize(stmt);
	return result;
}

std::vector<tournament_manage::TournamentRecord> tournament_manage::list_tournaments() {
	std::vector<TournamentRecord> result;
	if (!init()) {
		return result;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"SELECT id, name, game_type, status, registration_open, checkin_open, checkin_closes_at, checkin_grace_time "
		"FROM tournaments "
		"ORDER BY id DESC;";

	if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return result;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		result.push_back(read_tournament(stmt));
	}

	sqlite3_finalize(stmt);
	return result;
}

std::vector<tournament_manage::TournamentRecord> tournament_manage::list_tournaments_by_status(const std::string& status) {
	std::vector<TournamentRecord> result;
	if (!init()) {
		return result;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"SELECT id, name, game_type, status, registration_open, checkin_open, checkin_closes_at, checkin_grace_time "
		"FROM tournaments "
		"WHERE status = ? "
		"ORDER BY id DESC;";

	if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return result;
	}

	bind_text(stmt, 1, status);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		result.push_back(read_tournament(stmt));
	}

	sqlite3_finalize(stmt);
	return result;
}
