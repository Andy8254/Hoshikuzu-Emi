#include "core/CommandRegistry.hpp"
#include "core/sqlite.hpp"

/*
This section will be for commands related to player profiles, stats, and other player-related information,
which, consequently, will be used by Bracket.cpp as well as tetrio.cpp.
*/

Database& PlayerManager::get_db() {
	static Database instance("db/players.db");
	return instance;
}

void register_player_commands(dpp::cluster& bot) {
	auto bot_ptr = &bot;

	// -- REGISTER COMMAND --
	handlers["register"] = [bot_ptr](const dpp::slashcommand_t& event) {
		dpp::snowflake user_id = event.command.usr.id;

		if (PlayerManager::register_info(user_id)) {
			event.reply("✅ Profile initialized. Use `/link` to connect your Tetris accounts.");
		}
		else {
			event.reply("⚠️ You are already registered or a database error occurred.");
		}
	};

	// -- LINK COMMAND --
	handlers["link"] = [bot_ptr](const dpp::slashcommand_t& event) {
		dpp::snowflake user_id = event.command.usr.id;
		std::string platform = std::get<std::string>(event.get_parameter("platform"));
		std::string username = std::get<std::string>(event.get_parameter("id"));

		event.thinking();
		std::thread([event, user_id, platform, username]() {
			bool success = PlayerManager::change_info(user_id, platform + "_id", username);

			if (success) {
				event.edit_response("✅ Linked " + username + " as your " + platform + " account.");
			}
			else {
				event.edit_response("❌ Failed to link account. Make sure you have used `/register` first.");
			}
		}).detach();
	};

	// -- DELETE COMMAND --
	handlers["unlink"] = [bot_ptr](const dpp::slashcommand_t& event) {
		if (PlayerManager::delete_info(event.command.usr.id)) {
			event.reply("🗑️ Your profile and linked accounts have been deleted.");
		}
		else {
			event.reply("❌ No profile found to delete.");
		}
	};
}