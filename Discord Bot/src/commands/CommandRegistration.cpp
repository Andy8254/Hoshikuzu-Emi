#include "commands/CommandRegistration.hpp"
#include "core/Fundamentals.hpp"
#include "core/sqlite.hpp"

namespace {
    dpp::command_option& add_platform_choices(dpp::command_option& option) {
        return option
            .add_choice(dpp::command_option_choice("TETR.IO", "tetrio"))
            .add_choice(dpp::command_option_choice("Jstris", "jstris"))
            .add_choice(dpp::command_option_choice("Puyo Puyo Tetris 2", "ppt2"))
            .add_choice(dpp::command_option_choice("Tetris Effect: Connected", "tec"))
            .add_choice(dpp::command_option_choice("Tetra eSports", "tetra"))
            .add_choice(dpp::command_option_choice("Classic Tetris", "ctwc"))
            .add_choice(dpp::command_option_choice("Other", "other"));
    }

    dpp::command_option& add_platform_id_choices(dpp::command_option& option) {
        return option
            .add_choice(dpp::command_option_choice("TETR.IO", "tetrio_id"))
            .add_choice(dpp::command_option_choice("Jstris", "jstris_id"))
            .add_choice(dpp::command_option_choice("Puyo Puyo Tetris 2", "ppt2_id"))
            .add_choice(dpp::command_option_choice("Tetris Effect: Connected", "tec_id"))
            .add_choice(dpp::command_option_choice("Tetra eSports", "tetra_id"))
            .add_choice(dpp::command_option_choice("Classic Tetris", "ctwc_id"))
            .add_choice(dpp::command_option_choice("Other", "other_id"));
    }

    dpp::command_option& add_format_choices(dpp::command_option& option) {
        return option
            .add_choice(dpp::command_option_choice("Single elimination", "single_elimination"))
            .add_choice(dpp::command_option_choice("Double elimination", "double_elimination"))
            .add_choice(dpp::command_option_choice("Round robin", "round_robin")) //to be implemented in BETA
            .add_choice(dpp::command_option_choice("Swiss", "swiss")); //to be implemented in BETA
    }

    dpp::command_option& add_language_choices(dpp::command_option& option, bool include_none = false) {
        option
            .add_choice(dpp::command_option_choice("English (GB)", "EN-gb"))
            .add_choice(dpp::command_option_choice("Korean", "KO-kr"));

        if (include_none) {
            option.add_choice(dpp::command_option_choice("None", "none"));
        }

        return option;
}

    dpp::command_option& add_tetrio_rank_choices(dpp::command_option& option) {
        return option
            .add_choice(dpp::command_option_choice("X+", "X+"))
            .add_choice(dpp::command_option_choice("X", "X"))
            .add_choice(dpp::command_option_choice("U", "U"))
            .add_choice(dpp::command_option_choice("SS", "SS"))
            .add_choice(dpp::command_option_choice("S+", "S+"))
            .add_choice(dpp::command_option_choice("S", "S"))
            .add_choice(dpp::command_option_choice("S-", "S-"))
            .add_choice(dpp::command_option_choice("A+", "A+"))
            .add_choice(dpp::command_option_choice("A", "A"))
            .add_choice(dpp::command_option_choice("A-", "A-"))
            .add_choice(dpp::command_option_choice("B+", "B+"))
            .add_choice(dpp::command_option_choice("B", "B"))
            .add_choice(dpp::command_option_choice("B-", "B-"))
            .add_choice(dpp::command_option_choice("C+", "C+"))
            .add_choice(dpp::command_option_choice("C", "C"))
            .add_choice(dpp::command_option_choice("C-", "C-"))
            .add_choice(dpp::command_option_choice("D+", "D+"))
            .add_choice(dpp::command_option_choice("D", "D"))
            .add_choice(dpp::command_option_choice("Unranked", "Z"));
    }
}

void register_discord_commands(dpp::cluster& bot, dpp::snowflake mod_channel_id) {
    bot.on_ready([&bot, mod_channel_id](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_commands>()) {
            bot.message_create(dpp::message(mod_channel_id, get_hello_message()));
            dpp::snowflake app_id = bot.me.id;

            dpp::slashcommand bot_command("bot", "Bot help and information", app_id);
            dpp::command_option bot_help(dpp::co_sub_command, "help", "Show help message");
            bot_help.add_option(
                dpp::command_option(dpp::co_string, "category", "Help category", false)
                .add_choice(dpp::command_option_choice("bot", "bot"))
                .add_choice(dpp::command_option_choice("profile", "profile"))
                .add_choice(dpp::command_option_choice("settings", "settings"))
                .add_choice(dpp::command_option_choice("moderation", "moderation"))
                .add_choice(dpp::command_option_choice("tournament", "tournament"))
                .add_choice(dpp::command_option_choice("tournament_bracket", "tournament_bracket"))
                .add_choice(dpp::command_option_choice("tournament_config", "tournament_config"))
                .add_choice(dpp::command_option_choice("panels", "panels"))
            );
            bot_help.add_option(dpp::command_option(dpp::co_string, "command", "Specific command", false));
            bot_command.add_option(bot_help);
            bot_command.add_option(dpp::command_option(dpp::co_sub_command, "hello", "Say hello to Emi"));
            bot_command.add_option(dpp::command_option(dpp::co_sub_command, "ping", "Check bot latency"));
            bot_command.add_option(dpp::command_option(dpp::co_sub_command, "info", "Show bot information"));
            bot_command.add_option(dpp::command_option(dpp::co_sub_command, "privacy", "Show shortened Privacy Policy"));
            bot.global_command_create(bot_command);

            dpp::slashcommand profile("profile", "Player profile and linked accounts", app_id);
            profile.add_option(dpp::command_option(dpp::co_sub_command, "init", "Initialize your tournament profile"));

            dpp::command_option profile_link(dpp::co_sub_command, "link", "Connect a Tetris platform account");
            dpp::command_option link_platform(dpp::co_string, "platform", "Platform to link", true);
            profile_link.add_option(add_platform_choices(link_platform));
            profile_link.add_option(dpp::command_option(dpp::co_string, "id", "Your in-game ID or username", true));
            profile.add_option(profile_link);

            dpp::command_option profile_unlink(dpp::co_sub_command, "unlink", "Remove a specific linked account");
            dpp::command_option unlink_platform(dpp::co_string, "platform", "Platform to disconnect", true);
            profile_unlink.add_option(add_platform_id_choices(unlink_platform));
            profile.add_option(profile_unlink);

            dpp::command_option profile_show(dpp::co_sub_command, "show", "Find a player's profile");
            profile_show.add_option(dpp::command_option(dpp::co_user, "user", "Search by Discord user", false));
            dpp::command_option show_platform(dpp::co_string, "platform", "Platform to search in", false);
            profile_show.add_option(add_platform_id_choices(show_platform));
            profile_show.add_option(dpp::command_option(dpp::co_string, "ign", "In-game name to search for", false));
            profile.add_option(profile_show);

            profile.add_option(dpp::command_option(dpp::co_sub_command, "delete", "Wipe your local profile data"));

            dpp::command_option profile_language(dpp::co_sub_command, "language", "Set your personal language preference");
            dpp::command_option user_language(dpp::co_string, "language", "Language, or none to use the server default", true);
            profile_language.add_option(add_language_choices(user_language, true));
            profile.add_option(profile_language);

            dpp::command_option profile_tetrio(dpp::co_sub_command, "tetrio", "Show a TETR.IO profile");
            profile_tetrio.add_option(dpp::command_option(dpp::co_string, "username", "TETR.IO username. Leave empty to use your linked account.", false));
            profile.add_option(profile_tetrio);
            bot.global_command_create(profile);

            // -- Guild Commands --
            dpp::slashcommand settings("settings", "Server settings", app_id);

            dpp::command_option settings_show(dpp::co_sub_command, "show", "Show server settings");
            settings.add_option(settings_show);

            dpp::command_option settings_set_admin(dpp::co_sub_command, "set_admin_role", "Owner-only: set server admin role");
            settings_set_admin.add_option(dpp::command_option(dpp::co_role, "role", "Server admin role", true));
            settings.add_option(settings_set_admin);

            dpp::command_option settings_set_mod(dpp::co_sub_command, "set_moderator_role", "Admin-only: set server moderator role");
            settings_set_mod.add_option(dpp::command_option(dpp::co_role, "role", "Server moderator role", true));
            settings.add_option(settings_set_mod);

            dpp::command_option settings_set_staff(dpp::co_sub_command, "set_staff_role", "Moderator-only: set server staff role");
            settings_set_staff.add_option(dpp::command_option(dpp::co_role, "role", "Server staff role", true));
            settings.add_option(settings_set_staff);

            dpp::command_option settings_language(dpp::co_sub_command, "language", "Set server primary language");
            dpp::command_option settings_language_value(dpp::co_string, "language", "Language", true);
            settings_language.add_option(add_language_choices(settings_language_value));
            settings.add_option(settings_language);

            dpp::command_option settings_secondary_language(dpp::co_sub_command, "secondary_language", "Set server secondary language");
            dpp::command_option settings_secondary_language_value(dpp::co_string, "language", "Language, or none to disable", true);
            settings_secondary_language.add_option(add_language_choices(settings_secondary_language_value, true));
            settings.add_option(settings_secondary_language);

            dpp::command_option settings_modlog_set(dpp::co_sub_command, "modlog_set", "Set moderation log channel");
            settings_modlog_set.add_option(dpp::command_option(dpp::co_channel, "channel", "Moderation log channel", true));
            settings.add_option(settings_modlog_set);

            dpp::command_option settings_modlog_clear(dpp::co_sub_command, "modlog_clear", "Clear moderation log channel");
            settings.add_option(settings_modlog_clear);

            dpp::slashcommand mod("mod", "Manual moderation tools", app_id);

            dpp::command_option mod_warn(dpp::co_sub_command, "warn", "Record a warning");
            mod_warn.add_option(dpp::command_option(dpp::co_user, "user", "Target user", true));
            mod_warn.add_option(dpp::command_option(dpp::co_string, "reason", "Reason", true));
            mod.add_option(mod_warn);

            dpp::command_option mod_note(dpp::co_sub_command, "note", "Record a staff note");
            mod_note.add_option(dpp::command_option(dpp::co_user, "user", "Target user", true));
            mod_note.add_option(dpp::command_option(dpp::co_string, "note", "Note", true));
            mod.add_option(mod_note);

            dpp::command_option mod_history(dpp::co_sub_command, "history", "Show moderation history");
            mod_history.add_option(dpp::command_option(dpp::co_user, "user", "Target user", true));
            mod.add_option(mod_history);

            dpp::command_option mod_timeout(dpp::co_sub_command, "timeout", "Timeout a user");
            mod_timeout.add_option(dpp::command_option(dpp::co_user, "user", "Target user", true));
            mod_timeout.add_option(dpp::command_option(dpp::co_integer, "duration", "Duration in seconds, max 2419200", true));
            mod_timeout.add_option(dpp::command_option(dpp::co_string, "reason", "Reason", true));
            mod.add_option(mod_timeout);

            dpp::command_option mod_clear_timeout(dpp::co_sub_command, "clear_timeout", "Clear a user timeout");
            mod_clear_timeout.add_option(dpp::command_option(dpp::co_user, "user", "Target user", true));
            mod_clear_timeout.add_option(dpp::command_option(dpp::co_string, "reason", "Reason", false));
            mod.add_option(mod_clear_timeout);

            dpp::command_option mod_kick(dpp::co_sub_command, "kick", "Kick a user");
            mod_kick.add_option(dpp::command_option(dpp::co_user, "user", "Target user", true));
            mod_kick.add_option(dpp::command_option(dpp::co_string, "reason", "Reason", true));
            mod.add_option(mod_kick);

            dpp::command_option mod_ban(dpp::co_sub_command, "ban", "Ban a user");
            mod_ban.add_option(dpp::command_option(dpp::co_user, "user", "Target user", true));
            mod_ban.add_option(dpp::command_option(dpp::co_string, "reason", "Reason", true));
            mod.add_option(mod_ban);

            dpp::command_option mod_unban(dpp::co_sub_command, "unban", "Unban a user");
            mod_unban.add_option(dpp::command_option(dpp::co_user, "user", "Target user", true));
            mod_unban.add_option(dpp::command_option(dpp::co_string, "reason", "Reason", false));
            mod.add_option(mod_unban);

            dpp::slashcommand tournament("tournament", "Tournament management and player workflows", app_id);

            dpp::command_option create(dpp::co_sub_command, "create", "Create a tournament");
            create.add_option(dpp::command_option(dpp::co_string, "name", "Tournament name", true));
            dpp::command_option create_game(dpp::co_string, "game", "Platform or game", false);
            create.add_option(add_platform_choices(create_game));
            dpp::command_option create_format(dpp::co_string, "format", "Tournament format", false);
            create.add_option(add_format_choices(create_format));
            tournament.add_option(create);

            dpp::command_option edit(dpp::co_sub_command, "edit", "Edit a tournament");
            edit.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            edit.add_option(dpp::command_option(dpp::co_string, "name", "New tournament name", false));
            dpp::command_option edit_game(dpp::co_string, "game", "New platform or game", false);
            edit.add_option(add_platform_choices(edit_game));
            dpp::command_option edit_format(dpp::co_string, "format", "New tournament format", false);
            edit.add_option(add_format_choices(edit_format));
            edit.add_option(dpp::command_option(dpp::co_string, "status", "New status", false)
                .add_choice(dpp::command_option_choice("Open", "open"))
                .add_choice(dpp::command_option_choice("Check-in", "checkin"))
                .add_choice(dpp::command_option_choice("Running", "running"))
                .add_choice(dpp::command_option_choice("Closed", "closed")));
            tournament.add_option(edit);

            dpp::command_option del(dpp::co_sub_command, "delete", "Delete a tournament");
            del.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            tournament.add_option(del);

            dpp::command_option clear(dpp::co_sub_command, "clear", "Expunge tournament module data");
            clear.add_option(dpp::command_option(dpp::co_string, "confirm", "Type RESET to confirm", true));
            tournament.add_option(clear);

            dpp::command_option info(dpp::co_sub_command, "info", "Show tournament information");
            info.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            tournament.add_option(info);

            dpp::command_option staff_info(dpp::co_sub_command, "staff_info", "Show staff tournament information");
            staff_info.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            tournament.add_option(staff_info);

            dpp::command_option registration_open(dpp::co_sub_command, "registration_open", "Open tournament registration");
            registration_open.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            tournament.add_option(registration_open);

            dpp::command_option registration_close(dpp::co_sub_command, "registration_close", "Close tournament registration");
            registration_close.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            tournament.add_option(registration_close);

            dpp::command_option checkin_open(dpp::co_sub_command, "checkin_open", "Open tournament check-in");
            checkin_open.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            checkin_open.add_option(dpp::command_option(dpp::co_integer, "closes_at", "Unix timestamp for check-in close", true));
            checkin_open.add_option(dpp::command_option(dpp::co_integer, "grace_time", "Grace time in seconds", false));
            tournament.add_option(checkin_open);

            dpp::command_option checkin_close(dpp::co_sub_command, "checkin_close", "Close tournament check-in");
            checkin_close.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            tournament.add_option(checkin_close);

            dpp::command_option tournament_register(dpp::co_sub_command, "register", "Register for a tournament");
            tournament_register.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            tournament_register.add_option(dpp::command_option(dpp::co_string, "username", "Tournament username", false));
            tournament_register.add_option(dpp::command_option(dpp::co_boolean, "abort", "Unregister instead", false));
            tournament_register.add_option(dpp::command_option(dpp::co_user, "user", "Staff target player", false));
            tournament.add_option(tournament_register);

            dpp::command_option checkin(dpp::co_sub_command, "checkin", "Check in for a tournament");
            checkin.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            checkin.add_option(dpp::command_option(dpp::co_string, "username", "Tournament username", false));
            checkin.add_option(dpp::command_option(dpp::co_boolean, "abort", "Undo check-in instead", false));
            checkin.add_option(dpp::command_option(dpp::co_user, "user", "Staff target player", false));
            tournament.add_option(checkin);

            dpp::command_option call_staff(dpp::co_sub_command, "call_staff", "Call tournament staff for a match");
            call_staff.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            call_staff.add_option(dpp::command_option(dpp::co_integer, "match_id", "Match ID", true));
            call_staff.add_option(dpp::command_option(dpp::co_string, "reason", "What needs staff attention", false));
            tournament.add_option(call_staff);

            dpp::command_option participants(dpp::co_sub_command, "participants", "List tournament participants");
            participants.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            tournament.add_option(participants);

            dpp::command_option seed(dpp::co_sub_command, "seed", "Seed checked-in players");
            seed.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            seed.add_option(dpp::command_option(dpp::co_string, "mode", "Seeding mode", false)
                .add_choice(dpp::command_option_choice("General", "general"))
                .add_choice(dpp::command_option_choice("TETR.IO", "tetrio")));
            tournament.add_option(seed);

            dpp::command_option bracket(dpp::co_sub_command_group, "bracket", "Bracket and match operations");

            dpp::command_option bracket_generate(dpp::co_sub_command, "generate", "Generate a bracket and queue current match threads");
            bracket_generate.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            dpp::command_option bracket_type(dpp::co_string, "type", "Bracket type, or omit to use tournament format", false);
            bracket_generate.add_option(add_format_choices(bracket_type));
            bracket.add_option(bracket_generate);

            dpp::command_option bracket_current(dpp::co_sub_command, "current", "Show current playable matches");
            bracket_current.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            bracket.add_option(bracket_current);

            dpp::command_option bracket_round(dpp::co_sub_command, "round", "Show matches in a specific round");
            bracket_round.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            bracket_round.add_option(dpp::command_option(dpp::co_integer, "round", "Round number, starting from 1", true));
            bracket.add_option(bracket_round);

            dpp::command_option bracket_match(dpp::co_sub_command, "match", "Show one match");
            bracket_match.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            bracket_match.add_option(dpp::command_option(dpp::co_integer, "match_id", "Match ID", true));
            bracket.add_option(bracket_match);

            dpp::command_option bracket_report(dpp::co_sub_command, "report", "Report a match score");
            bracket_report.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            bracket_report.add_option(dpp::command_option(dpp::co_integer, "match_id", "Match ID", true));
            bracket_report.add_option(dpp::command_option(dpp::co_integer, "score_a", "Player A score", true));
            bracket_report.add_option(dpp::command_option(dpp::co_integer, "score_b", "Player B score", true));
            bracket.add_option(bracket_report);

            dpp::command_option bracket_correct_report(dpp::co_sub_command, "correct_report", "Admin-only correction for a completed match");
            bracket_correct_report.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            bracket_correct_report.add_option(dpp::command_option(dpp::co_integer, "match_id", "Match ID", true));
            bracket_correct_report.add_option(dpp::command_option(dpp::co_integer, "score_a", "Corrected Player A score", true));
            bracket_correct_report.add_option(dpp::command_option(dpp::co_integer, "score_b", "Corrected Player B score", true));
            bracket_correct_report.add_option(dpp::command_option(dpp::co_string, "confirm", "Type CORRECT to confirm", true));
            bracket.add_option(bracket_correct_report);

            dpp::command_option bracket_forfeit(dpp::co_sub_command, "forfeit", "Staff records a player forfeit/DQ in a match");
            bracket_forfeit.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            bracket_forfeit.add_option(dpp::command_option(dpp::co_integer, "match_id", "Match ID", true));
            bracket_forfeit.add_option(dpp::command_option(dpp::co_user, "player", "Player to forfeit", true));
            bracket_forfeit.add_option(dpp::command_option(dpp::co_string, "reason", "Forfeit/DQ reason", false));
            bracket.add_option(bracket_forfeit);

            dpp::command_option bracket_resolve_no_shows(dpp::co_sub_command, "resolve_no_shows", "Resolve due match no-shows and auto-DQs");
            bracket_resolve_no_shows.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            bracket.add_option(bracket_resolve_no_shows);

            dpp::command_option bracket_threads(dpp::co_sub_command, "threads", "Create Discord threads for current or round matches");
            bracket_threads.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            bracket_threads.add_option(dpp::command_option(dpp::co_integer, "round", "Round number, or omit for current matches", false));
            bracket_threads.add_option(dpp::command_option(dpp::co_boolean, "buttons", "Add check-in/report buttons", false));
            bracket.add_option(bracket_threads);

            dpp::command_option stream_assign(dpp::co_sub_command, "stream_assign", "Assign a match to stream");
            stream_assign.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            stream_assign.add_option(dpp::command_option(dpp::co_integer, "match_id", "Match ID", true));
            bracket.add_option(stream_assign);

            dpp::command_option stream_clear(dpp::co_sub_command, "stream_clear", "Clear a streamed match assignment");
            stream_clear.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            stream_clear.add_option(dpp::command_option(dpp::co_integer, "match_id", "Match ID", true));
            bracket.add_option(stream_clear);

            dpp::command_option stream_list(dpp::co_sub_command, "stream_list", "Show streamed matches");
            stream_list.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            bracket.add_option(stream_list);

            dpp::command_option standings(dpp::co_sub_command, "standings", "Show Swiss or round-robin standings");
            standings.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            bracket.add_option(standings);

            dpp::command_option bracket_svg(dpp::co_sub_command, "svg", "Export the bracket as SVG");
            bracket_svg.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            bracket.add_option(bracket_svg);

            dpp::command_option match_svg(dpp::co_sub_command, "match_svg", "Export one match as SVG");
            match_svg.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            match_svg.add_option(dpp::command_option(dpp::co_integer, "match_id", "Match ID", true));
            bracket.add_option(match_svg);

            tournament.add_option(bracket);

            dpp::command_option config(dpp::co_sub_command_group, "config", "Configure tournament module settings");

            dpp::command_option config_roles(dpp::co_sub_command, "roles", "Show tournament role configuration");
            config.add_option(config_roles);

            dpp::command_option config_set_staff_role(dpp::co_sub_command, "set_staff_role", "Set tournament staff role");
            config_set_staff_role.add_option(dpp::command_option(dpp::co_role, "role", "Tournament staff role", true));
            config.add_option(config_set_staff_role);

            dpp::command_option config_set_admin_role(dpp::co_sub_command, "set_admin_role", "Set tournament admin role");
            config_set_admin_role.add_option(dpp::command_option(dpp::co_role, "role", "Tournament admin role", true));
            config.add_option(config_set_admin_role);

            dpp::command_option config_clear_staff_role(dpp::co_sub_command, "clear_staff_role", "Clear tournament staff role");
            config.add_option(config_clear_staff_role);

            dpp::command_option config_clear_admin_role(dpp::co_sub_command, "clear_admin_role", "Clear tournament admin role");
            config.add_option(config_clear_admin_role);

            dpp::command_option config_set_channel(dpp::co_sub_command, "set_channel", "Set tournament registration/check-in channel");
            config_set_channel.add_option(dpp::command_option(dpp::co_channel, "channel", "Tournament channel", true));
            config.add_option(config_set_channel);

            dpp::command_option config_clear_channel(dpp::co_sub_command, "clear_channel", "Clear tournament registration/check-in channel");
            config.add_option(config_clear_channel);

            dpp::command_option config_log_channel_assign(dpp::co_sub_command, "log_channel_assign", "Set tournament audit log channel");
            config_log_channel_assign.add_option(dpp::command_option(dpp::co_channel, "channel", "Tournament log channel", true));
            config.add_option(config_log_channel_assign);

            dpp::command_option config_log_channel_clear(dpp::co_sub_command, "log_channel_clear", "Clear tournament audit log channel");
            config.add_option(config_log_channel_clear);

            dpp::command_option config_set_format(dpp::co_sub_command, "set_format", "Set a tournament's default format");
            config_set_format.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            dpp::command_option config_format(dpp::co_string, "format", "Tournament format", true);
            config_set_format.add_option(add_format_choices(config_format));
            config.add_option(config_set_format);

            dpp::command_option tetrio_restrictions(dpp::co_sub_command, "tetrio_restrictions", "Set TETR.IO rank/TR registration restrictions");
            tetrio_restrictions.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            dpp::command_option current_rank_min(dpp::co_string, "current_rank_min", "Lowest current rank allowed", false);
            tetrio_restrictions.add_option(add_tetrio_rank_choices(current_rank_min));
            dpp::command_option current_rank_max(dpp::co_string, "current_rank_max", "Highest current rank allowed", false);
            tetrio_restrictions.add_option(add_tetrio_rank_choices(current_rank_max));
            dpp::command_option top_rank_min(dpp::co_string, "top_rank_min", "Lowest top rank allowed", false);
            tetrio_restrictions.add_option(add_tetrio_rank_choices(top_rank_min));
            dpp::command_option top_rank_max(dpp::co_string, "top_rank_max", "Highest top rank allowed", false);
            tetrio_restrictions.add_option(add_tetrio_rank_choices(top_rank_max));
            tetrio_restrictions.add_option(dpp::command_option(dpp::co_number, "tr_min", "Minimum TR allowed", false));
            tetrio_restrictions.add_option(dpp::command_option(dpp::co_number, "tr_max", "Maximum TR allowed", false));
            tetrio_restrictions.add_option(dpp::command_option(dpp::co_boolean, "allow_unranked", "Allow unranked players; TR still affects seeding when present", false));
            tetrio_restrictions.add_option(dpp::command_option(dpp::co_boolean, "clear", "Clear all TETR.IO restrictions", false));
            config.add_option(tetrio_restrictions);

            dpp::command_option ruleset_show(dpp::co_sub_command, "ruleset_show", "Show tournament rulesets");
            ruleset_show.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            config.add_option(ruleset_show);

            dpp::command_option ruleset_set_primary(dpp::co_sub_command, "ruleset_set_primary", "Set the default match ruleset");
            ruleset_set_primary.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            ruleset_set_primary.add_option(dpp::command_option(dpp::co_integer, "first_to", "First-to score", true));
            ruleset_set_primary.add_option(dpp::command_option(dpp::co_string, "deuce", "Deuce handling", false)
                .add_choice(dpp::command_option_choice("Off", "none"))
                .add_choice(dpp::command_option_choice("Win by difference", "win_by_diff"))
                .add_choice(dpp::command_option_choice("Golden point", "golden_point")));
            ruleset_set_primary.add_option(dpp::command_option(dpp::co_integer, "win_by", "Required score difference", false));
            ruleset_set_primary.add_option(dpp::command_option(dpp::co_integer, "score_cap", "Score cap, or 0 for no cap", false));
            ruleset_set_primary.add_option(dpp::command_option(dpp::co_boolean, "allow_draw", "Allow drawn matches", false));
            config.add_option(ruleset_set_primary);

            dpp::command_option ruleset_set_secondary(dpp::co_sub_command, "ruleset_set_secondary", "Set secondary match rules");
            ruleset_set_secondary.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            ruleset_set_secondary.add_option(dpp::command_option(dpp::co_string, "trigger", "When secondary rules apply", true)
                .add_choice(dpp::command_option_choice("Top 8", "top8"))
                .add_choice(dpp::command_option_choice("Grand Finals", "grand_finals")));
            ruleset_set_secondary.add_option(dpp::command_option(dpp::co_integer, "first_to", "First-to score", true));
            ruleset_set_secondary.add_option(dpp::command_option(dpp::co_string, "deuce", "Deuce handling", false)
                .add_choice(dpp::command_option_choice("Off", "none"))
                .add_choice(dpp::command_option_choice("Win by difference", "win_by_diff"))
                .add_choice(dpp::command_option_choice("Golden point", "golden_point")));
            ruleset_set_secondary.add_option(dpp::command_option(dpp::co_integer, "win_by", "Required score difference", false));
            ruleset_set_secondary.add_option(dpp::command_option(dpp::co_integer, "score_cap", "Score cap, or 0 for no cap", false));
            ruleset_set_secondary.add_option(dpp::command_option(dpp::co_boolean, "allow_draw", "Allow drawn matches", false));
            config.add_option(ruleset_set_secondary);

            dpp::command_option ruleset_clear_secondary(dpp::co_sub_command, "ruleset_clear_secondary", "Disable secondary match rules");
            ruleset_clear_secondary.add_option(dpp::command_option(dpp::co_integer, "id", "Tournament ID", true));
            config.add_option(ruleset_clear_secondary);

            tournament.add_option(config);

            for (const auto& guild_id : event.guilds) {
                bot.guild_get(guild_id, [guild_id](const dpp::confirmation_callback_t& cb) {
                    if (!cb.is_error()) {
                        const dpp::guild guild = std::get<dpp::guild>(cb.value);
                        ServerSettingsManager::set_owner_if_empty(guild_id, guild.owner_id);
                    }
                });
                bot.guild_command_create(settings, guild_id);
                bot.guild_command_create(mod, guild_id);
                bot.guild_command_create(tournament, guild_id);
            }

        }
    });
}

