#include "core/CommandRegistry.hpp"
#include "core/security/PermissionManager.hpp"
#include "core/sqlite.hpp"
#include "tournament/bracket/MatchStore.hpp"
#include "tournament/discord/MatchThreads.hpp"
#include "tournament/manage.hpp"
#include "tournament/registration.hpp"
#include "tournament/ruleset.hpp"
#include "tournament/seeding.hpp"
#include "tournament/utility/BracketSvg.hpp"
#include <ctime>
#include <sstream>

namespace {
	const dpp::command_data_option* find_option(
		const dpp::command_data_option& parent,
		const std::string& name
	) {
		for (const auto& option : parent.options) {
			if (option.name == name) {
				return &option;
			}
		}

		return nullptr;
	}

	std::string get_string_option(
		const dpp::command_data_option& parent,
		const std::string& name,
		const std::string& fallback = ""
	) {
		const auto* option = find_option(parent, name);
		if (!option || !std::holds_alternative<std::string>(option->value)) {
			return fallback;
		}

		return std::get<std::string>(option->value);
	}

	int get_int_option(const dpp::command_data_option& parent, const std::string& name, int fallback = 0) {
		const auto* option = find_option(parent, name);
		if (!option || !std::holds_alternative<int64_t>(option->value)) {
			return fallback;
		}

		return static_cast<int>(std::get<int64_t>(option->value));
	}

	bool get_bool_option(const dpp::command_data_option& parent, const std::string& name, bool fallback = false) {
		const auto* option = find_option(parent, name);
		if (!option || !std::holds_alternative<bool>(option->value)) {
			return fallback;
		}

		return std::get<bool>(option->value);
	}

	bool has_option(const dpp::command_data_option& parent, const std::string& name) {
		return find_option(parent, name) != nullptr;
	}

	dpp::snowflake get_snowflake_option(const dpp::command_data_option& parent, const std::string& name) {
		const auto* option = find_option(parent, name);
		if (!option || !std::holds_alternative<dpp::snowflake>(option->value)) {
			return 0;
		}

		return std::get<dpp::snowflake>(option->value);
	}

	std::string channel_display(dpp::snowflake channel_id) {
		if (!channel_id) {
			return "Not configured";
		}

		return "<#" + std::to_string(channel_id) + ">";
	}

	std::string user_display(const dpp::slashcommand_t& event, dpp::snowflake user_id) {
		if (user_id == event.command.usr.id) {
			return event.command.usr.username;
		}

		const auto user_it = event.command.resolved.users.find(user_id);
		if (user_it != event.command.resolved.users.end()) {
			return user_it->second.username;
		}

		return std::to_string(user_id);
	}

	void reply_ephemeral(const dpp::slashcommand_t& event, const std::string& content) {
		event.reply(dpp::message(content).set_flags(dpp::m_ephemeral));
	}

	void edit_ephemeral(const dpp::slashcommand_t& event, const std::string& content) {
		event.edit_response(dpp::message(content).set_flags(dpp::m_ephemeral));
	}

	void edit_logged(const dpp::slashcommand_t& event, const std::string& content) {
		event.edit_response(dpp::message(content));
	}

	void edit_logged_embed(const dpp::slashcommand_t& event, const dpp::embed& embed) {
		event.edit_response(dpp::message().add_embed(embed));
	}

	void edit_logged_file(
		const dpp::slashcommand_t& event,
		const std::string& content,
		const std::string& filename,
		const std::string& filecontent
	) {
		event.edit_response(dpp::message(content).add_file(filename, filecontent, "image/svg+xml"));
	}

	std::string yes_no(bool value) {
		return value ? "Yes" : "No";
	}

	bool require_manage(const dpp::slashcommand_t& event) {
		if (PermissionManager::can_manage_tournament(event)) {
			return true;
		}

		reply_ephemeral(event, "You need tournament staff or Discord moderator permission to use this.");
		return false;
	}

	bool require_admin(const dpp::slashcommand_t& event) {
		if (PermissionManager::can_admin_tournament(event)) {
			return true;
		}

		reply_ephemeral(event, "You need tournament admin or Discord administrator permission to use this.");
		return false;
	}

	bool require_role_config(const dpp::slashcommand_t& event) {
		if (PermissionManager::can_configure_tournament_roles(event)) {
			return true;
		}

		reply_ephemeral(event, "You need Discord administrator permission to configure tournament roles.");
		return false;
	}

	bool require_tournament_channel(const dpp::slashcommand_t& event) {
		const dpp::snowflake configured_channel = GuildConfigManager::get_tournament_channel(event.command.guild_id);
		if (!configured_channel || configured_channel == event.command.channel_id) {
			return true;
		}

		reply_ephemeral(event, "Please use the tournament channel: " + channel_display(configured_channel));
		return false;
	}

	std::string participant_summary(const std::vector<tournament_registration::ParticipantRecord>& participants) {
		if (participants.empty()) {
			return "No participants found.";
		}

		std::ostringstream out;
		for (const auto& participant : participants) {
			out << "- " << (participant.display_name.empty() ? participant.discord_id : participant.display_name)
				<< " (" << tournament_registration::status_to_string(participant.status) << ")";

			if (participant.seed > 0) {
				out << " seed " << participant.seed;
			}

			out << "\n";
		}

		std::string result = out.str();
		if (result.size() > 1800) {
			result.resize(1800);
			result += "\n...";
		}

		return result;
	}

	std::string match_summary(const std::vector<tournament_bracket::StoredMatch>& matches) {
		if (matches.empty()) {
			return "No matches found.";
		}

		std::ostringstream out;
		for (const auto& match : matches) {
			out << "- " << tournament_bracket::describe_match(match) << "\n";
		}

		std::string result = out.str();
		if (result.size() > 1800) {
			result.resize(1800);
			result += "\n...";
		}

		return result;
	}

	int count_status(
		const std::vector<tournament_registration::ParticipantRecord>& participants,
		tournament_registration::ParticipantStatus status
	) {
		int count = 0;
		for (const auto& participant : participants) {
			if (participant.status == status) {
				++count;
			}
		}

		return count;
	}

	void handle_info(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		const auto tournament = tournament_manage::get_tournament(tournament_id);
		if (!tournament) {
			edit_logged(event, "Tournament not found.");
			return;
		}

		const auto participants = tournament_registration::list_participants(tournament_id);
		const auto checked_in = tournament_registration::list_checked_in_participants(tournament_id);
		const auto primary = tournament_ruleset::get_effective_primary_ruleset(tournament_id);
		const auto secondary = tournament_ruleset::get_ruleset(
			tournament_id,
			tournament_ruleset::RulesetScope::SECONDARY
		);

		dpp::embed embed = dpp::embed()
			.set_title(tournament->name)
			.set_color(0x5f9ea0)
			.add_field("Game", tournament->game_type.empty() ? "Unspecified" : tournament->game_type, true)
			.add_field("Status", tournament->status, true)
			.add_field("Registration", yes_no(tournament->registration_open), true)
			.add_field("Check-in", yes_no(tournament->checkin_open), true)
			.add_field("Players", std::to_string(participants.size()), true)
			.add_field("Checked in", std::to_string(checked_in.size()), true)
			.add_field("Primary rules", tournament_ruleset::describe_ruleset(primary), false);

		if (secondary) {
			embed.add_field("Secondary rules", tournament_ruleset::describe_ruleset(*secondary), false);
		}

		edit_logged_embed(event, embed);
	}

	void handle_staff_info(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		const auto tournament = tournament_manage::get_tournament(tournament_id);
		if (!tournament) {
			edit_logged(event, "Tournament not found.");
			return;
		}

		const auto participants = tournament_registration::list_participants(tournament_id);
		const dpp::snowflake configured_channel = GuildConfigManager::get_tournament_channel(event.command.guild_id);

		dpp::embed embed = dpp::embed()
			.set_title("Staff tournament info: " + tournament->name)
			.set_color(0xf0b429)
			.add_field("Tournament ID", std::to_string(tournament->id), true)
			.add_field("Status", tournament->status, true)
			.add_field("Tournament channel", channel_display(configured_channel), false)
			.add_field("Registration open", yes_no(tournament->registration_open), true)
			.add_field("Check-in open", yes_no(tournament->checkin_open), true)
			.add_field("Check-in closes at", tournament->checkin_closes_at > 0 ? std::to_string(tournament->checkin_closes_at) : "Not set", true)
			.add_field("Check-in grace", std::to_string(tournament->checkin_grace_time) + " seconds", true)
			.add_field("Registered", std::to_string(count_status(participants, tournament_registration::ParticipantStatus::Registered)), true)
			.add_field("Checked in", std::to_string(count_status(participants, tournament_registration::ParticipantStatus::CheckedIn)), true)
			.add_field("Late checked in", std::to_string(count_status(participants, tournament_registration::ParticipantStatus::LateCheckedIn)), true)
			.add_field("Dropped", std::to_string(count_status(participants, tournament_registration::ParticipantStatus::Dropped)), true)
			.add_field("No-show", std::to_string(count_status(participants, tournament_registration::ParticipantStatus::NoShow)), true);

		edit_logged_embed(event, embed);
	}

	void handle_create(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const std::string name = get_string_option(subcommand, "name");
		const std::string game = get_string_option(subcommand, "game", "tetrio");

		auto id = tournament_manage::create_tournament(name, game);
		if (!id) {
			edit_logged(event, "Could not create the tournament. Check the name and try again.");
			return;
		}

		edit_logged(event, "Tournament created: `" + name + "` with ID `" + std::to_string(*id) + "`.");
	}

	void handle_edit(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		tournament_manage::TournamentUpdate update;
		update.name = get_string_option(subcommand, "name");
		update.game_type = get_string_option(subcommand, "game");
		update.status = get_string_option(subcommand, "status");

		if (update.name->empty()) update.name.reset();
		if (update.game_type->empty()) update.game_type.reset();
		if (update.status->empty()) update.status.reset();

		const int tournament_id = get_int_option(subcommand, "id");
		if (!tournament_manage::update_tournament(tournament_id, update)) {
			edit_logged(event, "Could not update that tournament.");
			return;
		}

		edit_logged(event, "Tournament updated.");
	}

	void handle_delete(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_admin(event)) return;
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		if (!tournament_manage::delete_tournament(tournament_id)) {
			edit_logged(event, "Could not delete that tournament.");
			return;
		}

		edit_logged(event, "Tournament deleted.");
	}

	void handle_clear(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_admin(event)) return;
		event.thinking(false);

		const std::string confirm = get_string_option(subcommand, "confirm");
		if (confirm != "RESET") {
			edit_logged(event, "Clear aborted. Type `RESET` in the `confirm` option to expunge tournament data.");
			return;
		}

		if (!tournament_manage::clear_all_tournament_data()) {
			edit_logged(event, "Could not clear tournament data.");
			return;
		}

		edit_logged(event, "Tournament module data has been cleared. Player profile links were left untouched.");
	}

	void handle_register(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		const dpp::snowflake target_id = get_snowflake_option(subcommand, "user");
		const dpp::snowflake effective_user_id = target_id ? target_id : event.command.usr.id;
		const bool staff_action = effective_user_id != event.command.usr.id;
		const bool abort = get_bool_option(subcommand, "abort", false);

		if (staff_action) {
			if (!require_manage(event)) return;
		}
		else if (!require_tournament_channel(event)) {
			return;
		}

		event.thinking(staff_action ? false : true);
		const int tournament_id = get_int_option(subcommand, "id");
		const std::string username = get_string_option(subcommand, "username");

		if (abort) {
			auto result = tournament_registration::unregister_player(
				tournament_id,
				std::to_string(effective_user_id)
			);

			if (staff_action) {
				edit_logged(event, result.message + " Target: <@" + std::to_string(effective_user_id) + ">.");
			}
			else {
				edit_ephemeral(event, result.message);
			}
			return;
		}

		if (username.empty()) {
			if (staff_action) {
				edit_logged(event, "Provide `username` when registering a player.");
			}
			else {
				edit_ephemeral(event, "Please provide the username you want to register with.");
			}
			return;
		}

		auto tournament = tournament_manage::get_tournament(tournament_id);
		if (!tournament) {
			staff_action ? edit_logged(event, "Tournament not found.") : edit_ephemeral(event, "Tournament not found.");
			return;
		}

		if (!staff_action && !tournament->registration_open) {
			edit_ephemeral(event, "Registration is not open for this tournament.");
			return;
		}

		auto profile = PlayerManager::get_profile(effective_user_id);
		const std::string linked_tetrio = profile.count("tetrio_id") ? profile["tetrio_id"] : "";

		tournament_registration::RegistrationRequest request;
		request.tournament_id = tournament_id;
		request.discord_id = std::to_string(effective_user_id);
		request.display_name = user_display(event, effective_user_id);
		request.provided_username = username;
		request.linked_tetrio_id = linked_tetrio;
		request.registered_at = static_cast<int>(time(nullptr));

		auto result = tournament_registration::register_player(request);
		if (staff_action) {
			edit_logged(event, result.message + " Target: <@" + std::to_string(effective_user_id) + ">.");
		}
		else {
			edit_ephemeral(event, result.message);
		}
	}

	void handle_checkin(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		const dpp::snowflake target_id = get_snowflake_option(subcommand, "user");
		const dpp::snowflake effective_user_id = target_id ? target_id : event.command.usr.id;
		const bool staff_action = effective_user_id != event.command.usr.id;
		const bool abort = get_bool_option(subcommand, "abort", false);

		if (staff_action) {
			if (!require_manage(event)) return;
		}
		else if (!require_tournament_channel(event)) {
			return;
		}

		event.thinking(staff_action ? false : true);
		const int tournament_id = get_int_option(subcommand, "id");
		const std::string username = get_string_option(subcommand, "username");

		if (abort) {
			auto result = tournament_registration::undo_check_in(
				tournament_id,
				std::to_string(effective_user_id)
			);

			if (staff_action) {
				edit_logged(event, result.message + " Target: <@" + std::to_string(effective_user_id) + ">.");
			}
			else {
				edit_ephemeral(event, result.message);
			}
			return;
		}

		auto tournament = tournament_manage::get_tournament(tournament_id);
		if (!tournament) {
			staff_action ? edit_logged(event, "Tournament not found.") : edit_ephemeral(event, "Tournament not found.");
			return;
		}

		if (!staff_action && !tournament->checkin_open) {
			edit_ephemeral(event, "Check-in is not open for this tournament.");
			return;
		}

		auto participant = tournament_registration::get_participant(tournament_id, std::to_string(effective_user_id));
		const std::string effective_username = !username.empty()
			? username
			: (participant ? participant->provided_username : "");

		auto profile = PlayerManager::get_profile(effective_user_id);
		const std::string linked_tetrio = profile.count("tetrio_id") ? profile["tetrio_id"] : "";

		tournament_registration::CheckInRequest request;
		request.tournament_id = tournament_id;
		request.discord_id = std::to_string(effective_user_id);
		request.provided_username = effective_username;
		request.linked_tetrio_id = linked_tetrio;
		request.now = static_cast<int>(time(nullptr));
		request.checkin_closes_at = tournament->checkin_closes_at;
		request.grace_time = tournament->checkin_grace_time;
		request.staff_override = staff_action;

		auto result = tournament_registration::check_in_player(request);
		if (staff_action) {
			edit_logged(event, result.message + " Target: <@" + std::to_string(effective_user_id) + ">.");
		}
		else {
			edit_ephemeral(event, result.message);
		}
	}

	void handle_participants(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		auto participants = tournament_registration::list_participants(tournament_id);
		edit_logged(event, participant_summary(participants));
	}

	void handle_seed(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		const std::string mode = get_string_option(subcommand, "mode", "general");
		auto participants = tournament_registration::list_checked_in_participants(tournament_id);

		std::vector<tournament_seeding::SeededPlayer> seeded;
		if (mode == "tetrio") {
			seeded = tournament_seeding::seed_tetrio(participants);
		}
		else {
			seeded = tournament_seeding::seed_general(participants);
		}

		for (const auto& player : seeded) {
			tournament_registration::set_participant_seed(tournament_id, player.discord_id, player.seed);
		}

		std::string csv = tournament_seeding::export_seed_csv(seeded);
		if (csv.size() > 1800) {
			csv.resize(1800);
			csv += "\n...";
		}

		const std::string message = "Seeding complete.\n```csv\n" + csv + "```";
		edit_logged(event, message);
	}

	void handle_bracket_generate(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		const std::string type = get_string_option(subcommand, "type", "single_elimination");
		if (type != "single_elimination") {
			edit_logged(event, "Only single-elimination bracket generation is available right now.");
			return;
		}

		if (!tournament_bracket::generate_single_elimination(tournament_id)) {
			edit_logged(event, "Could not generate a bracket. Make sure at least two players are checked in.");
			return;
		}

		edit_logged(event, "Single-elimination bracket generated for tournament `" + std::to_string(tournament_id) + "`.");
	}

	void handle_matches_current(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		event.thinking(false);
		const int tournament_id = get_int_option(subcommand, "id");
		edit_logged(event, match_summary(tournament_bracket::list_current_matches(tournament_id)));
	}

	void handle_matches_round(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		event.thinking(false);
		const int tournament_id = get_int_option(subcommand, "id");
		const int round = get_int_option(subcommand, "round");
		edit_logged(event, match_summary(tournament_bracket::list_round_matches(tournament_id, round - 1)));
	}

	void handle_match_show(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		event.thinking(false);
		const int tournament_id = get_int_option(subcommand, "id");
		const int match_id = get_int_option(subcommand, "match_id");
		auto match = tournament_bracket::get_match(tournament_id, match_id);
		if (!match) {
			edit_logged(event, "Match not found.");
			return;
		}

		edit_logged_embed(event, tournament_discord::build_match_embed(*match));
	}

	void handle_match_report(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		const int match_id = get_int_option(subcommand, "match_id");
		const int score_a = get_int_option(subcommand, "score_a");
		const int score_b = get_int_option(subcommand, "score_b");

		if (!tournament_bracket::report_match(tournament_id, match_id, score_a, score_b)) {
			edit_logged(event, "Could not report that match.");
			return;
		}

		edit_logged(event, "Match `" + std::to_string(match_id) + "` reported.");
	}

	void handle_stream_assign(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		const int match_id = get_int_option(subcommand, "match_id");
		if (!tournament_bracket::assign_streamed(tournament_id, match_id, true)) {
			edit_logged(event, "Could not assign that match to stream.");
			return;
		}

		edit_logged(event, "Match `" + std::to_string(match_id) + "` assigned to stream.");
	}

	void handle_stream_clear(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		const int match_id = get_int_option(subcommand, "match_id");
		if (!tournament_bracket::assign_streamed(tournament_id, match_id, false)) {
			edit_logged(event, "Could not clear that streamed match assignment.");
			return;
		}

		edit_logged(event, "Match `" + std::to_string(match_id) + "` removed from stream assignments.");
	}

	void handle_stream_list(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		event.thinking(false);
		const int tournament_id = get_int_option(subcommand, "id");
		edit_logged(event, match_summary(tournament_bracket::list_streamed_matches(tournament_id)));
	}

	void handle_match_threads(
		dpp::cluster& bot,
		const dpp::slashcommand_t& event,
		const dpp::command_data_option& subcommand
	) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		const int round = get_int_option(subcommand, "round", 0);
		const bool include_buttons = get_bool_option(subcommand, "buttons", true);
		const dpp::snowflake channel_id = GuildConfigManager::get_tournament_channel(event.command.guild_id);
		if (!channel_id) {
			edit_logged(event, "Set a tournament channel first with `/tournament config set_channel`.");
			return;
		}

		std::vector<tournament_bracket::StoredMatch> matches =
			round > 0
			? tournament_bracket::list_round_matches(tournament_id, round - 1)
			: tournament_bracket::list_current_matches(tournament_id);

		int queued = 0;
		for (const auto& match : matches) {
			if (match.thread_id || match.player_a_id.empty() || match.player_b_id.empty()) {
				continue;
			}

			tournament_discord::create_match_thread(bot, channel_id, match, include_buttons);
			++queued;
		}

		edit_logged(event, "Queued `" + std::to_string(queued) + "` match thread creation request(s).");
	}

	void handle_bracket_svg(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		const auto matches = tournament_bracket::list_matches(tournament_id);
		if (matches.empty()) {
			edit_logged(event, "No bracket matches found.");
			return;
		}

		const std::string svg = tournament_utility::render_bracket_svg(matches);
		edit_logged_file(event, "Bracket SVG generated.", "bracket-" + std::to_string(tournament_id) + ".svg", svg);
	}

	void handle_match_svg(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		const int match_id = get_int_option(subcommand, "match_id");
		auto match = tournament_bracket::get_match(tournament_id, match_id);
		if (!match) {
			edit_logged(event, "Match not found.");
			return;
		}

		const std::string svg = tournament_utility::render_match_svg(*match);
		edit_logged_file(event, "Match SVG generated.", "match-" + std::to_string(match_id) + ".svg", svg);
	}

	void handle_set_staff_role(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_role_config(event)) return;
		event.thinking(false);

		const dpp::snowflake role_id = get_snowflake_option(subcommand, "role");
		if (!GuildConfigManager::set_staff_role(event.command.guild_id, role_id)) {
			edit_logged(event, "Could not set the tournament staff role.");
			return;
		}

		edit_logged(event, "Tournament staff role updated.");
	}

	void handle_set_admin_role(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_role_config(event)) return;
		event.thinking(false);

		const dpp::snowflake role_id = get_snowflake_option(subcommand, "role");
		if (!GuildConfigManager::set_admin_role(event.command.guild_id, role_id)) {
			edit_logged(event, "Could not set the tournament admin role.");
			return;
		}

		edit_logged(event, "Tournament admin role updated.");
	}

	std::string role_display(dpp::snowflake role_id) {
		if (!role_id) {
			return "Not configured";
		}

		return "<@&" + std::to_string(role_id) + ">";
	}

	void handle_roles(const dpp::slashcommand_t& event) {
		if (!require_role_config(event)) return;
		event.thinking(false);

		GuildConfigManager::init();
		const dpp::snowflake staff_role = GuildConfigManager::get_staff_role(event.command.guild_id);
		const dpp::snowflake admin_role = GuildConfigManager::get_admin_role(event.command.guild_id);

		edit_logged(
			event,
			"Tournament role configuration:\n"
			"Staff: " + role_display(staff_role) + "\n"
			"Admin: " + role_display(admin_role)
		);
	}

	void handle_clear_staff_role(const dpp::slashcommand_t& event) {
		if (!require_role_config(event)) return;
		event.thinking(false);

		if (!GuildConfigManager::clear_staff_role(event.command.guild_id)) {
			edit_logged(event, "Could not clear the tournament staff role.");
			return;
		}

		edit_logged(event, "Tournament staff role cleared.");
	}

	void handle_clear_admin_role(const dpp::slashcommand_t& event) {
		if (!require_role_config(event)) return;
		event.thinking(false);

		if (!GuildConfigManager::clear_admin_role(event.command.guild_id)) {
			edit_logged(event, "Could not clear the tournament admin role.");
			return;
		}

		edit_logged(event, "Tournament admin role cleared.");
	}

	void handle_set_tournament_channel(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_role_config(event)) return;
		event.thinking(false);

		const dpp::snowflake channel_id = get_snowflake_option(subcommand, "channel");
		if (!GuildConfigManager::set_tournament_channel(event.command.guild_id, channel_id)) {
			edit_logged(event, "Could not set the tournament channel.");
			return;
		}

		edit_logged(event, "Tournament channel updated: " + channel_display(channel_id));
	}

	void handle_clear_tournament_channel(const dpp::slashcommand_t& event) {
		if (!require_role_config(event)) return;
		event.thinking(false);

		if (!GuildConfigManager::clear_tournament_channel(event.command.guild_id)) {
			edit_logged(event, "Could not clear the tournament channel.");
			return;
		}

		edit_logged(event, "Tournament channel cleared.");
	}

	void handle_registration_open(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		if (!tournament_manage::set_registration_open(tournament_id, true)) {
			edit_logged(event, "Could not open registration.");
			return;
		}

		edit_logged(event, "Registration opened for tournament `" + std::to_string(tournament_id) + "`.");
	}

	void handle_registration_close(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		if (!tournament_manage::set_registration_open(tournament_id, false)) {
			edit_logged(event, "Could not close registration.");
			return;
		}

		edit_logged(event, "Registration closed for tournament `" + std::to_string(tournament_id) + "`.");
	}

	void handle_checkin_open(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		const int closes_at = get_int_option(subcommand, "closes_at");
		const int grace_time = get_int_option(
			subcommand,
			"grace_time",
			tournament_registration::DEFAULT_CHECKIN_GRACE_TIME
		);

		if (!tournament_manage::set_checkin_open(tournament_id, true, closes_at, grace_time)) {
			edit_logged(event, "Could not open check-in.");
			return;
		}

		edit_logged(
			event,
			"Check-in opened for tournament `" + std::to_string(tournament_id) +
			"` with `" + std::to_string(grace_time) + "` seconds of grace time."
		);
	}

	void handle_checkin_close(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		if (!tournament_manage::set_checkin_open(
			tournament_id,
			false,
			0,
			tournament_registration::DEFAULT_CHECKIN_GRACE_TIME
		)) {
			edit_logged(event, "Could not close check-in.");
			return;
		}

		edit_logged(event, "Check-in closed for tournament `" + std::to_string(tournament_id) + "`.");
	}

	MatchRules rules_from_options(const dpp::command_data_option& subcommand, const MatchRules& fallback) {
		MatchRules rules = fallback;
		rules.win_score = get_int_option(subcommand, "first_to", rules.win_score);
		rules.win_diff = get_int_option(subcommand, "win_by", rules.win_diff);
		rules.score_cap = get_int_option(subcommand, "score_cap", rules.score_cap);
		rules.allow_draw = get_bool_option(subcommand, "allow_draw", rules.allow_draw);

		const std::string deuce = get_string_option(subcommand, "deuce", tournament_ruleset::to_string(rules.deuce_mode));
		if (auto mode = tournament_ruleset::parse_deuce_mode(deuce)) {
			rules.deuce_mode = *mode;
		}

		return rules;
	}

	void handle_ruleset_show(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		const auto primary = tournament_ruleset::get_effective_primary_ruleset(tournament_id);
		const auto secondary = tournament_ruleset::get_ruleset(
			tournament_id,
			tournament_ruleset::RulesetScope::SECONDARY
		);

		std::string message =
			"Ruleset configuration for tournament `" + std::to_string(tournament_id) + "`:\n"
			"- " + tournament_ruleset::describe_ruleset(primary);

		if (secondary) {
			message += "\n- " + tournament_ruleset::describe_ruleset(*secondary);
		}
		else {
			message += "\n- secondary: disabled";
		}

		edit_logged(event, message);
	}

	void handle_ruleset_set_primary(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		tournament_ruleset::RulesetConfig config;
		config.tournament_id = get_int_option(subcommand, "id");
		config.scope = tournament_ruleset::RulesetScope::PRIMARY;
		config.trigger = tournament_ruleset::SecondaryTrigger::NONE;
		config.rules = rules_from_options(subcommand, tetrio_default_rules());

		if (!tournament_ruleset::set_ruleset(config)) {
			edit_logged(event, "Could not update the primary ruleset.");
			return;
		}

		edit_logged(event, "Primary ruleset updated: " + tournament_ruleset::describe_ruleset(config));
	}

	void handle_ruleset_set_secondary(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const std::string trigger_value = get_string_option(subcommand, "trigger");
		const auto trigger = tournament_ruleset::parse_secondary_trigger(trigger_value);
		if (!trigger || *trigger == tournament_ruleset::SecondaryTrigger::NONE) {
			edit_logged(event, "Choose a secondary ruleset trigger.");
			return;
		}

		tournament_ruleset::RulesetConfig config;
		config.tournament_id = get_int_option(subcommand, "id");
		config.scope = tournament_ruleset::RulesetScope::SECONDARY;
		config.trigger = *trigger;
		config.rules = rules_from_options(subcommand, tetrio_default_top8_rules());

		if (!tournament_ruleset::set_ruleset(config)) {
			edit_logged(event, "Could not update the secondary ruleset.");
			return;
		}

		edit_logged(event, "Secondary ruleset updated: " + tournament_ruleset::describe_ruleset(config));
	}

	void handle_ruleset_clear_secondary(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(false);

		const int tournament_id = get_int_option(subcommand, "id");
		if (!tournament_ruleset::clear_secondary_ruleset(tournament_id)) {
			edit_logged(event, "Could not clear the secondary ruleset.");
			return;
		}

		edit_logged(event, "Secondary ruleset disabled for tournament `" + std::to_string(tournament_id) + "`.");
	}

	void handle_config(const dpp::slashcommand_t& event, const dpp::command_data_option& group) {
		if (group.options.empty()) {
			reply_ephemeral(event, "Choose a tournament config subcommand.");
			return;
		}

		const auto& subcommand = group.options.front();

		if (subcommand.name == "roles") return handle_roles(event);
		if (subcommand.name == "set_staff_role") return handle_set_staff_role(event, subcommand);
		if (subcommand.name == "set_admin_role") return handle_set_admin_role(event, subcommand);
		if (subcommand.name == "clear_staff_role") return handle_clear_staff_role(event);
		if (subcommand.name == "clear_admin_role") return handle_clear_admin_role(event);
		if (subcommand.name == "set_channel") return handle_set_tournament_channel(event, subcommand);
		if (subcommand.name == "clear_channel") return handle_clear_tournament_channel(event);
		if (subcommand.name == "ruleset_show") return handle_ruleset_show(event, subcommand);
		if (subcommand.name == "ruleset_set_primary") return handle_ruleset_set_primary(event, subcommand);
		if (subcommand.name == "ruleset_set_secondary") return handle_ruleset_set_secondary(event, subcommand);
		if (subcommand.name == "ruleset_clear_secondary") return handle_ruleset_clear_secondary(event, subcommand);

		reply_ephemeral(event, "Unknown tournament config subcommand.");
	}

	void handle_bracket_group(dpp::cluster& bot, const dpp::slashcommand_t& event, const dpp::command_data_option& group) {
		if (group.options.empty()) {
			reply_ephemeral(event, "Choose a bracket subcommand.");
			return;
		}

		const auto& subcommand = group.options.front();

		if (subcommand.name == "generate") return handle_bracket_generate(event, subcommand);
		if (subcommand.name == "current") return handle_matches_current(event, subcommand);
		if (subcommand.name == "round") return handle_matches_round(event, subcommand);
		if (subcommand.name == "match") return handle_match_show(event, subcommand);
		if (subcommand.name == "report") return handle_match_report(event, subcommand);
		if (subcommand.name == "threads") return handle_match_threads(bot, event, subcommand);
		if (subcommand.name == "stream_assign") return handle_stream_assign(event, subcommand);
		if (subcommand.name == "stream_clear") return handle_stream_clear(event, subcommand);
		if (subcommand.name == "stream_list") return handle_stream_list(event, subcommand);
		if (subcommand.name == "svg") return handle_bracket_svg(event, subcommand);
		if (subcommand.name == "match_svg") return handle_match_svg(event, subcommand);

		reply_ephemeral(event, "Unknown bracket subcommand.");
	}
}

void register_tournament_commands(dpp::cluster& bot) {
	handlers["tournament"] = [&bot](const dpp::slashcommand_t& event) {
		const auto interaction = event.command.get_command_interaction();
		if (interaction.options.empty()) {
			reply_ephemeral(event, "Choose a tournament subcommand.");
			return;
		}

		const auto& subcommand = interaction.options.front();

		if (subcommand.name == "create") return handle_create(event, subcommand);
		if (subcommand.name == "edit") return handle_edit(event, subcommand);
		if (subcommand.name == "delete") return handle_delete(event, subcommand);
		if (subcommand.name == "clear") return handle_clear(event, subcommand);
		if (subcommand.name == "info") return handle_info(event, subcommand);
		if (subcommand.name == "staff_info") return handle_staff_info(event, subcommand);
		if (subcommand.name == "registration_open") return handle_registration_open(event, subcommand);
		if (subcommand.name == "registration_close") return handle_registration_close(event, subcommand);
		if (subcommand.name == "checkin_open") return handle_checkin_open(event, subcommand);
		if (subcommand.name == "checkin_close") return handle_checkin_close(event, subcommand);
		if (subcommand.name == "register") return handle_register(event, subcommand);
		if (subcommand.name == "checkin") return handle_checkin(event, subcommand);
		if (subcommand.name == "participants") return handle_participants(event, subcommand);
		if (subcommand.name == "seed") return handle_seed(event, subcommand);
		if (subcommand.name == "bracket_generate") return handle_bracket_generate(event, subcommand);
		if (subcommand.name == "matches_current") return handle_matches_current(event, subcommand);
		if (subcommand.name == "matches_round") return handle_matches_round(event, subcommand);
		if (subcommand.name == "match_show") return handle_match_show(event, subcommand);
		if (subcommand.name == "match_report") return handle_match_report(event, subcommand);
		if (subcommand.name == "match_threads") return handle_match_threads(bot, event, subcommand);
		if (subcommand.name == "stream_assign") return handle_stream_assign(event, subcommand);
		if (subcommand.name == "stream_clear") return handle_stream_clear(event, subcommand);
		if (subcommand.name == "stream_list") return handle_stream_list(event, subcommand);
		if (subcommand.name == "bracket_svg") return handle_bracket_svg(event, subcommand);
		if (subcommand.name == "match_svg") return handle_match_svg(event, subcommand);
		if (subcommand.name == "config") return handle_config(event, subcommand);
		if (subcommand.name == "bracket") return handle_bracket_group(bot, event, subcommand);
		if (subcommand.name == "roles") return handle_roles(event);
		if (subcommand.name == "set_staff_role") return handle_set_staff_role(event, subcommand);
		if (subcommand.name == "set_admin_role") return handle_set_admin_role(event, subcommand);
		if (subcommand.name == "clear_staff_role") return handle_clear_staff_role(event);
		if (subcommand.name == "clear_admin_role") return handle_clear_admin_role(event);

		reply_ephemeral(event, "Unknown tournament subcommand.");
	};
}
