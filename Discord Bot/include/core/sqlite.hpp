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

	sqlite3* get_handle() { return db; }
};

class PlayerManager {
public:
	static bool register_info(dpp::snowflake id);
	static bool change_info(dpp::snowflake id, const std::string& platform, const std::string& value);
	static bool delete_info(dpp::snowflake id);

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