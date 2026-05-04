#include "core/CommandRegistry.hpp"
#include "core/Localization.hpp"
#include "core/sqlite.hpp"
#include <memory>

/* * Consistent with your master database refactor,
 * changed from "db/players.db" to "db/master.db"
 */
Database& PlayerManager::get_db() {
    static Database instance("db/master.db");
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
            if (PlayerManager::exists(user_id)) {
                event.reply("ℹ️ You are already registered! Use `/whois` to see your profile.");
            }
            else {
                event.reply("❌ A database error occurred. Please try again later.");
            }
        }
    };

    // -- LINK COMMAND --
    handlers["link"] = [bot_ptr](const dpp::slashcommand_t& event) {
        dpp::snowflake user_id = event.command.usr.id;

        if (!PlayerManager::exists(user_id)) {
            event.reply("❌ You must `/register` before you can link accounts!");
            return;
        }

        std::string platform = std::get<std::string>(event.get_parameter("platform"));
        std::string username = std::get<std::string>(event.get_parameter("id"));

        event.thinking();
        // Capture by value for the thread
        std::thread([event, user_id, platform, username]() mutable {
            bool success = PlayerManager::change_info(user_id, platform + "_id", username);

            if (success) {
                event.edit_response("✅ Linked " + username + " as your " + platform + " account.");
            }
            else {
                event.edit_response("❌ Failed to link account. Ensure you have used `/register` first.");
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

    // -- WHOIS COMMAND --
    handlers["whois"] = [bot_ptr](const dpp::slashcommand_t& event) {
        dpp::snowflake target_id = 0;

        auto user_param = event.get_parameter("user");
        auto platform_param = event.get_parameter("platform");
        auto ign_param = event.get_parameter("ign");

        if (user_param.index() != 0) {
            target_id = std::get<dpp::snowflake>(user_param);
        }
        else if (platform_param.index() != 0 && ign_param.index() != 0) {
            std::string platform = std::get<std::string>(platform_param);
            std::string ign = std::get<std::string>(ign_param);

            std::transform(platform.begin(), platform.end(), platform.begin(), ::tolower);

            target_id = PlayerManager::find_by_platform(platform, ign);

            if (target_id == 0) {
                event.reply("❌ No player found with that " + platform + " name.");
                return;
            }
        }
        else {
            target_id = event.command.usr.id;
        }

        PlayerManager::send_profile_embed(*bot_ptr, event, target_id);
    };

    handlers["unlink_platform"] = [bot_ptr](const dpp::slashcommand_t& event) {
        dpp::snowflake user_id = event.command.usr.id;
        std::string platform = std::get<std::string>(event.get_parameter("platform"));

        if (PlayerManager::not_found(user_id)) {
            event.reply("❌ You don't have a profile! Use `/register` first.");
            return;
        }

        if (PlayerManager::unlink_platform(user_id, platform)) {
            // Clean up the name for the reply (e.g., tetrio_id -> tetrio)
            std::string display = platform;
            if (display.find("_id") != std::string::npos) display.erase(display.find("_id"));

            event.reply("🗑️ Successfully unlinked your " + display + " account.");
        }
        else {
            event.reply("❌ Failed to unlink. You might not have that platform connected.");
        }
    };

    handlers["language"] = [](const dpp::slashcommand_t& event) {
        const std::string language = std::get<std::string>(event.get_parameter("language"));

        if (language == "none") {
            if (!UserSettingsManager::clear_language(event.command.usr.id)) {
                event.reply(dpp::message("Could not update your language preference.").set_flags(dpp::m_ephemeral));
                return;
            }

            event.reply(
                dpp::message(localization::user_text(
                    event.command.guild_id,
                    event.command.usr.id,
                    "user.language.cleared"
                )).set_flags(dpp::m_ephemeral)
            );
            return;
        }

        if (!localization::is_supported_language(language)) {
            event.reply(
                dpp::message(localization::user_text(
                    event.command.guild_id,
                    event.command.usr.id,
                    "settings.language.unsupported"
                )).set_flags(dpp::m_ephemeral)
            );
            return;
        }

        if (!UserSettingsManager::set_language(event.command.usr.id, language)) {
            event.reply(dpp::message("Could not update your language preference.").set_flags(dpp::m_ephemeral));
            return;
        }

        event.reply(
            dpp::message(localization::user_text(
                event.command.guild_id,
                event.command.usr.id,
                "user.language.updated",
                { { "language", language } }
            )).set_flags(dpp::m_ephemeral)
        );
    };
}
