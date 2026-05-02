#include "core/CommandRegistry.hpp"
#include "core/security/PermissionManager.hpp"
#include "core/sqlite.hpp"
#include "tournament/manage.hpp"
#include "tournament/registration.hpp"
#include "tournament/seeding.hpp"
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

	dpp::snowflake get_snowflake_option(const dpp::command_data_option& parent, const std::string& name) {
		const auto* option = find_option(parent, name);
		if (!option || !std::holds_alternative<dpp::snowflake>(option->value)) {
			return 0;
		}

		return std::get<dpp::snowflake>(option->value);
	}

	void reply_ephemeral(const dpp::slashcommand_t& event, const std::string& content) {
		event.reply(dpp::message(content).set_flags(dpp::m_ephemeral));
	}

	void edit_ephemeral(const dpp::slashcommand_t& event, const std::string& content) {
		event.edit_response(dpp::message(content).set_flags(dpp::m_ephemeral));
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

	void handle_create(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(true);

		const std::string name = get_string_option(subcommand, "name");
		const std::string game = get_string_option(subcommand, "game", "tetrio");

		auto id = tournament_manage::create_tournament(name, game);
		if (!id) {
			edit_ephemeral(event, "Could not create the tournament. Check the name and try again.");
			return;
		}

		edit_ephemeral(event, "Tournament created: `" + name + "` with ID `" + std::to_string(*id) + "`.");
	}

	void handle_edit(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(true);

		tournament_manage::TournamentUpdate update;
		update.name = get_string_option(subcommand, "name");
		update.game_type = get_string_option(subcommand, "game");
		update.status = get_string_option(subcommand, "status");

		if (update.name->empty()) update.name.reset();
		if (update.game_type->empty()) update.game_type.reset();
		if (update.status->empty()) update.status.reset();

		const int tournament_id = get_int_option(subcommand, "id");
		if (!tournament_manage::update_tournament(tournament_id, update)) {
			edit_ephemeral(event, "Could not update that tournament.");
			return;
		}

		edit_ephemeral(event, "Tournament updated.");
	}

	void handle_delete(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_admin(event)) return;
		event.thinking(true);

		const int tournament_id = get_int_option(subcommand, "id");
		if (!tournament_manage::delete_tournament(tournament_id)) {
			edit_ephemeral(event, "Could not delete that tournament.");
			return;
		}

		edit_ephemeral(event, "Tournament deleted.");
	}

	void handle_register(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		event.thinking(true);
		const int tournament_id = get_int_option(subcommand, "id");
		const std::string username = get_string_option(subcommand, "username");

		auto profile = PlayerManager::get_profile(event.command.usr.id);
		const std::string linked_tetrio = profile.count("tetrio_id") ? profile["tetrio_id"] : "";

		tournament_registration::RegistrationRequest request;
		request.tournament_id = tournament_id;
		request.discord_id = std::to_string(event.command.usr.id);
		request.display_name = event.command.usr.username;
		request.provided_username = username;
		request.linked_tetrio_id = linked_tetrio;
		request.registered_at = static_cast<int>(time(nullptr));

		auto result = tournament_registration::register_player(request);
		edit_ephemeral(event, result.message);
	}

	void handle_checkin(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		event.thinking(true);
		const int tournament_id = get_int_option(subcommand, "id");
		const std::string username = get_string_option(subcommand, "username");
		const int closes_at = get_int_option(subcommand, "closes_at");
		const int grace_time = get_int_option(
			subcommand,
			"grace_time",
			tournament_registration::DEFAULT_CHECKIN_GRACE_TIME
		);

		auto profile = PlayerManager::get_profile(event.command.usr.id);
		const std::string linked_tetrio = profile.count("tetrio_id") ? profile["tetrio_id"] : "";

		tournament_registration::CheckInRequest request;
		request.tournament_id = tournament_id;
		request.discord_id = std::to_string(event.command.usr.id);
		request.provided_username = username;
		request.linked_tetrio_id = linked_tetrio;
		request.now = static_cast<int>(time(nullptr));
		request.checkin_closes_at = closes_at;
		request.grace_time = grace_time;

		auto result = tournament_registration::check_in_player(request);
		edit_ephemeral(event, result.message);
	}

	void handle_participants(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;
		event.thinking(true);

		const int tournament_id = get_int_option(subcommand, "id");
		auto participants = tournament_registration::list_participants(tournament_id);
		edit_ephemeral(event, participant_summary(participants));
	}

	void handle_seed(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_manage(event)) return;

		const int tournament_id = get_int_option(subcommand, "id");
		const std::string mode = get_string_option(subcommand, "mode", "general");
		auto participants = tournament_registration::list_checked_in_participants(tournament_id);

		std::vector<tournament_seeding::SeededPlayer> seeded;
		if (mode == "tetrio") {
			event.thinking(true);
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
		event.edit_response(message);
	}

	void handle_set_staff_role(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_role_config(event)) return;
		event.thinking(true);

		const dpp::snowflake role_id = get_snowflake_option(subcommand, "role");
		if (!GuildConfigManager::set_staff_role(event.command.guild_id, role_id)) {
			edit_ephemeral(event, "Could not set the tournament staff role.");
			return;
		}

		edit_ephemeral(event, "Tournament staff role updated.");
	}

	void handle_set_admin_role(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!require_role_config(event)) return;
		event.thinking(true);

		const dpp::snowflake role_id = get_snowflake_option(subcommand, "role");
		if (!GuildConfigManager::set_admin_role(event.command.guild_id, role_id)) {
			edit_ephemeral(event, "Could not set the tournament admin role.");
			return;
		}

		edit_ephemeral(event, "Tournament admin role updated.");
	}
}

void register_tournament_commands(dpp::cluster& bot) {
	(void)bot;

	handlers["tournament"] = [](const dpp::slashcommand_t& event) {
		const auto interaction = event.command.get_command_interaction();
		if (interaction.options.empty()) {
			reply_ephemeral(event, "Choose a tournament subcommand.");
			return;
		}

		const auto& subcommand = interaction.options.front();

		if (subcommand.name == "create") return handle_create(event, subcommand);
		if (subcommand.name == "edit") return handle_edit(event, subcommand);
		if (subcommand.name == "delete") return handle_delete(event, subcommand);
		if (subcommand.name == "register") return handle_register(event, subcommand);
		if (subcommand.name == "checkin") return handle_checkin(event, subcommand);
		if (subcommand.name == "participants") return handle_participants(event, subcommand);
		if (subcommand.name == "seed") return handle_seed(event, subcommand);
		if (subcommand.name == "set_staff_role") return handle_set_staff_role(event, subcommand);
		if (subcommand.name == "set_admin_role") return handle_set_admin_role(event, subcommand);

		reply_ephemeral(event, "Unknown tournament subcommand.");
	};
}
