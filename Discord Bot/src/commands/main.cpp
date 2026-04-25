#include <dpp/dpp.h>
#include "core/Config.hpp"
#include "core/CommandRegistry.hpp"
#include "commands/Discord_Commands.hpp"

int main() {
    dpp::cluster bot(get_bot_token());

    register_fundamental_commands(bot);
    register_general_commands(bot);

    bot.on_ready([&bot](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_commands>()) {
            dpp::snowflake app_id = bot.me.id;

            bot.global_bulk_command_delete([&bot, app_id](const dpp::confirmation_callback_t& cc) {
                dpp::slashcommand codex("codex", "Show help message", app_id);
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
                bot.global_command_create(dpp::slashcommand("ping", "A simple ping command", app_id));
                bot.global_command_create(dpp::slashcommand("info", "Show bot information", app_id));
                }); // closes global_bulk_command_delete callback
        }
        }); // closes on_ready

    bot.on_autocomplete([](const dpp::autocomplete_t&) {});

    bot.on_slashcommand([](const dpp::slashcommand_t& event) {
        auto name = event.command.get_command_name();
        auto it = handlers.find(name);
        if (it != handlers.end()) {
            it->second(event);
        }
        else {
            event.reply("Unknown command.");
        }
        }); // closes on_slashcommand

    bot.start(dpp::st_wait);
}

