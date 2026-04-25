#include "core/sqlite.hpp"
#include <iostream>
#include <ctime>

Database::Database(const std::string& db_path) {
    //Ensure root/db directory exists
    std::filesystem::path p(db_path);
    if (p.has_parent_path() && !std::filesystem::exists(p.parent_path())) {
        std::filesystem::create_directories(p.parent_path());
    }

    //Open Connection
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "CRITICAL: SQLITE Open Failed: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;
    }
    else {
        //Enable Foreign Keys for ON DELETE CASCADE
        execute("PRAGMA foreign_keys = ON;");
        sqlite3_busy_timeout(db, 5000);

        //Initialize Master Schema
        std::string master_schema =
            "CREATE TABLE IF NOT EXISTS player_links ("
            "discord_id INTEGER PRIMARY KEY, "
            "tetrio_id TEXT, jstris_id TEXT, ppt2_id TEXT, tec_id TEXT, "
            "tetra_id TEXT, tgm_id TEXT, ctwc_id TEXT, other_id TEXT, "
            "last_sync INTEGER);"

            "CREATE TABLE IF NOT EXISTS tournaments ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, "
            "game_type TEXT, status TEXT DEFAULT 'open');"

            "CREATE TABLE IF NOT EXISTS participants ("
            "t_id INTEGER, p_id INTEGER, seed INTEGER DEFAULT 0, placement INTEGER, "
            "PRIMARY KEY (t_id, p_id), "
            "FOREIGN KEY (t_id) REFERENCES tournaments(id) ON DELETE CASCADE, "
            "FOREIGN KEY (p_id) REFERENCES player_links(discord_id) ON DELETE CASCADE);"

            "CREATE TABLE IF NOT EXISTS match_history ("
            "m_id INTEGER PRIMARY KEY AUTOINCREMENT, t_id INTEGER, "
            "winner_id INTEGER, loser_id INTEGER, score TEXT, "
            "FOREIGN KEY (t_id) REFERENCES tournaments(id) ON DELETE CASCADE);";

        execute(master_schema);
    }
}

Database::~Database() {
    if (db) sqlite3_close(db);
}

bool Database::execute(const std::string& sql) {
    char* zErrmsg = 0;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &zErrmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL Error: " << zErrmsg << std::endl;
        sqlite3_free(zErrmsg);
        return false;
    }
    return true;
}

bool PlayerManager::register_info(dpp::snowflake id) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT OR IGNORE INTO player_links (discord_id) VALUES (?);";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, (long long)id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

bool PlayerManager::change_info(dpp::snowflake id, const std::string& platform, const std::string& value) {
    // Whitelist check
    static const std::vector<std::string> allowed = { "tetrio_id", "jstris_id", "ppt2_id", "tec_id" };
    bool ok = false;
    for (const auto& p : allowed) if (p == platform) ok = true;
    if (!ok) return false;

    sqlite3_stmt* stmt;
    std::string sql = "UPDATE player_links SET " + platform + " = ?, last_sync = ? WHERE discord_id = ?;";

    sqlite3_prepare_v2(get_db().get_handle(), sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, (long long)time(nullptr));
    sqlite3_bind_int64(stmt, 3, (long long)id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool PlayerManager::delete_info(dpp::snowflake id) {
    sqlite3_stmt* stmt;
    // This will also wipe tournament entries if ON DELETE CASCADE is set
    const char* sql = "DELETE FROM player_links WHERE discord_id = ?;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, (long long)id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}