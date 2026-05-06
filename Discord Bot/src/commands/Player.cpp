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
            event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "profile.init.ok"));
        }
        else {
            if (PlayerManager::exists(user_id)) {
                event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "profile.init.exists"));
            }
            else {
                event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "profile.init.failed"));
            }
        }
    };

    // -- LINK COMMAND --
    handlers["link"] = [bot_ptr](const dpp::slashcommand_t& event) {
        dpp::snowflake user_id = event.command.usr.id;

        if (!PlayerManager::exists(user_id)) {
            event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "profile.link.requires_profile"));
            return;
        }

        std::string platform = std::get<std::string>(event.get_parameter("platform"));
        std::string username = std::get<std::string>(event.get_parameter("id"));

        event.thinking();
        // Capture by value for the thread
        std::thread([event, user_id, platform, username]() mutable {
            bool success = PlayerManager::change_info(user_id, platform + "_id", username);

            if (success) {
                event.edit_response(localization::message_text(
                    event.command.guild_id,
                    event.command.usr.id,
                    "profile.link.ok",
                    { { "username", username }, { "platform", platform } }
                ));
            }
            else {
                event.edit_response(localization::message_text(event.command.guild_id, event.command.usr.id, "profile.link.failed"));
            }
            }).detach();
        };

    // -- DELETE COMMAND --
    handlers["unlink"] = [bot_ptr](const dpp::slashcommand_t& event) {
        if (PlayerManager::delete_info(event.command.usr.id)) {
            event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "profile.delete.ok"));
        }
        else {
            event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "profile.delete.missing"));
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
                event.reply(localization::message_text(
                    event.command.guild_id,
                    event.command.usr.id,
                    "profile.show.not_found",
                    { { "platform", platform } }
                ));
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
            event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "profile.unlink.requires_profile"));
            return;
        }

        if (PlayerManager::unlink_platform(user_id, platform)) {
            // Clean up the name for the reply (e.g., tetrio_id -> tetrio)
            std::string display = platform;
            if (display.find("_id") != std::string::npos) display.erase(display.find("_id"));

            event.reply(localization::message_text(
                event.command.guild_id,
                event.command.usr.id,
                "profile.unlink.ok",
                { { "platform", display } }
            ));
        }
        else {
            event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "profile.unlink.failed"));
        }
    };

    handlers["language"] = [](const dpp::slashcommand_t& event) {
        const std::string language = std::get<std::string>(event.get_parameter("language"));

        if (language == "none") {
            if (!UserSettingsManager::clear_language(event.command.usr.id)) {
                event.reply(dpp::message(localization::user_text(event.command.guild_id, event.command.usr.id, "profile.language.update_failed")).set_flags(dpp::m_ephemeral));
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
            event.reply(dpp::message(localization::user_text(event.command.guild_id, event.command.usr.id, "profile.language.update_failed")).set_flags(dpp::m_ephemeral));
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

    handlers["profile"] = [](const dpp::slashcommand_t& event) {
        const auto interaction = event.command.get_command_interaction();
        if (interaction.options.empty()) {
            event.reply(dpp::message(localization::user_text(event.command.guild_id, event.command.usr.id, "profile.choose_subcommand")).set_flags(dpp::m_ephemeral));
            return;
        }

        const std::string& subcommand = interaction.options.front().name;
        std::string target;
        if (subcommand == "init") {
            target = "register";
        }
        else if (subcommand == "show") {
            target = "whois";
        }
        else if (subcommand == "delete") {
            target = "unlink";
        }
        else if (subcommand == "unlink") {
            target = "unlink_platform";
        }
        else {
            target = subcommand;
        }

        const auto it = handlers.find(target);
        if (it != handlers.end()) {
            it->second(event);
            return;
        }

        event.reply(dpp::message(localization::user_text(event.command.guild_id, event.command.usr.id, "profile.unknown_subcommand")).set_flags(dpp::m_ephemeral));
    };
}
