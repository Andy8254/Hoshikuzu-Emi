#include "misc/sqlite-user.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace {
	std::string resolve_user_db_path(const std::string& fallback) {
		char* override_path = nullptr;
		size_t override_size = 0;
		const errno_t rc = _dupenv_s(&override_path, &override_size, "BOT_USER_DB_PATH");
		std::string result = fallback;

		if (rc == 0 && override_path && *override_path) {
			result = override_path;
		}

		free(override_path);
		return result;
	}
}

namespace misc_user_sqlite {

	std::string UserDatabase::default_path() {
		return "db/user.db";
	}

	UserDatabase::UserDatabase(const std::string& db_path)
		: resolved_path(resolve_user_db_path(db_path)) {
		std::filesystem::path path(resolved_path);
		if (path.has_parent_path() && !std::filesystem::exists(path.parent_path())) {
			std::filesystem::create_directories(path.parent_path());
		}

		if (sqlite3_open(resolved_path.c_str(), &db) != SQLITE_OK) {
			set_error(db ? sqlite3_errmsg(db) : "Could not allocate SQLite handle.");
			std::cerr << "CRITICAL: user SQLite open failed: " << last_error_message << std::endl;
			if (db) {
				sqlite3_close(db);
				db = nullptr;
			}
			return;
		}

		sqlite3_busy_timeout(db, 5000);
		execute("PRAGMA foreign_keys = ON;");
		execute("PRAGMA journal_mode = WAL;");
		execute("PRAGMA synchronous = NORMAL;");
		execute("PRAGMA temp_store = MEMORY;");
	}

	UserDatabase::~UserDatabase() {
		if (db) {
			sqlite3_close(db);
		}
	}

	void UserDatabase::set_error(const std::string& message) {
		last_error_message = message;
	}

	bool UserDatabase::execute(const std::string& sql) {
		if (!db || sql.empty()) {
			set_error("Database is not open or SQL is empty.");
			return false;
		}

		char* error = nullptr;
		const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
		if (rc != SQLITE_OK) {
			set_error(error ? error : sqlite3_errmsg(db));
			std::cerr << "User SQLite error: " << last_error_message << std::endl;
			sqlite3_free(error);
			return false;
		}

		set_error("");
		return true;
	}

	bool UserDatabase::table_has_column(const std::string& table, const std::string& column) {
		if (!db || table.empty() || column.empty()) {
			set_error("Database is not open, table is empty, or column is empty.");
			return false;
		}

		sqlite3_stmt* stmt = nullptr;
		const std::string sql = "PRAGMA table_info(" + table + ");";
		if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
			set_error(sqlite3_errmsg(db));
			return false;
		}

		bool found = false;
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			const unsigned char* value = sqlite3_column_text(stmt, 1);
			if (value && column == reinterpret_cast<const char*>(value)) {
				found = true;
				break;
			}
		}

		sqlite3_finalize(stmt);
		set_error("");
		return found;
	}

	bool UserDatabase::add_column_if_missing(const std::string& table, const std::string& column_definition) {
		if (!db || table.empty() || column_definition.empty()) {
			set_error("Database is not open, table is empty, or column definition is empty.");
			return false;
		}

		const size_t name_end = column_definition.find_first_of(" \t");
		const std::string column = name_end == std::string::npos
			? column_definition
			: column_definition.substr(0, name_end);

		if (table_has_column(table, column)) {
			return true;
		}

		return execute("ALTER TABLE " + table + " ADD COLUMN " + column_definition + ";");
	}

	bool UserDatabase::create_index_if_missing(
		const std::string& index_name,
		const std::string& table,
		const std::string& columns
	) {
		if (!db || index_name.empty() || table.empty() || columns.empty()) {
			set_error("Database is not open, index name is empty, table is empty, or columns are empty.");
			return false;
		}

		return execute("CREATE INDEX IF NOT EXISTS " + index_name + " ON " + table + " (" + columns + ");");
	}

	UserDatabaseTransaction::UserDatabaseTransaction(UserDatabase& database)
		: db(database),
		active(database.execute("BEGIN TRANSACTION;")) {
	}

	UserDatabaseTransaction::~UserDatabaseTransaction() {
		if (active) {
			db.execute("ROLLBACK;");
		}
	}

	bool UserDatabaseTransaction::commit() {
		if (!active) {
			return false;
		}

		if (!db.execute("COMMIT;")) {
			return false;
		}

		active = false;
		return true;
	}

	void UserDatabaseTransaction::rollback() {
		if (active) {
			db.execute("ROLLBACK;");
			active = false;
		}
	}

	UserDatabase& user_db() {
		static UserDatabase instance;
		return instance;
	}

	bool init_user_database() {
		return user_db().ok();
	}

}
