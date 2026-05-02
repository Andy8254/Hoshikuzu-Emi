#include <dpp/dpp.h>
#include "core/Config.hpp"
#include "core/CommandRegistry.hpp"
#include "core/Fundamentals.hpp" //useless as (bleep)
#include "commands/Discord_Commands.hpp"
#include <exception>
#include <iostream>

const dpp::snowflake MOD_CHANNEL_ID = 1498301392073396234;

int main() {
    dpp::cluster bot(get_bot_token());

    //Register logic handlers into the global 'handlers' map
    register_fundamental_commands(bot);
    register_general_commands(bot);
    register_player_commands(bot);   // From Player.cpp
    register_tetrio_commands(bot);   // From Tetrio.cpp
    register_tournament_commands(bot);

    bot.on_ready([&bot](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_commands>()) {
            bot.message_create(dpp::message(MOD_CHANNEL_ID, get_hello_message()));
            dpp::snowflake app_id = bot.me.id;

            // Hello function
            dpp::slashcommand hello("hello", "Say Hello to Emi! Who knows she'll greet you back?", app_id);
            dpp::slashcommand privacy("privacy", "Show shortened Privacy Policy", app_id);
            bot.global_command_create(hello);
            bot.global_command_create(privacy);
            // --- Existing Codex/Ping/Info ---
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

            dpp::slashcommand whois("whois", "Find a player's profile", app_id);
            whois.add_option(dpp::command_option(dpp::co_user, "user", "Search by Discord User", false));
            whois.add_option(
                dpp::command_option(dpp::co_string, "platform", "Platform to search in", false)
                .add_choice(dpp::command_option_choice("TETR.IO", "tetrio_id"))
                .add_choice(dpp::command_option_choice("Jstris", "jstris_id"))
                .add_choice(dpp::command_option_choice("PPT2", "ppt2_id"))
                .add_choice(dpp::command_option_choice("TE:C", "tec_id"))
                .add_choice(dpp::command_option_choice("Tetra eSports", "tetra_id"))
                .add_choice(dpp::command_option_choice("Tetris: The Grand Master", "tgm_id"))
                .add_choice(dpp::command_option_choice("Classic", "ctwc_id"))
                .add_choice(dpp::command_option_choice("Other", "other_id"))
            );
            whois.add_option(dpp::command_option(dpp::co_string, "ign", "In-game name to search for", false));
            bot.global_command_create(whois);

            dpp::slashcommand unlink_p("unlink_platform", "Remove a specific linked account", app_id);
            unlink_p.add_option(
                dpp::command_option(dpp::co_string, "platform", "Platform to disconnect", true)
                .add_choice(dpp::command_option_choice("TETR.IO", "tetrio_id"))
                .add_choice(dpp::command_option_choice("Jstris", "jstris_id"))
                .add_choice(dpp::command_option_choice("PPT2", "ppt2_id"))
                .add_choice(dpp::command_option_choice("TE:C", "tec_id"))
                .add_choice(dpp::command_option_choice("Tetra eSports", "tetra_id"))
                .add_choice(dpp::command_option_choice("Tetris: The Grand Master", "tgm_id"))
                .add_choice(dpp::command_option_choice("Classic", "ctwc_id"))
                .add_choice(dpp::command_option_choice("Other", "other_id"))
            );
            bot.global_command_create(unlink_p);
            
            dpp::slashcommand tetrio_cmd("tetrio", "Show a TETR.IO profile", app_id);
            tetrio_cmd.add_option(
                dpp::command_option(
                    dpp::co_string,
                    "username",
                    "TETR.IO username. Leave empty to use your linked account.",
                    false
                )
            );
            bot.global_command_create(tetrio_cmd);

            // -- Guild Commands --
            dpp::slashcommand tournament("tournament", "Tournament management and player workflows", app_id);

            dpp::command_option create(dpp::co_sub_command, "create", "Create a tournament");
            create.add_option(dpp::command_option(dpp::co_string, "name", "Tournament name", true));
            create.add_option(dpp::command_option(dpp::co_string, "game", "Game or ruleset", false));
            tournament.add_option(create);

            dpp::command_option edit(dpp::co_sub_command, "edit", "Edit a tournament");
            edit.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            edit.add_option(dpp::command_option(dpp::co_string, "name", "New tournament name", false));
            edit.add_option(dpp::command_option(dpp::co_string, "game", "New game or ruleset", false));
            edit.add_option(dpp::command_option(dpp::co_string, "status", "New status", false)
                .add_choice(dpp::command_option_choice("Open", "open"))
                .add_choice(dpp::command_option_choice("Check-in", "checkin"))
                .add_choice(dpp::command_option_choice("Running", "running"))
                .add_choice(dpp::command_option_choice("Closed", "closed")));
            tournament.add_option(edit);

            dpp::command_option del(dpp::co_sub_command, "delete", "Delete a tournament");
            del.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            tournament.add_option(del);

            dpp::command_option tournament_register(dpp::co_sub_command, "register", "Register for a tournament");
            tournament_register.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            tournament_register.add_option(dpp::command_option(dpp::co_string, "username", "Tournament username", true));
            tournament.add_option(tournament_register);

            dpp::command_option checkin(dpp::co_sub_command, "checkin", "Check in for a tournament");
            checkin.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            checkin.add_option(dpp::command_option(dpp::co_string, "username", "Tournament username", true));
            checkin.add_option(dpp::command_option(dpp::co_integer, "closes_at", "Unix timestamp for check-in close", false));
            checkin.add_option(dpp::command_option(dpp::co_integer, "grace_time", "Grace time in seconds", false));
            tournament.add_option(checkin);

            dpp::command_option participants(dpp::co_sub_command, "participants", "List tournament participants");
            participants.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            tournament.add_option(participants);

            dpp::command_option seed(dpp::co_sub_command, "seed", "Seed checked-in players");
            seed.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            seed.add_option(dpp::command_option(dpp::co_string, "mode", "Seeding mode", false)
                .add_choice(dpp::command_option_choice("General", "general"))
                .add_choice(dpp::command_option_choice("TETR.IO", "tetrio")));
            tournament.add_option(seed);

            dpp::command_option set_staff_role(dpp::co_sub_command, "set_staff_role", "Set tournament staff role");
            set_staff_role.add_option(dpp::command_option(dpp::co_role, "role", "Tournament staff role", true));
            tournament.add_option(set_staff_role);

            dpp::command_option set_admin_role(dpp::co_sub_command, "set_admin_role", "Set tournament admin role");
            set_admin_role.add_option(dpp::command_option(dpp::co_role, "role", "Tournament admin role", true));
            tournament.add_option(set_admin_role);

            for (const auto& guild_id : event.guilds) {
                bot.guild_command_create(tournament, guild_id);
            }

        }
    });

    bot.on_slashcommand([](const dpp::slashcommand_t& event) {
        try {
            auto name = event.command.get_command_name();
            auto it = handlers.find(name);
            if (it != handlers.end()) {
                it->second(event);
            }
            else {
                event.reply("I'm sorry, I haven't heard about that command... (´;ω;｀)");
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Slash command failed: " << e.what() << std::endl;
            event.reply(
                dpp::message(std::string("Command failed: ") + e.what())
                .set_flags(dpp::m_ephemeral)
            );
        }
        catch (...) {
            std::cerr << "Slash command failed with an unknown exception." << std::endl;
            event.reply(
                dpp::message("Command failed with an unknown error.")
                .set_flags(dpp::m_ephemeral)
            );
        }
    });

    bot.start(dpp::st_wait);
}

//comment
