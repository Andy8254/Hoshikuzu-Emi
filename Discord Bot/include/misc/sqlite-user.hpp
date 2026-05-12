#ifndef MISC_SQLITE_USER_HPP
#define MISC_SQLITE_USER_HPP

#include <sqlite3.h>

#include <string>

namespace misc_user_sqlite {

	class UserDatabase {
	public:
		explicit UserDatabase(const std::string& db_path = default_path());
		~UserDatabase();

		UserDatabase(const UserDatabase&) = delete;
		UserDatabase& operator=(const UserDatabase&) = delete;

		bool ok() const { return db != nullptr; }
		bool execute(const std::string& sql);
		bool table_has_column(const std::string& table, const std::string& column);
		bool add_column_if_missing(const std::string& table, const std::string& column_definition);
		bool create_index_if_missing(const std::string& index_name, const std::string& table, const std::string& columns);

		sqlite3* get_handle() { return db; }
		const std::string& path() const { return resolved_path; }
		const std::string& last_error() const { return last_error_message; }

		static std::string default_path();

	private:
		sqlite3* db = nullptr;
		std::string resolved_path;
		std::string last_error_message;

		void set_error(const std::string& message);
	};

	class UserDatabaseTransaction {
	public:
		explicit UserDatabaseTransaction(UserDatabase& database);
		~UserDatabaseTransaction();

		UserDatabaseTransaction(const UserDatabaseTransaction&) = delete;
		UserDatabaseTransaction& operator=(const UserDatabaseTransaction&) = delete;

		bool ok() const { return active; }
		bool commit();
		void rollback();

	private:
		UserDatabase& db;
		bool active = false;
	};

	UserDatabase& user_db();
	bool init_user_database();

}

#endif
