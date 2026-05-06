#include "interactions/InteractionHandlers.hpp"
#include "core/CommandRegistry.hpp"
#include "core/Localization.hpp"
#include "core/sqlite.hpp"
#include "tetrio/TetrioService.hpp"
#include "tournament/bracket/MatchStore.hpp"
#include "tournament/manage.hpp"
#include "tournament/registration.hpp"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <exception>
#include <iostream>
#include <optional>
#include <sstream>
#include <vector>

namespace {

    std::vector<std::string> split_custom_id(const std::string& value) {
        std::vector<std::string> parts;
        std::stringstream stream(value);
        std::string item;
        while (std::getline(stream, item, ':')) {
            parts.push_back(item);
        }
        return parts;
    }

    std::string component_value(const std::vector<dpp::component>& components, const std::string& custom_id) {
        for (const auto& component : components) {
            if (component.custom_id == custom_id && std::holds_alternative<std::string>(component.value)) {
                return std::get<std::string>(component.value);
            }

            const std::string nested = component_value(component.components, custom_id);
            if (!nested.empty()) {
                return nested;
            }
        }

        return "";
    }

    std::string dashboard_help_text(const std::string& category) {
        if (category == "settings") {
            return "Settings shortcuts:\n"
                "- `/settings show` for the full server configuration snapshot.\n"
                "- `/settings language` and `/settings secondary_language` for display language.\n"
                "- `/settings set_admin_role`, `/settings set_moderator_role`, `/settings set_staff_role` for staff access.\n"
                "- `/settings modlog_set` for moderation audit logs.";
        }

        if (category == "moderation") {
            return "Moderation shortcuts:\n"
                "- `/mod history` to inspect a user's moderation record.\n"
                "- `/mod warn` and `/mod note` for non-destructive staff records.\n"
                "- `/mod timeout` and `/mod clear_timeout` for temporary action.\n"
                "- `/mod kick`, `/mod ban`, `/mod unban` for admin-level live action.";
        }

        if (category == "tournament_bracket") {
            return "Bracket shortcuts:\n"
                "- `/tournament bracket generate` after check-in and seeding.\n"
                "- `/tournament bracket current` for playable matches.\n"
                "- `/tournament bracket report` or match buttons for scores.\n"
                "- `/tournament bracket threads` to create match threads with player buttons.";
        }

        if (category == "tournament_config") {
            return "Tournament config shortcuts:\n"
                "- `/tournament config roles` to inspect staff/admin roles.\n"
                "- `/tournament config set_channel` for registration and check-in panels.\n"
                "- `/tournament config log_channel_assign` for tournament audit logs.\n"
                "- `/tournament config ruleset_show` and ruleset setters for match rules.";
        }

        return "Tournament shortcuts:\n"
            "- `/tournament create` then `/tournament registration_open` to publish registration.\n"
            "- `/tournament checkin_open` before bracket generation.\n"
            "- `/tournament participants` and `/tournament seed` for player prep.\n"
            "- `/tournament staff_info` for a private event snapshot.";
    }

    bool handle_staff_dashboard_button(const dpp::button_click_t& event) {
        const auto parts = split_custom_id(event.custom_id);
        if (parts.size() != 4 || parts[0] != "staffdash" || parts[2] != "help") {
            return false;
        }

        event.reply(dpp::message(dashboard_help_text(parts[1])).set_flags(dpp::m_ephemeral));
        return true;
    }

    std::string ut(const dpp::button_click_t& event, const std::string& key, const localization::Params& params = {}) {
        return localization::user_text(event.command.guild_id, event.command.usr.id, key, params);
    }

    std::string ut(const dpp::form_submit_t& event, const std::string& key, const localization::Params& params = {}) {
        return localization::user_text(event.command.guild_id, event.command.usr.id, key, params);
    }

    std::string participant_result_text(dpp::snowflake guild_id, dpp::snowflake user_id, const std::string& message) {
        const std::pair<const char*, const char*> map[] = {
            { "Invalid tournament or player.", "registration.result.invalid_player" },
            { "Please provide the username you want to register with.", "registration.result.username_required" },
            { "That username does not match the TETR.IO account linked to your bot profile.", "registration.result.username_mismatch" },
            { "Could not initialize participant storage.", "registration.result.storage_failed" },
            { "Could not prepare registration.", "registration.result.prepare_registration_failed" },
            { "Could not register you for this tournament.", "registration.result.register_failed" },
            { "Registration saved, but could not reload your participant record.", "registration.result.reload_failed" },
            { "You are registered for this tournament.", "registration.result.registered" },
            { "You are not registered for this tournament.", "registration.result.not_registered" },
            { "Could not prepare unregister request.", "registration.result.prepare_unregister_failed" },
            { "Could not unregister you from this tournament.", "registration.result.unregister_failed" },
            { "You have been removed from this tournament.", "registration.result.unregistered" },
            { "Check-in is closed, including the grace period.", "registration.result.checkin_grace_closed" },
            { "Could not prepare check-in.", "registration.result.prepare_checkin_failed" },
            { "Could not check you in.", "registration.result.checkin_failed" },
            { "You are checked in during grace time.", "registration.result.checked_in_late" },
            { "You are checked in.", "registration.result.checked_in" },
            { "Could not prepare check-in rollback.", "registration.result.prepare_undo_checkin_failed" },
            { "Could not undo check-in.", "registration.result.undo_checkin_failed" },
            { "Check-in has been undone.", "registration.result.undo_checkin_ok" }
        };

        for (const auto& [source, key] : map) {
            if (message == source) {
                return localization::user_text(guild_id, user_id, key);
            }
        }

        return message;
    }

    std::string participant_result_text(const dpp::button_click_t& event, const std::string& message) {
        return participant_result_text(event.command.guild_id, event.command.usr.id, message);
    }

    std::string participant_result_text(const dpp::form_submit_t& event, const std::string& message) {
        return participant_result_text(event.command.guild_id, event.command.usr.id, message);
    }

    int rank_value(const std::string& rank) {
        const std::pair<const char*, int> ranks[] = {
            { "Z", 0 }, { "D", 1 }, { "D+", 2 }, { "C-", 3 }, { "C", 4 },
            { "C+", 5 }, { "B-", 6 }, { "B", 7 }, { "B+", 8 },
            { "A-", 9 }, { "A", 10 }, { "A+", 11 }, { "S-", 12 },
            { "S", 13 }, { "S+", 14 }, { "SS", 15 }, { "U", 16 },
            { "X", 17 }, { "X+", 18 }
        };

        for (const auto& [name, value] : ranks) {
            if (rank == name) return value;
        }

        return -1;
    }

    bool tetrio_filters_active(const tournament_seeding::TetrioSeedFilters& filters) {
        return filters.current_rank_min || filters.current_rank_max
            || filters.top_rank_min || filters.top_rank_max
            || filters.tr_min || filters.tr_max
            || filters.allow_unranked;
    }

    struct EligibilityFailure {
        std::string key;
        localization::Params params;
    };

    std::optional<EligibilityFailure> check_tetrio_eligibility(
        const tournament_manage::TournamentRecord& tournament,
        const std::string& username
    ) {
        if (!tetrio_filters_active(tournament.tetrio_filters)) {
            return std::nullopt;
        }

        if (username.empty()) {
            return EligibilityFailure{ "tournament.tetrio_filter.username_required", {} };
        }

        auto profile = TetrioService::fetch_user(username);
        if (!profile) {
            return EligibilityFailure{ "tournament.tetrio_filter.profile_not_found", { { "username", username } } };
        }

        if (!profile->has_league_data && tournament.tetrio_filters.allow_unranked) {
            return std::nullopt;
        }

        if (!profile->has_league_data) {
            return EligibilityFailure{ "tournament.tetrio_filter.no_league", { { "username", profile->username } } };
        }

        const auto& filters = tournament.tetrio_filters;
        const bool allowed_unranked_rank = filters.allow_unranked && profile->rank == "Z";
        if (!allowed_unranked_rank && filters.current_rank_min && rank_value(profile->rank) < rank_value(*filters.current_rank_min)) {
            return EligibilityFailure{ "tournament.tetrio_filter.current_rank_below", { { "player", profile->username } } };
        }
        if (!allowed_unranked_rank && filters.current_rank_max && rank_value(profile->rank) > rank_value(*filters.current_rank_max)) {
            return EligibilityFailure{ "tournament.tetrio_filter.current_rank_above", { { "player", profile->username } } };
        }
        if (!allowed_unranked_rank && filters.top_rank_min && rank_value(profile->top_rank) < rank_value(*filters.top_rank_min)) {
            return EligibilityFailure{ "tournament.tetrio_filter.top_rank_below", { { "player", profile->username } } };
        }
        if (!allowed_unranked_rank && filters.top_rank_max && rank_value(profile->top_rank) > rank_value(*filters.top_rank_max)) {
            return EligibilityFailure{ "tournament.tetrio_filter.top_rank_above", { { "player", profile->username } } };
        }
        if (filters.tr_min && profile->rating < *filters.tr_min) {
            return EligibilityFailure{ "tournament.tetrio_filter.tr_below", { { "player", profile->username } } };
        }
        if (filters.tr_max && profile->rating > *filters.tr_max) {
            return EligibilityFailure{ "tournament.tetrio_filter.tr_above", { { "player", profile->username } } };
        }

        return std::nullopt;
    }

    void send_tournament_interaction_log(
        dpp::cluster& bot,
        dpp::snowflake guild_id,
        const std::string& actor_username,
        const std::string& title,
        const std::string& description,
        int color = 0xf0b429
    ) {
        const dpp::snowflake channel_id = GuildConfigManager::get_tournament_log_channel(guild_id);
        if (!channel_id) {
            return;
        }

        dpp::embed embed = dpp::embed()
            .set_title(title)
            .set_description(description)
            .set_color(color)
            .set_timestamp(time(nullptr))
            .set_footer(dpp::embed_footer().set_text("Action by " + actor_username));

        bot.message_create(dpp::message(channel_id, "").add_embed(embed));
    }

    dpp::interaction_modal_response registration_modal(int tournament_id) {
        return dpp::interaction_modal_response(
            "tournament:register_modal:" + std::to_string(tournament_id) + ":0",
            "Tournament Registration"
        ).add_component(
            dpp::component()
            .set_label("Tournament username")
            .set_id("username")
            .set_text_style(dpp::text_short)
            .set_placeholder("Your in-game name")
            .set_min_length(1)
            .set_max_length(64)
            .set_required(true)
        );
    }

    dpp::interaction_modal_response report_score_modal(int tournament_id, int match_id) {
        return dpp::interaction_modal_response(
            "tournament:report_modal:" + std::to_string(tournament_id) + ":" + std::to_string(match_id),
            "Report Score"
        ).add_component(
            dpp::component()
            .set_label("Player A score")
            .set_id("score_a")
            .set_text_style(dpp::text_short)
            .set_placeholder("0")
            .set_min_length(1)
            .set_max_length(3)
            .set_required(true)
        ).add_row().add_component(
            dpp::component()
            .set_label("Player B score")
            .set_id("score_b")
            .set_text_style(dpp::text_short)
            .set_placeholder("0")
            .set_min_length(1)
            .set_max_length(3)
            .set_required(true)
        );
    }

    dpp::interaction_modal_response forfeit_modal(int tournament_id, int match_id) {
        return dpp::interaction_modal_response(
            "tournament:forfeit_modal:" + std::to_string(tournament_id) + ":" + std::to_string(match_id),
            "Confirm Forfeit"
        ).add_component(
            dpp::component()
            .set_label("Type FORFEIT")
            .set_id("confirm")
            .set_text_style(dpp::text_short)
            .set_placeholder("FORFEIT")
            .set_min_length(7)
            .set_max_length(7)
            .set_required(true)
        );
    }

    bool is_match_player(const tournament_bracket::StoredMatch& match, dpp::snowflake user_id) {
        const std::string user = std::to_string(user_id);
        return user == match.player_a_id || user == match.player_b_id;
    }

    bool can_use_player_match_action(const dpp::button_click_t& event, int tournament_id, int match_id) {
        auto match = tournament_bracket::get_match(tournament_id, match_id);
        if (!match || !is_match_player(*match, event.command.usr.id)) {
            event.reply(dpp::message(ut(event, "interaction.match.player_only")).set_flags(dpp::m_ephemeral));
            return false;
        }

        if (match->state != tournament_bracket::StoredMatchState::Ready
            && match->state != tournament_bracket::StoredMatchState::Ongoing) {
            event.reply(dpp::message(ut(event, "interaction.match.not_accepting_actions")).set_flags(dpp::m_ephemeral));
            return false;
        }

        return true;
    }

    void handle_report_submit(const dpp::form_submit_t& event, int tournament_id, int match_id) {
        auto match = tournament_bracket::get_match(tournament_id, match_id);
        if (!match || !is_match_player(*match, event.command.usr.id)) {
            event.reply(dpp::message(ut(event, "interaction.match.report_player_only")).set_flags(dpp::m_ephemeral));
            return;
        }

        try {
            const int score_a = std::stoi(component_value(event.components, "score_a"));
            const int score_b = std::stoi(component_value(event.components, "score_b"));
            const bool ok = tournament_bracket::report_match(tournament_id, match_id, score_a, score_b);
            event.reply(dpp::message(ut(event, ok ? "interaction.match.score_reported" : "interaction.match.score_report_failed")).set_flags(dpp::m_ephemeral));
        }
        catch (...) {
            event.reply(dpp::message(ut(event, "interaction.match.score_integer_required")).set_flags(dpp::m_ephemeral));
        }
    }

    void handle_forfeit_submit(dpp::cluster& bot, const dpp::form_submit_t& event, int tournament_id, int match_id) {
        auto match = tournament_bracket::get_match(tournament_id, match_id);
        if (!match || !is_match_player(*match, event.command.usr.id)) {
            event.reply(dpp::message(ut(event, "interaction.match.forfeit_player_only")).set_flags(dpp::m_ephemeral));
            return;
        }

        if (component_value(event.components, "confirm") != "FORFEIT") {
            event.reply(dpp::message(ut(event, "interaction.match.forfeit_cancelled")).set_flags(dpp::m_ephemeral));
            return;
        }

        const std::string player_id = std::to_string(event.command.usr.id);
        const bool ok = tournament_bracket::forfeit_player(tournament_id, match_id, player_id, "player_forfeit");
        event.reply(dpp::message(ut(event, ok ? "interaction.match.forfeit_recorded" : "interaction.match.forfeit_failed")).set_flags(dpp::m_ephemeral));

        if (ok) {
            send_tournament_interaction_log(
                bot,
                event.command.guild_id,
                event.command.usr.username,
                "Player Forfeit",
                "Tournament `" + std::to_string(tournament_id) + "`, match `" + std::to_string(match_id) +
                "`: <@" + player_id + "> forfeited from the match button.",
                0xe05252
            );
        }
    }

    void handle_panel_checkin(const dpp::button_click_t& event, int tournament_id) {
        auto tournament = tournament_manage::get_tournament(tournament_id);
        if (!tournament) {
            event.reply(dpp::message(localization::user_text(event.command.guild_id, event.command.usr.id, "panel.tournament_not_found")).set_flags(dpp::m_ephemeral));
            return;
        }

        if (!tournament->checkin_open) {
            event.reply(dpp::message(localization::user_text(event.command.guild_id, event.command.usr.id, "panel.checkin_closed")).set_flags(dpp::m_ephemeral));
            return;
        }

        const std::string discord_id = std::to_string(event.command.usr.id);
        auto participant = tournament_registration::get_participant(tournament_id, discord_id);
        if (!participant) {
            event.reply(dpp::message(localization::user_text(event.command.guild_id, event.command.usr.id, "panel.checkin.not_registered")).set_flags(dpp::m_ephemeral));
            return;
        }

        if (auto failure = check_tetrio_eligibility(*tournament, participant->provided_username)) {
            event.reply(dpp::message(ut(event, failure->key, failure->params)).set_flags(dpp::m_ephemeral));
            return;
        }

        auto profile = PlayerManager::get_profile(event.command.usr.id);
        tournament_registration::CheckInRequest request;
        request.tournament_id = tournament_id;
        request.discord_id = discord_id;
        request.provided_username = participant->provided_username;
        request.linked_tetrio_id = profile.count("tetrio_id") ? profile["tetrio_id"] : "";
        request.now = static_cast<int>(time(nullptr));
        request.checkin_closes_at = tournament->checkin_closes_at;
        request.grace_time = tournament->checkin_grace_time;
        request.staff_override = false;

        auto result = tournament_registration::check_in_player(request);
        event.reply(dpp::message(participant_result_text(event, result.message)).set_flags(dpp::m_ephemeral));
    }

    void handle_registration_submit(dpp::cluster& bot, const dpp::form_submit_t& event, int tournament_id) {
        auto tournament = tournament_manage::get_tournament(tournament_id);
        if (!tournament) {
            event.reply(dpp::message(localization::user_text(event.command.guild_id, event.command.usr.id, "panel.tournament_not_found")).set_flags(dpp::m_ephemeral));
            return;
        }

        if (!tournament->registration_open) {
            event.reply(dpp::message(localization::user_text(event.command.guild_id, event.command.usr.id, "panel.registration_closed")).set_flags(dpp::m_ephemeral));
            return;
        }

        const std::string username = component_value(event.components, "username");
        if (username.empty()) {
            event.reply(dpp::message(localization::user_text(event.command.guild_id, event.command.usr.id, "panel.registration.username_required")).set_flags(dpp::m_ephemeral));
            return;
        }

        if (auto failure = check_tetrio_eligibility(*tournament, username)) {
            event.reply(dpp::message(ut(event, failure->key, failure->params)).set_flags(dpp::m_ephemeral));
            return;
        }

        auto profile = PlayerManager::get_profile(event.command.usr.id);
        tournament_registration::RegistrationRequest request;
        request.tournament_id = tournament_id;
        request.discord_id = std::to_string(event.command.usr.id);
        request.display_name = event.command.usr.username;
        request.provided_username = username;
        request.linked_tetrio_id = profile.count("tetrio_id") ? profile["tetrio_id"] : "";
        request.registered_at = static_cast<int>(time(nullptr));

        auto result = tournament_registration::register_player(request);
        event.reply(dpp::message(participant_result_text(event, result.message)).set_flags(dpp::m_ephemeral));

        if (result.ok) {
            const dpp::snowflake channel_id = GuildConfigManager::get_tournament_log_channel(event.command.guild_id);
            if (channel_id) {
                bot.message_create(dpp::message(
                    channel_id,
                    "Tournament `" + std::to_string(tournament_id) + "`: <@" +
                    std::to_string(event.command.usr.id) + "> registered as `" + username + "`."
                ));
            }
        }
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

        if (action == "register") {
            event.dialog(registration_modal(tournament_id));
            return;
        }

        if (action == "checkin") {
            handle_panel_checkin(event, tournament_id);
            return;
        }

        if (action == "match_checkin") {
            const bool ok = tournament_bracket::mark_checked_in(
                tournament_id,
                match_id,
                std::to_string(event.command.usr.id)
            );
            event.reply(
                dpp::message(ut(event, ok ? "interaction.match.checkin_recorded" : "interaction.match.not_player"))
                .set_flags(dpp::m_ephemeral)
            );
            return;
        }

        if (action == "match_report") {
            if (!can_use_player_match_action(event, tournament_id, match_id)) {
                return;
            }

            event.dialog(report_score_modal(tournament_id, match_id));
            return;
        }

        if (action == "match_forfeit") {
            if (!can_use_player_match_action(event, tournament_id, match_id)) {
                return;
            }

            event.dialog(forfeit_modal(tournament_id, match_id));
            return;
        }

        if (action == "call_staff") {
            auto match = tournament_bracket::get_match(tournament_id, match_id);
            const std::string caller_id = std::to_string(event.command.usr.id);
            if (!match || (caller_id != match->player_a_id && caller_id != match->player_b_id)) {
                event.reply(
                    dpp::message(ut(event, "interaction.match.call_staff_player_only"))
                    .set_flags(dpp::m_ephemeral)
                );
                return;
            }

            event.reply(
                dpp::message(ut(event, "interaction.match.call_staff_ok", { { "match_id", std::to_string(match_id) } }))
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

void register_interaction_handlers(dpp::cluster& bot) {
    bot.on_slashcommand([](const dpp::slashcommand_t& event) {
        try {
            auto name = event.command.get_command_name();
            auto it = handlers.find(name);
            if (it != handlers.end()) {
                it->second(event);
            }
            else {
                event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "error.unknown_command"));
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
            if (handle_staff_dashboard_button(event)) {
                return;
            }

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

    bot.on_form_submit([&bot](const dpp::form_submit_t& event) {
        try {
            const auto parts = split_custom_id(event.custom_id);
            if (parts.size() == 4 && parts[0] == "tournament" && parts[1] == "register_modal") {
                handle_registration_submit(bot, event, std::stoi(parts[2]));
                return;
            }

            if (parts.size() == 4 && parts[0] == "tournament" && parts[1] == "report_modal") {
                handle_report_submit(event, std::stoi(parts[2]), std::stoi(parts[3]));
                return;
            }

            if (parts.size() == 4 && parts[0] == "tournament" && parts[1] == "forfeit_modal") {
                handle_forfeit_submit(bot, event, std::stoi(parts[2]), std::stoi(parts[3]));
                return;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Form submit failed: " << e.what() << std::endl;
            event.reply(
                dpp::message(std::string("Form failed: ") + e.what())
                .set_flags(dpp::m_ephemeral)
            );
        }
    });
}

