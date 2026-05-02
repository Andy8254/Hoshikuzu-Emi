#include "core/sqlite.hpp"
#include <iostream>
#include <ctime>
#include <unordered_set>

static const std::vector<std::string> allowed = {
    "tetrio_id", "jstris_id", "ppt2_id", "tec_id",
    "tetra_id", "tgm_id", "ctwc_id", "other_id"
};

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

    bool was_inserted = (sqlite3_changes(get_db().get_handle()) > 0);

    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE && was_inserted);
}

bool PlayerManager::change_info(dpp::snowflake id, const std::string& platform, const std::string& value) {
    // Whitelist check
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

    int affected = sqlite3_changes(get_db().get_handle());

    sqlite3_finalize(stmt);
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
    if (!ok) return false;

    sqlite3_stmt* stmt;
    std::string sql = "UPDATE player_links SET " + platform + " = NULL, last_sync = ? WHERE discord_id = ?;";

    if (sqlite3_prepare_v2(get_db().get_handle(), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, (long long)time(nullptr));
    sqlite3_bind_int64(stmt, 2, (long long)id);

    int rc = sqlite3_step(stmt);
    int affected = sqlite3_changes(get_db().get_handle());
    sqlite3_finalize(stmt);

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
        "tournament_channel_id INTEGER"
        ");";

    if (!get_db().execute(sql)) {
        return false;
    }

    char* error = nullptr;
    const int rc = sqlite3_exec(
        get_db().get_handle(),
        "ALTER TABLE guild_config ADD COLUMN tournament_channel_id INTEGER;",
        nullptr,
        nullptr,
        &error
    );

    if (rc != SQLITE_OK) {
        const std::string message = error ? error : "";
        sqlite3_free(error);
        return message.find("duplicate column name") != std::string::npos;
    }

    return true;
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
