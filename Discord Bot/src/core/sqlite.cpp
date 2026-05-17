#include "core/sqlite.hpp"
#include "core/Log.hpp"
#include "core/SensitiveText.hpp"
#include <cstdlib>
#include <ctime>
#include <unordered_set>

static const std::vector<std::string> allowed = {
    "tetrio_id", "jstris_id", "ppt2_id", "tec_id",
    "tetra_id", "tgm_id", "ctwc_id", "other_id"
};

namespace {
    std::string resolve_db_path(const std::string& db_path) {
        char* override_path = nullptr;
        size_t override_size = 0;
        const errno_t rc = _dupenv_s(&override_path, &override_size, "BOT_DB_PATH");
        std::string result = db_path;

        if (rc == 0 && override_path && *override_path && db_path == "db/master.db") {
            result = override_path;
        }

        free(override_path);
        return result;
    }

    void log_sqlite(const std::string& action, const std::string& detail = "") {
        bot_log::info("sqlite", action, detail);
    }

    void log_sqlite_error(const std::string& action, const std::string& detail = "") {
        bot_log::error("sqlite", action, detail);
    }
}

Database::Database(const std::string& db_path) {
    //Ensure root/db directory exists
    const std::string resolved_db_path = resolve_db_path(db_path);
    std::filesystem::path p(resolved_db_path);
    if (p.has_parent_path() && !std::filesystem::exists(p.parent_path())) {
        std::filesystem::create_directories(p.parent_path());
    }

    //Open Connection
    if (sqlite3_open(resolved_db_path.c_str(), &db) != SQLITE_OK) {
        log_sqlite_error("open_failed", std::string("path=") + resolved_db_path + " error=\"" + (db ? sqlite3_errmsg(db) : "no_handle") + "\"");
        db = nullptr;
    }
    else {
        log_sqlite("open_ok", "path=" + resolved_db_path);
        sqlite3_busy_timeout(db, 5000);
        execute("PRAGMA foreign_keys = ON;");
        execute("PRAGMA journal_mode = WAL;");
        execute("PRAGMA synchronous = NORMAL;");
        execute("PRAGMA temp_store = MEMORY;");

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
        log_sqlite("master_schema_initialized", "path=" + resolved_db_path);
    }
}

Database::~Database() {
    if (db) sqlite3_close(db);
}

bool Database::execute(const std::string& sql) {
    char* zErrmsg = 0;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &zErrmsg);
    if (rc != SQLITE_OK) {
        log_sqlite_error("execute_failed", std::string("error=\"") + (zErrmsg ? zErrmsg : sqlite3_errmsg(db)) + "\"");
        sqlite3_free(zErrmsg);
        return false;
    }
    return true;
}

bool PlayerManager::register_info(dpp::snowflake id) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT OR IGNORE INTO player_links (discord_id) VALUES (?);";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log_sqlite_error("player_register_prepare_failed", "discord_id=" + std::to_string(id));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, (long long)id);
    int rc = sqlite3_step(stmt);

    bool was_inserted = (sqlite3_changes(get_db().get_handle()) > 0);

    sqlite3_finalize(stmt);
    log_sqlite(rc == SQLITE_DONE ? "player_register_done" : "player_register_failed", "discord_id=" + std::to_string(id) + " inserted=" + std::to_string(was_inserted ? 1 : 0));
    return (rc == SQLITE_DONE && was_inserted);
}

bool PlayerManager::change_info(dpp::snowflake id, const std::string& platform, const std::string& value) {
    // Whitelist check
    bool ok = false;
    for (const auto& p : allowed) if (p == platform) ok = true;
    if (!ok) {
        log_sqlite_error("player_change_rejected", "discord_id=" + std::to_string(id) + " platform=" + platform);
        return false;
    }

    sqlite3_stmt* stmt;
    std::string sql = "UPDATE player_links SET " + platform + " = ?, last_sync = ? WHERE discord_id = ?;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        log_sqlite_error("player_change_prepare_failed", "discord_id=" + std::to_string(id) + " platform=" + platform);
        return false;
    }
    sqlite3_bind_text(stmt, 1, value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, (long long)time(nullptr));
    sqlite3_bind_int64(stmt, 3, (long long)id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    log_sqlite(rc == SQLITE_DONE ? "player_change_done" : "player_change_failed", "discord_id=" + std::to_string(id) + " platform=" + platform);
    return rc == SQLITE_DONE;
}

bool PlayerManager::delete_info(dpp::snowflake id) {
    sqlite3_stmt* stmt;
    // This will also wipe tournament entries if ON DELETE CASCADE is set
    const char* sql = "DELETE FROM player_links WHERE discord_id = ?;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log_sqlite_error("player_delete_prepare_failed", "discord_id=" + std::to_string(id));
        return false;
    }

    sqlite3_bind_int64(stmt, 1, (long long)id);
    int rc = sqlite3_step(stmt);

    int affected = sqlite3_changes(get_db().get_handle());

    sqlite3_finalize(stmt);
    log_sqlite(rc == SQLITE_DONE ? "player_delete_done" : "player_delete_failed", "discord_id=" + std::to_string(id) + " affected=" + std::to_string(affected));
    return (rc == SQLITE_DONE && affected > 0);
}

std::map<std::string, std::string> PlayerManager::get_profile(dpp::snowflake id) {
    std::map<std::string, std::string> profile;
    sqlite3_stmt* stmt;
    const char* sql = "SELECT * FROM player_links WHERE discord_id = ?;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, (long long)id);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int cols = sqlite3_column_count(stmt);
            for (int i = 0; i < cols; i++) {
                const char* col_name = sqlite3_column_name(stmt, i);
                const unsigned char* val = sqlite3_column_text(stmt, i);
                if (val) {
                    profile[col_name] = reinterpret_cast<const char*>(val);
                }
            }
        }
    }
    sqlite3_finalize(stmt);
    return profile;
}

dpp::snowflake PlayerManager::find_by_platform(const std::string& platform, const std::string& handle) {
    dpp::snowflake found_id = 0;

    // 1. Hard Whitelist Check (Critical!)
    //Duplicates Unavoidable?
    static const std::unordered_set<std::string> allowed_platforms = { "tetrio_id", "jstris_id", "ppt2_id", "tec_id", "tetra_id", "tgm_id", "ctwc_id", "other_id" };
    if (allowed_platforms.find(platform) == allowed_platforms.end()) {
        log_sqlite_error("player_find_rejected", "platform=" + platform);
        return 0; // Or throw an exception
    }

    sqlite3_stmt* stmt;
    // Now the platform concatenation is safe because we verified the string
    std::string sql = "SELECT discord_id FROM player_links WHERE " + platform + " = ? LIMIT 1;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        // Use SQLITE_TRANSIENT because 'handle' is a reference that might go out of scope
        sqlite3_bind_text(stmt, 1, handle.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            // Using int64 ensures we don't truncate the 64-bit Discord Snowflake
            found_id = static_cast<dpp::snowflake>(sqlite3_column_int64(stmt, 0));
        }
    }

    sqlite3_finalize(stmt);
    return found_id;
}

bool PlayerManager::exists(dpp::snowflake id) {
    sqlite3_stmt* stmt;
    // 'SELECT 1' is a classic optimization: it returns the number 1 if the row exists
    const char* sql = "SELECT 1 FROM player_links WHERE discord_id = ? LIMIT 1;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, (long long)id);

    // If sqlite3_step returns SQLITE_ROW, it found something
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_ROW);
}

bool PlayerManager::unlink_platform(dpp::snowflake id, const std::string& platform) {
    bool ok = false;
    for (const auto& p : allowed) if (p == platform) ok = true;
    if (!ok) {
        log_sqlite_error("player_unlink_rejected", "discord_id=" + std::to_string(id) + " platform=" + platform);
        return false;
    }

    sqlite3_stmt* stmt;
    std::string sql = "UPDATE player_links SET " + platform + " = NULL, last_sync = ? WHERE discord_id = ?;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        log_sqlite_error("player_unlink_prepare_failed", "discord_id=" + std::to_string(id) + " platform=" + platform);
        return false;
    }

    sqlite3_bind_int64(stmt, 1, (long long)time(nullptr));
    sqlite3_bind_int64(stmt, 2, (long long)id);

    int rc = sqlite3_step(stmt);
    int affected = sqlite3_changes(get_db().get_handle());
    sqlite3_finalize(stmt);

    log_sqlite(rc == SQLITE_DONE ? "player_unlink_done" : "player_unlink_failed", "discord_id=" + std::to_string(id) + " platform=" + platform + " affected=" + std::to_string(affected));
    return (rc == SQLITE_DONE && affected > 0);
}

void PlayerManager::send_profile_embed(
    dpp::cluster& bot,
    const dpp::slashcommand_t& event,
    dpp::snowflake target_id
) {
    auto data = PlayerManager::get_profile(target_id);

    if (data.empty()) {
        event.reply("⚠️ This user is not registered in the system.");
        return;
    }

    auto data_ptr = std::make_shared<std::map<std::string, std::string>>(data);

    bot.user_get(target_id, [event, data_ptr](const dpp::confirmation_callback_t& cb) mutable {
        if (cb.is_error()) {
            event.reply("Could not retrieve user data from Discord.");
            return;
        }

        dpp::user_identified target_user = std::get<dpp::user_identified>(cb.value);

        dpp::embed embed = dpp::embed()
            .set_color(0x3498db)
            .set_title(target_user.username + "'s Profile")
            .set_thumbnail(target_user.get_avatar_url())
            .set_timestamp(time(nullptr));

        for (auto const& [platform, handle] : *data_ptr) {
            if (platform == "discord_id" || platform == "last_sync" || handle.empty()) continue;

            std::string display_name = platform;
            size_t pos = display_name.find("_id");
            if (pos != std::string::npos) display_name.erase(pos);

            embed.add_field(display_name, handle, true);
        }

        if (data_ptr->count("last_sync") && !(*data_ptr)["last_sync"].empty()) {
            try {
                embed.set_footer(dpp::embed_footer().set_text("Last updated"));
                embed.set_timestamp(std::stoll((*data_ptr)["last_sync"]));
            }
            catch (...) {}
        }

        event.reply(dpp::message().add_embed(embed));
        });
}

// --- DB instance ---
Database& GuildConfigManager::get_db() {
    static Database instance("db/master.db");
    return instance;
}

// --- Init ---
bool GuildConfigManager::init() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS guild_config ("
        "guild_id INTEGER PRIMARY KEY,"
        "tournament_staff_role_id INTEGER,"
        "tournament_admin_role_id INTEGER,"
        "tournament_channel_id INTEGER,"
        "tournament_log_channel_id INTEGER"
        ");";

    if (!get_db().execute(sql)) {
        return false;
    }

    const char* migrations[] = {
        "tournament_channel_id INTEGER",
        "tournament_log_channel_id INTEGER"
    };

    for (const char* migration : migrations) {
        if (!get_db().add_column_if_missing("guild_config", migration)) {
            return false;
        }
    }

    return get_db().set_schema_version(1);
}

bool Database::table_has_column(const std::string& table, const std::string& column) {
    if (!db || table.empty() || column.empty()) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const std::string sql = "PRAGMA table_info(" + table + ");";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
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
    return found;
}

bool Database::add_column_if_missing(const std::string& table, const std::string& column_definition) {
    if (!db || table.empty() || column_definition.empty()) {
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

bool Database::create_index_if_missing(const std::string& index_name, const std::string& table, const std::string& columns) {
    if (!db || index_name.empty() || table.empty() || columns.empty()) {
        return false;
    }

    return execute("CREATE INDEX IF NOT EXISTS " + index_name + " ON " + table + " (" + columns + ");");
}

bool Database::set_schema_version(int version) {
    if (!db || version < 0) {
        return false;
    }

    if (!execute(
        "CREATE TABLE IF NOT EXISTS schema_meta ("
        "key TEXT PRIMARY KEY,"
        "value TEXT NOT NULL"
        ");"
    )) {
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO schema_meta (key, value) "
        "VALUES ('schema_version', ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    const std::string value = std::to_string(version);
    sqlite3_bind_text(stmt, 1, value.c_str(), -1, SQLITE_TRANSIENT);
    const bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

int Database::get_schema_version() {
    if (!db) {
        return 0;
    }

    if (!execute(
        "CREATE TABLE IF NOT EXISTS schema_meta ("
        "key TEXT PRIMARY KEY,"
        "value TEXT NOT NULL"
        ");"
    )) {
        return 0;
    }

    sqlite3_stmt* stmt = nullptr;
    int result = 0;
    const char* sql = "SELECT value FROM schema_meta WHERE key = 'schema_version' LIMIT 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* value = sqlite3_column_text(stmt, 0);
            if (value) {
                result = std::atoi(reinterpret_cast<const char*>(value));
            }
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

DatabaseTransaction::DatabaseTransaction(Database& database)
    : db(database),
    active(database.execute("BEGIN TRANSACTION;")) {
}

DatabaseTransaction::~DatabaseTransaction() {
    if (active) {
        db.execute("ROLLBACK;");
    }
}

bool DatabaseTransaction::commit() {
    if (!active) {
        return false;
    }

    if (!db.execute("COMMIT;")) {
        return false;
    }

    active = false;
    return true;
}

void DatabaseTransaction::rollback() {
    if (active) {
        db.execute("ROLLBACK;");
        active = false;
    }
}

bool GuildConfigManager::set_staff_role(dpp::snowflake guild_id, dpp::snowflake role_id) {
    sqlite3_stmt* stmt;

    const char* sql =
        "INSERT INTO guild_config (guild_id, tournament_staff_role_id) "
        "VALUES (?, ?) "
        "ON CONFLICT(guild_id) DO UPDATE SET "
        "tournament_staff_role_id = excluded.tournament_staff_role_id;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_int64(stmt, 2, role_id);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool GuildConfigManager::clear_staff_role(dpp::snowflake guild_id) {
    sqlite3_stmt* stmt;

    const char* sql =
        "INSERT INTO guild_config (guild_id, tournament_staff_role_id) "
        "VALUES (?, NULL) "
        "ON CONFLICT(guild_id) DO UPDATE SET "
        "tournament_staff_role_id = NULL;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, guild_id);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

dpp::snowflake GuildConfigManager::get_staff_role(dpp::snowflake guild_id) {
    sqlite3_stmt* stmt;
    dpp::snowflake result = 0;

    const char* sql =
        "SELECT tournament_staff_role_id FROM guild_config WHERE guild_id = ? LIMIT 1;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, guild_id);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = static_cast<dpp::snowflake>(sqlite3_column_int64(stmt, 0));
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

bool GuildConfigManager::set_admin_role(dpp::snowflake guild_id, dpp::snowflake role_id) {
    sqlite3_stmt* stmt;

    const char* sql =
        "INSERT INTO guild_config (guild_id, tournament_admin_role_id) "
        "VALUES (?, ?) "
        "ON CONFLICT(guild_id) DO UPDATE SET "
        "tournament_admin_role_id = excluded.tournament_admin_role_id;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_int64(stmt, 2, role_id);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool GuildConfigManager::clear_admin_role(dpp::snowflake guild_id) {
    sqlite3_stmt* stmt;

    const char* sql =
        "INSERT INTO guild_config (guild_id, tournament_admin_role_id) "
        "VALUES (?, NULL) "
        "ON CONFLICT(guild_id) DO UPDATE SET "
        "tournament_admin_role_id = NULL;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, guild_id);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

dpp::snowflake GuildConfigManager::get_admin_role(dpp::snowflake guild_id) {
    sqlite3_stmt* stmt;
    dpp::snowflake result = 0;

    const char* sql =
        "SELECT tournament_admin_role_id FROM guild_config WHERE guild_id = ? LIMIT 1;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, guild_id);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = static_cast<dpp::snowflake>(sqlite3_column_int64(stmt, 0));
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

bool GuildConfigManager::set_tournament_channel(dpp::snowflake guild_id, dpp::snowflake channel_id) {
    if (!init()) {
        return false;
    }

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO guild_config (guild_id, tournament_channel_id) "
        "VALUES (?, ?) "
        "ON CONFLICT(guild_id) DO UPDATE SET "
        "tournament_channel_id = excluded.tournament_channel_id;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_int64(stmt, 2, channel_id);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool GuildConfigManager::clear_tournament_channel(dpp::snowflake guild_id) {
    if (!init()) {
        return false;
    }

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO guild_config (guild_id, tournament_channel_id) "
        "VALUES (?, NULL) "
        "ON CONFLICT(guild_id) DO UPDATE SET "
        "tournament_channel_id = NULL;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, guild_id);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

dpp::snowflake GuildConfigManager::get_tournament_channel(dpp::snowflake guild_id) {
    if (!init()) {
        return 0;
    }

    sqlite3_stmt* stmt;
    dpp::snowflake result = 0;

    const char* sql =
        "SELECT tournament_channel_id FROM guild_config WHERE guild_id = ? LIMIT 1;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, guild_id);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = static_cast<dpp::snowflake>(sqlite3_column_int64(stmt, 0));
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

bool GuildConfigManager::set_tournament_log_channel(dpp::snowflake guild_id, dpp::snowflake channel_id) {
    if (!init()) {
        return false;
    }

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO guild_config (guild_id, tournament_log_channel_id) "
        "VALUES (?, ?) "
        "ON CONFLICT(guild_id) DO UPDATE SET "
        "tournament_log_channel_id = excluded.tournament_log_channel_id;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_int64(stmt, 2, channel_id);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool GuildConfigManager::clear_tournament_log_channel(dpp::snowflake guild_id) {
    if (!init()) {
        return false;
    }

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO guild_config (guild_id, tournament_log_channel_id) "
        "VALUES (?, NULL) "
        "ON CONFLICT(guild_id) DO UPDATE SET "
        "tournament_log_channel_id = NULL;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, guild_id);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

dpp::snowflake GuildConfigManager::get_tournament_log_channel(dpp::snowflake guild_id) {
    if (!init()) {
        return 0;
    }

    sqlite3_stmt* stmt;
    dpp::snowflake result = 0;

    const char* sql =
        "SELECT tournament_log_channel_id FROM guild_config WHERE guild_id = ? LIMIT 1;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, guild_id);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = static_cast<dpp::snowflake>(sqlite3_column_int64(stmt, 0));
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

Database& ServerSettingsManager::get_db() {
    static Database instance("db/master.db");
    return instance;
}

bool ServerSettingsManager::init() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS server_settings ("
        "guild_id INTEGER PRIMARY KEY,"
        "owner_id INTEGER,"
        "admin_role_id INTEGER,"
        "moderator_role_id INTEGER,"
        "staff_role_id INTEGER,"
        "language TEXT DEFAULT 'EN-gb',"
        "secondary_language TEXT DEFAULT NULL,"
        "modlog_channel_id INTEGER"
        ");";

    if (!get_db().execute(sql)) {
        return false;
    }

    const char* migrations[] = {
        "owner_id INTEGER",
        "admin_role_id INTEGER",
        "moderator_role_id INTEGER",
        "staff_role_id INTEGER",
        "language TEXT DEFAULT 'EN-gb'",
        "secondary_language TEXT DEFAULT NULL",
        "modlog_channel_id INTEGER"
    };

    for (const char* migration : migrations) {
        if (!get_db().add_column_if_missing("server_settings", migration)) {
            return false;
        }
    }

    return get_db().create_index_if_missing("idx_server_settings_language", "server_settings", "language")
        && get_db().set_schema_version(1);
}

bool ServerSettingsManager::set_owner_if_empty(dpp::snowflake guild_id, dpp::snowflake owner_id) {
    if (!init() || !guild_id || !owner_id) {
        return false;
    }

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO server_settings (guild_id, owner_id, language) "
        "VALUES (?, ?, 'EN-gb') "
        "ON CONFLICT(guild_id) DO UPDATE SET "
        "owner_id = CASE "
        "WHEN server_settings.owner_id IS NULL OR server_settings.owner_id = 0 THEN excluded.owner_id "
        "ELSE server_settings.owner_id END;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_int64(stmt, 2, owner_id);
    const bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

dpp::snowflake ServerSettingsManager::get_owner(dpp::snowflake guild_id) {
    if (!init()) {
        return 0;
    }

    sqlite3_stmt* stmt;
    dpp::snowflake result = 0;
    const char* sql = "SELECT owner_id FROM server_settings WHERE guild_id = ? LIMIT 1;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, guild_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = static_cast<dpp::snowflake>(sqlite3_column_int64(stmt, 0));
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

namespace {
    bool set_server_role_column(dpp::snowflake guild_id, dpp::snowflake role_id, const char* column) {
        if (!ServerSettingsManager::init() || !guild_id || !role_id) {
            return false;
        }

        std::string sql = std::string("INSERT INTO server_settings (guild_id, ") + column + ") "
            "VALUES (?, ?) "
            "ON CONFLICT(guild_id) DO UPDATE SET " + column + " = excluded." + column + ";";

        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(ServerSettingsManager::get_db().get_handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_int64(stmt, 1, guild_id);
        sqlite3_bind_int64(stmt, 2, role_id);
        const bool success = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return success;
    }

    dpp::snowflake get_server_role_column(dpp::snowflake guild_id, const char* column) {
        if (!ServerSettingsManager::init()) {
            return 0;
        }

        std::string sql = std::string("SELECT ") + column + " FROM server_settings WHERE guild_id = ? LIMIT 1;";
        sqlite3_stmt* stmt;
        dpp::snowflake result = 0;

        if (sqlite3_prepare_v2(ServerSettingsManager::get_db().get_handle(), sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, guild_id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                result = static_cast<dpp::snowflake>(sqlite3_column_int64(stmt, 0));
            }
        }

        sqlite3_finalize(stmt);
        return result;
    }
}

bool ServerSettingsManager::set_admin_role(dpp::snowflake guild_id, dpp::snowflake role_id) {
    return set_server_role_column(guild_id, role_id, "admin_role_id");
}

dpp::snowflake ServerSettingsManager::get_admin_role(dpp::snowflake guild_id) {
    return get_server_role_column(guild_id, "admin_role_id");
}

bool ServerSettingsManager::set_moderator_role(dpp::snowflake guild_id, dpp::snowflake role_id) {
    return set_server_role_column(guild_id, role_id, "moderator_role_id");
}

dpp::snowflake ServerSettingsManager::get_moderator_role(dpp::snowflake guild_id) {
    return get_server_role_column(guild_id, "moderator_role_id");
}

bool ServerSettingsManager::set_staff_role(dpp::snowflake guild_id, dpp::snowflake role_id) {
    return set_server_role_column(guild_id, role_id, "staff_role_id");
}

dpp::snowflake ServerSettingsManager::get_staff_role(dpp::snowflake guild_id) {
    return get_server_role_column(guild_id, "staff_role_id");
}

bool ServerSettingsManager::set_language(dpp::snowflake guild_id, const std::string& language) {
    if (!init() || !guild_id || language.empty()) {
        return false;
    }

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO server_settings (guild_id, language) "
        "VALUES (?, ?) "
        "ON CONFLICT(guild_id) DO UPDATE SET language = excluded.language;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_text(stmt, 2, language.c_str(), -1, SQLITE_TRANSIENT);
    const bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::string ServerSettingsManager::get_language(dpp::snowflake guild_id) {
    if (!init()) {
        return "EN-gb";
    }

    sqlite3_stmt* stmt;
    std::string result = "EN-gb";
    const char* sql = "SELECT language FROM server_settings WHERE guild_id = ? LIMIT 1;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, guild_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* value = sqlite3_column_text(stmt, 0);
            if (value) {
                result = reinterpret_cast<const char*>(value);
            }
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

bool ServerSettingsManager::set_secondary_language(dpp::snowflake guild_id, const std::string& language) {
    if (!init() || !guild_id || language.empty()) {
        return false;
    }

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO server_settings (guild_id, secondary_language) "
        "VALUES (?, ?) "
        "ON CONFLICT(guild_id) DO UPDATE SET secondary_language = excluded.secondary_language;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_text(stmt, 2, language.c_str(), -1, SQLITE_TRANSIENT);
    const bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool ServerSettingsManager::clear_secondary_language(dpp::snowflake guild_id) {
    if (!init() || !guild_id) {
        return false;
    }

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO server_settings (guild_id, secondary_language) "
        "VALUES (?, NULL) "
        "ON CONFLICT(guild_id) DO UPDATE SET secondary_language = NULL;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, guild_id);
    const bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::string ServerSettingsManager::get_secondary_language(dpp::snowflake guild_id) {
    if (!init()) {
        return "";
    }

    sqlite3_stmt* stmt;
    std::string result;
    const char* sql = "SELECT secondary_language FROM server_settings WHERE guild_id = ? LIMIT 1;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, guild_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* value = sqlite3_column_text(stmt, 0);
            if (value) {
                result = reinterpret_cast<const char*>(value);
            }
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

bool ServerSettingsManager::set_modlog_channel(dpp::snowflake guild_id, dpp::snowflake channel_id) {
    if (!init() || !guild_id || !channel_id) {
        return false;
    }

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO server_settings (guild_id, modlog_channel_id) "
        "VALUES (?, ?) "
        "ON CONFLICT(guild_id) DO UPDATE SET modlog_channel_id = excluded.modlog_channel_id;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_int64(stmt, 2, channel_id);
    const bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool ServerSettingsManager::clear_modlog_channel(dpp::snowflake guild_id) {
    if (!init() || !guild_id) {
        return false;
    }

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO server_settings (guild_id, modlog_channel_id) "
        "VALUES (?, NULL) "
        "ON CONFLICT(guild_id) DO UPDATE SET modlog_channel_id = NULL;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, guild_id);
    const bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

dpp::snowflake ServerSettingsManager::get_modlog_channel(dpp::snowflake guild_id) {
    if (!init()) {
        return 0;
    }

    sqlite3_stmt* stmt;
    dpp::snowflake result = 0;
    const char* sql = "SELECT modlog_channel_id FROM server_settings WHERE guild_id = ? LIMIT 1;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, guild_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = static_cast<dpp::snowflake>(sqlite3_column_int64(stmt, 0));
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

Database& UserSettingsManager::get_db() {
    static Database instance("db/master.db");
    return instance;
}

bool UserSettingsManager::init() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS user_settings ("
        "user_id INTEGER PRIMARY KEY,"
        "language TEXT DEFAULT ''"
        ");";

    return get_db().execute(sql)
        && get_db().create_index_if_missing("idx_user_settings_language", "user_settings", "language")
        && get_db().set_schema_version(1);
}

bool UserSettingsManager::set_language(dpp::snowflake user_id, const std::string& language) {
    if (!init() || !user_id || language.empty()) {
        return false;
    }

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO user_settings (user_id, language) "
        "VALUES (?, ?) "
        "ON CONFLICT(user_id) DO UPDATE SET language = excluded.language;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, language.c_str(), -1, SQLITE_TRANSIENT);
    const bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

bool UserSettingsManager::clear_language(dpp::snowflake user_id) {
    if (!init() || !user_id) {
        return false;
    }

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO user_settings (user_id, language) "
        "VALUES (?, '') "
        "ON CONFLICT(user_id) DO UPDATE SET language = '';";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, user_id);
    const bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return success;
}

std::string UserSettingsManager::get_language(dpp::snowflake user_id) {
    if (!init()) {
        return "";
    }

    sqlite3_stmt* stmt;
    std::string result;
    const char* sql = "SELECT language FROM user_settings WHERE user_id = ? LIMIT 1;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* value = sqlite3_column_text(stmt, 0);
            if (value) {
                result = reinterpret_cast<const char*>(value);
            }
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

Database& ModerationManager::get_db() {
    static Database instance("db/master.db");
    return instance;
}

bool ModerationManager::init() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS moderation_cases ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "guild_id INTEGER NOT NULL,"
        "target_id INTEGER NOT NULL,"
        "actor_id INTEGER NOT NULL,"
        "action TEXT NOT NULL,"
        "reason TEXT,"
        "duration_seconds INTEGER DEFAULT 0,"
        "created_at INTEGER DEFAULT 0"
        ");";

    return get_db().execute(sql)
        && get_db().create_index_if_missing(
            "idx_moderation_cases_guild_target_created",
            "moderation_cases",
            "guild_id, target_id, created_at"
        )
        && get_db().set_schema_version(1);
}

bool ModerationManager::encryption_enabled() {
    return sensitive_text::encryption_available();
}

std::optional<int> ModerationManager::create_case(
    dpp::snowflake guild_id,
    dpp::snowflake target_id,
    dpp::snowflake actor_id,
    const std::string& action,
    const std::string& reason,
    int duration_seconds
) {
    if (!init() || !guild_id || !target_id || !actor_id || action.empty()) {
        return std::nullopt;
    }

    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT INTO moderation_cases "
        "(guild_id, target_id, actor_id, action, reason, duration_seconds, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_int64(stmt, 2, target_id);
    sqlite3_bind_int64(stmt, 3, actor_id);
    sqlite3_bind_text(stmt, 4, action.c_str(), -1, SQLITE_TRANSIENT);
    const std::string stored_reason = sensitive_text::protect(reason);
    sqlite3_bind_text(stmt, 5, stored_reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, duration_seconds);
    sqlite3_bind_int(stmt, 7, static_cast<int>(time(nullptr)));

    const bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    if (!success) {
        return std::nullopt;
    }

    return static_cast<int>(sqlite3_last_insert_rowid(get_db().get_handle()));
}

std::vector<ModerationCase> ModerationManager::list_cases(dpp::snowflake guild_id, dpp::snowflake target_id, int limit) {
    std::vector<ModerationCase> result;
    if (!init() || !guild_id || !target_id) {
        return result;
    }

    sqlite3_stmt* stmt;
    const char* sql =
        "SELECT id, guild_id, target_id, actor_id, action, reason, duration_seconds, created_at "
        "FROM moderation_cases "
        "WHERE guild_id = ? AND target_id = ? "
        "ORDER BY id DESC LIMIT ?;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    sqlite3_bind_int64(stmt, 1, guild_id);
    sqlite3_bind_int64(stmt, 2, target_id);
    sqlite3_bind_int(stmt, 3, limit <= 0 ? 10 : limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ModerationCase entry;
        entry.id = sqlite3_column_int(stmt, 0);
        entry.guild_id = static_cast<dpp::snowflake>(sqlite3_column_int64(stmt, 1));
        entry.target_id = static_cast<dpp::snowflake>(sqlite3_column_int64(stmt, 2));
        entry.actor_id = static_cast<dpp::snowflake>(sqlite3_column_int64(stmt, 3));
        const unsigned char* action = sqlite3_column_text(stmt, 4);
        const unsigned char* reason = sqlite3_column_text(stmt, 5);
        entry.action = action ? reinterpret_cast<const char*>(action) : "";
        entry.reason = reason ? sensitive_text::reveal(reinterpret_cast<const char*>(reason)) : "";
        entry.duration_seconds = sqlite3_column_int(stmt, 6);
        entry.created_at = sqlite3_column_int(stmt, 7);
        result.push_back(entry);
    }

    sqlite3_finalize(stmt);
    return result;
}
