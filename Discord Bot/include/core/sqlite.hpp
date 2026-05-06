#ifndef SQLITE_HPP
#define SQLITE_HPP

#include <dpp/dpp.h>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

class Database {
private:
	sqlite3* db;
public:
	Database(const std::string& db_path);
	~Database();

	Database(const Database&) = delete;
	Database& operator=(const Database&) = delete;

	bool execute(const std::string& sql);
	bool table_has_column(const std::string& table, const std::string& column);
	bool add_column_if_missing(const std::string& table, const std::string& column_definition);
	bool create_index_if_missing(const std::string& index_name, const std::string& table, const std::string& columns);
	bool set_schema_version(int version);
	int get_schema_version();

	sqlite3* get_handle() { return db; }
};

class DatabaseTransaction {
private:
	Database& db;
	bool active = false;

public:
	explicit DatabaseTransaction(Database& database);
	~DatabaseTransaction();

	DatabaseTransaction(const DatabaseTransaction&) = delete;
	DatabaseTransaction& operator=(const DatabaseTransaction&) = delete;

	bool ok() const { return active; }
	bool commit();
	void rollback();
};

class PlayerManager {
public:
	static bool register_info(dpp::snowflake id);
	static bool change_info(dpp::snowflake id, const std::string& platform, const std::string& value);
	static bool delete_info(dpp::snowflake id);
	static bool exists(dpp::snowflake id);
	static bool not_found(dpp::snowflake id) { return !exists(id); }
	static std::map<std::string, std::string> get_profile(dpp::snowflake id);
	static dpp::snowflake find_by_platform(const std::string& platform, const std::string& handle);
	static bool unlink_platform(dpp::snowflake id, const std::string& platform);
	static void send_profile_embed(dpp::cluster& bot, const dpp::slashcommand_t& event, dpp::snowflake target_id);
private:
	static Database& get_db();
};

class GuildConfigManager {
public:
	static bool init(); //create table if not exists

	// Staff Role
	static bool set_staff_role(dpp::snowflake guild_id, dpp::snowflake role_id);
	static bool clear_staff_role(dpp::snowflake guild_id);
	static dpp::snowflake get_staff_role(dpp::snowflake guild_id);

	// Admin Role
	static bool set_admin_role(dpp::snowflake guild_id, dpp::snowflake role_id);
	static bool clear_admin_role(dpp::snowflake guild_id);
	static dpp::snowflake get_admin_role(dpp::snowflake guild_id);

	// Tournament automation channel
	static bool set_tournament_channel(dpp::snowflake guild_id, dpp::snowflake channel_id);
	static bool clear_tournament_channel(dpp::snowflake guild_id);
	static dpp::snowflake get_tournament_channel(dpp::snowflake guild_id);

	// Tournament log channel
	static bool set_tournament_log_channel(dpp::snowflake guild_id, dpp::snowflake channel_id);
	static bool clear_tournament_log_channel(dpp::snowflake guild_id);
	static dpp::snowflake get_tournament_log_channel(dpp::snowflake guild_id);

private:
	static Database& get_db();
};

class ServerSettingsManager {
public:
	static constexpr dpp::snowflake DEVELOPER_ID = 543676141177798676;
	static bool init();

	static bool set_owner_if_empty(dpp::snowflake guild_id, dpp::snowflake owner_id);
	static dpp::snowflake get_owner(dpp::snowflake guild_id);

	static bool set_admin_role(dpp::snowflake guild_id, dpp::snowflake role_id);
	static dpp::snowflake get_admin_role(dpp::snowflake guild_id);

	static bool set_moderator_role(dpp::snowflake guild_id, dpp::snowflake role_id);
	static dpp::snowflake get_moderator_role(dpp::snowflake guild_id);

	static bool set_staff_role(dpp::snowflake guild_id, dpp::snowflake role_id);
	static dpp::snowflake get_staff_role(dpp::snowflake guild_id);

	static bool set_language(dpp::snowflake guild_id, const std::string& language);
	static std::string get_language(dpp::snowflake guild_id);
	static bool set_secondary_language(dpp::snowflake guild_id, const std::string& language);
	static bool clear_secondary_language(dpp::snowflake guild_id);
	static std::string get_secondary_language(dpp::snowflake guild_id);

	static bool set_modlog_channel(dpp::snowflake guild_id, dpp::snowflake channel_id);
	static bool clear_modlog_channel(dpp::snowflake guild_id);
	static dpp::snowflake get_modlog_channel(dpp::snowflake guild_id);
	static Database& get_db();
};

class UserSettingsManager {
public:
	static bool init();
	static bool set_language(dpp::snowflake user_id, const std::string& language);
	static bool clear_language(dpp::snowflake user_id);
	static std::string get_language(dpp::snowflake user_id);

private:
	static Database& get_db();
};

struct ModerationCase {
	int id = 0;
	dpp::snowflake guild_id = 0;
	dpp::snowflake target_id = 0;
	dpp::snowflake actor_id = 0;
	std::string action;
	std::string reason;
	int duration_seconds = 0;
	int created_at = 0;
};

class ModerationManager {
public:
	static bool init();
	static bool encryption_enabled();
	static std::optional<int> create_case(
		dpp::snowflake guild_id,
		dpp::snowflake target_id,
		dpp::snowflake actor_id,
		const std::string& action,
		const std::string& reason,
		int duration_seconds = 0
	);
	static std::vector<ModerationCase> list_cases(dpp::snowflake guild_id, dpp::snowflake target_id, int limit = 10);

private:
	static Database& get_db();
};

// --- Match History & Reporting ---
class MatchManager {
public:
	// record_match(winner_id, loser_id, game_type, score)
	static bool record_result(dpp::snowflake winner, dpp::snowflake loser, const std::string& game, const std::string& score);
	static std::vector<std::string> get_recent_matches(dpp::snowflake player_id);
};

// --- Tournament & Bracket Logic ---
class TournamentManager {
public:
	static bool create_tournament(const std::string& name, const std::string& game);
	static bool add_participant(int tournament_id, dpp::snowflake player_id);
	static bool seed_players(int tournament_id);
	// Logic for Swiss, Single Elim, etc.
};

#endif
