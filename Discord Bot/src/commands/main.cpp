#include <dpp/dpp.h>
#include "core/Config.hpp"
#include "core/CommandRegistry.hpp"
#include "commands/Discord_Commands.hpp"

int main() {
    dpp::cluster bot(get_bot_token());

    // 1. Register logic handlers into the global 'handlers' map
    register_fundamental_commands(bot);
    register_general_commands(bot);
    register_player_commands(bot);   // From Player.cpp 

    bot.on_ready([&bot](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_commands>()) {
            dpp::snowflake app_id = bot.me.id;

            // --- Existing Codex/Ping/Info ---
            dpp::slashcommand codex("codex", "Show help message", app_id);
            // ... (your existing codex options)
            codex.add_option(
                dpp::command_option(dpp::co_string, "category", "Help category", false)
                .add_choice(dpp::command_option_choice("fundamentals", "fundamentals"))
                .add_choice(dpp::command_option_choice("general", "general"))
                .add_choice(dpp::command_option_choice("player", "player"))
                .add_choice(dpp::command_option_choice("tetrio", "tetrio"))
                .add_choice(dpp::command_option_choice("brackets", "brackets"))
                .add_choice(dpp::command_option_choice("misc", "misc"))
            );
            codex.add_option(
                dpp::command_option(dpp::co_string, "command", "Specific command", false)
            );
            bot.global_command_create(codex);
            bot.global_command_create(dpp::slashcommand("pinging", "A simple ping command", app_id));
            bot.global_command_create(dpp::slashcommand("info", "Show bot information", app_id));
            // --- Player & Link Commands ---
            bot.global_command_create(dpp::slashcommand("register", "Initialize your tournament profile", app_id));

            dpp::slashcommand link("link", "Connect a Tetris platform account", app_id);
            link.add_option(
                dpp::command_option(dpp::co_string, "platform", "Platform to link", true)
                .add_choice(dpp::command_option_choice("TETR.IO", "tetrio"))
                .add_choice(dpp::command_option_choice("Jstris", "jstris"))
                .add_choice(dpp::command_option_choice("PPT2", "ppt2"))
                .add_choice(dpp::command_option_choice("TE:C", "tec"))
                .add_choice(dpp::command_option_choice("Tetra eSports", "tetra"))
                .add_choice(dpp::command_option_choice("Tetris: The Grand Master", "tgm"))
                .add_choice(dpp::command_option_choice("Classic", "ctwc"))
                .add_choice(dpp::command_option_choice("Other", "other")));
            link.add_option(dpp::command_option(dpp::co_string, "id", "Your In-Game ID or Username", true));
            bot.global_command_create(link);
            bot.global_command_create(dpp::slashcommand("unlink", "Wipe your local profile data", app_id));
            }
        });

    bot.on_slashcommand([](const dpp::slashcommand_t& event) {
        auto name = event.command.get_command_name();
        auto it = handlers.find(name);
        if (it != handlers.end()) {
            it->second(event);
        }
        else {
            event.reply("Unknown command.");
        }
        });

    bot.start(dpp::st_wait);
}
