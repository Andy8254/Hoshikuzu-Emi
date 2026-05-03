#include <dpp/dpp.h>
#include "core/Config.hpp"
#include "core/CommandRegistry.hpp"
#include "core/Fundamentals.hpp" //useless as (bleep)
#include "core/sqlite.hpp"
#include "commands/Discord_Commands.hpp"
#include "tournament/bracket/MatchStore.hpp"
#include <ctime>
#include <exception>
#include <iostream>
#include <sstream>
#include <vector>

const dpp::snowflake MOD_CHANNEL_ID = 1498301392073396234;

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

    dpp::command_option& add_format_choices(dpp::command_option& option) {
        return option
            .add_choice(dpp::command_option_choice("Single elimination", "single_elimination"))
            .add_choice(dpp::command_option_choice("Double elimination", "double_elimination"))
            .add_choice(dpp::command_option_choice("Round robin", "round_robin"))
            .add_choice(dpp::command_option_choice("Swiss", "swiss"));
    }

    std::vector<std::string> split_custom_id(const std::string& value) {
        std::vector<std::string> parts;
        std::stringstream stream(value);
        std::string item;
        while (std::getline(stream, item, ':')) {
            parts.push_back(item);
        }
        return parts;
    }

    void send_tournament_button_log(
        dpp::cluster& bot,
        const dpp::button_click_t& event,
        const std::string& title,
        const std::string& description,
        int color = 0xf0b429
    ) {
        const dpp::snowflake channel_id = GuildConfigManager::get_tournament_log_channel(event.command.guild_id);
        if (!channel_id) {
            return;
        }

        dpp::embed embed = dpp::embed()
            .set_title(title)
            .set_description(description)
            .set_color(color)
            .set_timestamp(time(nullptr))
            .set_footer(dpp::embed_footer().set_text("Action by " + event.command.usr.username));

        bot.message_create(dpp::message(channel_id, "").add_embed(embed));
    }

    void handle_tournament_button(dpp::cluster& bot, const dpp::button_click_t& event) {
        const auto parts = split_custom_id(event.custom_id);
        if (parts.size() != 4 || parts[0] != "tournament") {
            return;
        }

        const std::string& action = parts[1];
        const int tournament_id = std::stoi(parts[2]);
        const int match_id = std::stoi(parts[3]);

        if (action == "match_checkin") {
            const bool ok = tournament_bracket::mark_checked_in(
                tournament_id,
                match_id,
                std::to_string(event.command.usr.id)
            );
            event.reply(
                dpp::message(ok ? "Match check-in recorded." : "You are not a player in this match.")
                .set_flags(dpp::m_ephemeral)
            );
            return;
        }

        if (action == "match_report") {
            event.reply(
                dpp::message("Use `/tournament bracket report` with the final score, or ask staff to report it.")
                .set_flags(dpp::m_ephemeral)
            );
            return;
        }

        if (action == "call_staff") {
            auto match = tournament_bracket::get_match(tournament_id, match_id);
            const std::string caller_id = std::to_string(event.command.usr.id);
            if (!match || (caller_id != match->player_a_id && caller_id != match->player_b_id)) {
                event.reply(
                    dpp::message("Only match players can call staff from this match button.")
                    .set_flags(dpp::m_ephemeral)
                );
                return;
            }

            event.reply(
                dpp::message("Staff has been called for match `" + std::to_string(match_id) + "`.")
                .set_flags(dpp::m_ephemeral)
            );
            send_tournament_button_log(
                bot,
                event,
                "Staff Called",
                "Tournament `" + std::to_string(tournament_id) + "`, match `" + std::to_string(match_id) +
                "`: <@" + caller_id + "> called staff from the match button."
            );
        }
    }
}

int main() {
    dpp::cluster bot(get_bot_token());

    //Register logic handlers into the global 'handlers' map
    register_fundamental_commands(bot);
    register_general_commands(bot);
    register_player_commands(bot);   // From Player.cpp
    register_tetrio_commands(bot);   // From Tetrio.cpp
    register_tournament_commands(bot);
    register_settings_commands(bot);
    register_moderation_commands(bot);

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
                .add_choice(dpp::command_option_choice("tournament", "tournament"))
                .add_choice(dpp::command_option_choice("settings", "settings"))
                .add_choice(dpp::command_option_choice("moderation", "moderation"))
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

            dpp::command_option settings_language(dpp::co_sub_command, "language", "Set server language placeholder");
            settings_language.add_option(dpp::command_option(dpp::co_string, "language", "Language", false)
                .add_choice(dpp::command_option_choice("English (GB)", "EN-gb")));
            settings.add_option(settings_language);

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

            dpp::command_option bracket_generate(dpp::co_sub_command, "generate", "Generate a bracket from checked-in players");
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

            dpp::command_option roles(dpp::co_sub_command, "roles", "Show tournament role configuration");
            tournament.add_option(roles);

            dpp::command_option set_staff_role(dpp::co_sub_command, "set_staff_role", "Set tournament staff role");
            set_staff_role.add_option(dpp::command_option(dpp::co_role, "role", "Tournament staff role", true));
            tournament.add_option(set_staff_role);

            dpp::command_option set_admin_role(dpp::co_sub_command, "set_admin_role", "Set tournament admin role");
            set_admin_role.add_option(dpp::command_option(dpp::co_role, "role", "Tournament admin role", true));
            tournament.add_option(set_admin_role);

            dpp::command_option clear_staff_role(dpp::co_sub_command, "clear_staff_role", "Clear tournament staff role");
            tournament.add_option(clear_staff_role);

            dpp::command_option clear_admin_role(dpp::co_sub_command, "clear_admin_role", "Clear tournament admin role");
            tournament.add_option(clear_admin_role);

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

    bot.on_button_click([&bot](const dpp::button_click_t& event) {
        try {
            handle_tournament_button(bot, event);
        }
        catch (const std::exception& e) {
            std::cerr << "Button interaction failed: " << e.what() << std::endl;
            event.reply(
                dpp::message(std::string("Button failed: ") + e.what())
                .set_flags(dpp::m_ephemeral)
            );
        }
    });

    bot.start(dpp::st_wait);
}

//comment
