#include "core/CommandRegistry.hpp"
#include "core/Localization.hpp"
#include "core/sqlite.hpp"
#include <algorithm>
#include <sstream>

namespace {
	bool has_role(const dpp::slashcommand_t& event, dpp::snowflake role_id) {
		if (!role_id) {
			return false;
		}

		const auto& roles = event.command.member.get_roles();
		return std::find(roles.begin(), roles.end(), role_id) != roles.end();
	}

	bool is_developer(const dpp::slashcommand_t& event) {
		return event.command.usr.id == ServerSettingsManager::DEVELOPER_ID;
	}

	bool is_owner(const dpp::slashcommand_t& event) {
		return is_developer(event)
			|| (event.command.guild_id
				&& event.command.usr.id == ServerSettingsManager::get_owner(event.command.guild_id));
	}

	bool has_discord_admin(const dpp::slashcommand_t& event) {
		const auto perm_it = event.command.resolved.member_permissions.find(event.command.usr.id);
		return perm_it != event.command.resolved.member_permissions.end()
			&& (perm_it->second & dpp::p_administrator);
	}

	bool is_settings_admin(const dpp::slashcommand_t& event) {
		return is_owner(event)
			|| has_discord_admin(event)
			|| has_role(event, ServerSettingsManager::get_admin_role(event.command.guild_id));
	}

	bool is_settings_moderator(const dpp::slashcommand_t& event) {
		return is_settings_admin(event)
			|| has_role(event, ServerSettingsManager::get_moderator_role(event.command.guild_id));
	}

	void reply_ephemeral(const dpp::slashcommand_t& event, const std::string& content) {
		event.reply(dpp::message(content).set_flags(dpp::m_ephemeral));
	}

	dpp::snowflake get_snowflake_option(const dpp::command_data_option& parent, const std::string& name) {
		for (const auto& option : parent.options) {
			if (option.name == name && std::holds_alternative<dpp::snowflake>(option.value)) {
				return std::get<dpp::snowflake>(option.value);
			}
		}

		return 0;
	}

	std::string get_string_option(const dpp::command_data_option& parent, const std::string& name, const std::string& fallback = "") {
		for (const auto& option : parent.options) {
			if (option.name == name && std::holds_alternative<std::string>(option.value)) {
				return std::get<std::string>(option.value);
			}
		}

		return fallback;
	}

	std::string role_display(dpp::snowflake role_id) {
		return role_id ? "<@&" + std::to_string(role_id) + ">" : "Not configured";
	}

	std::string secondary_language_display(const std::string& language) {
		if (language.empty()) {
			return "None";
		}

		if (language == localization::DEFAULT_SECONDARY_SENTINEL) {
			return std::string("Default (") + localization::DEFAULT_SECONDARY_LANGUAGE + ")";
		}

		return language;
	}

	dpp::embed build_settings_embed(const dpp::slashcommand_t& event) {
		const dpp::snowflake guild_id = event.command.guild_id;
		return dpp::embed()
			.set_title("Server Settings")
			.set_color(0x7aa2f7)
			.add_field("Developer", "<@543676141177798676>", true)
			.add_field("Owner", ServerSettingsManager::get_owner(guild_id) ? "<@" + std::to_string(ServerSettingsManager::get_owner(guild_id)) + ">" : "Not assigned", true)
			.add_field("Language", ServerSettingsManager::get_language(guild_id), true)
			.add_field("Secondary language", secondary_language_display(ServerSettingsManager::get_secondary_language(guild_id)), true)
			.add_field("Admin role", role_display(ServerSettingsManager::get_admin_role(guild_id)), true)
			.add_field("Moderator role", role_display(ServerSettingsManager::get_moderator_role(guild_id)), true)
			.add_field("Staff role", role_display(ServerSettingsManager::get_staff_role(guild_id)), true)
			.add_field("Moderation log", ServerSettingsManager::get_modlog_channel(guild_id) ? "<#" + std::to_string(ServerSettingsManager::get_modlog_channel(guild_id)) + ">" : "Not configured", true);
	}

	dpp::component dashboard_buttons() {
		dpp::component actions;
		actions.add_component(
			dpp::component()
			.set_label("Settings help")
			.set_id("staffdash:settings:help:0")
			.set_style(dpp::cos_secondary)
		).add_component(
			dpp::component()
			.set_label("Moderation help")
			.set_id("staffdash:moderation:help:0")
			.set_style(dpp::cos_secondary)
		);
		return actions;
	}

	void handle_dashboard(const dpp::slashcommand_t& event) {
		if (!is_settings_moderator(event)) {
			reply_ephemeral(event, localization::user_text(event.command.guild_id, event.command.usr.id, "settings.permission.staff"));
			return;
		}

		dpp::embed embed = build_settings_embed(event)
			.set_title("Settings Dashboard")
			.set_description("Configuration snapshot for server language, staff roles, and moderation logging.");

		event.reply(dpp::message().add_embed(embed).add_component(dashboard_buttons()).set_flags(dpp::m_ephemeral));
	}

	void handle_show(const dpp::slashcommand_t& event) {
		if (!is_settings_moderator(event)) {
			reply_ephemeral(event, localization::user_text(event.command.guild_id, event.command.usr.id, "settings.permission.staff"));
			return;
		}

		event.reply(dpp::message().add_embed(build_settings_embed(event)));
	}

	void handle_set_admin_role(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!is_owner(event)) {
			reply_ephemeral(event, localization::user_text(event.command.guild_id, event.command.usr.id, "settings.permission.owner"));
			return;
		}

		const dpp::snowflake role_id = get_snowflake_option(subcommand, "role");
		if (!ServerSettingsManager::set_admin_role(event.command.guild_id, role_id)) {
			reply_ephemeral(event, "Could not set the admin role.");
			return;
		}

		event.reply("Admin role updated: " + role_display(role_id));
	}

	void handle_set_moderator_role(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!is_settings_admin(event)) {
			reply_ephemeral(event, localization::user_text(event.command.guild_id, event.command.usr.id, "settings.permission.admin"));
			return;
		}

		const dpp::snowflake role_id = get_snowflake_option(subcommand, "role");
		if (!ServerSettingsManager::set_moderator_role(event.command.guild_id, role_id)) {
			reply_ephemeral(event, "Could not set the moderator role.");
			return;
		}

		event.reply("Moderator role updated: " + role_display(role_id));
	}

	void handle_set_staff_role(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!is_settings_moderator(event)) {
			reply_ephemeral(event, localization::user_text(event.command.guild_id, event.command.usr.id, "settings.permission.moderator"));
			return;
		}

		const dpp::snowflake role_id = get_snowflake_option(subcommand, "role");
		if (!ServerSettingsManager::set_staff_role(event.command.guild_id, role_id)) {
			reply_ephemeral(event, "Could not set the staff role.");
			return;
		}

		event.reply("Staff role updated: " + role_display(role_id));
	}

	void handle_language(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!is_settings_moderator(event)) {
			reply_ephemeral(event, localization::user_text(event.command.guild_id, event.command.usr.id, "settings.permission.moderator"));
			return;
		}

		const std::string language = get_string_option(subcommand, "language", "EN-gb");
		if (!localization::is_supported_language(language)) {
			reply_ephemeral(event, localization::user_text(event.command.guild_id, event.command.usr.id, "settings.language.unsupported"));
			return;
		}

		if (!ServerSettingsManager::set_language(event.command.guild_id, language)) {
			reply_ephemeral(event, "Could not update the language.");
			return;
		}

		event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "settings.language.updated", { { "language", language } }));
	}

	void handle_secondary_language(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!is_settings_moderator(event)) {
			reply_ephemeral(event, localization::user_text(event.command.guild_id, event.command.usr.id, "settings.permission.moderator"));
			return;
		}

		const std::string language = get_string_option(subcommand, "language", "none");
		if (language == "none") {
			if (!ServerSettingsManager::clear_secondary_language(event.command.guild_id)) {
				reply_ephemeral(event, "Could not update the secondary language.");
				return;
			}

			event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "settings.secondary_language.cleared"));
			return;
		}

		if (language != localization::DEFAULT_SECONDARY_SENTINEL && !localization::is_supported_language(language)) {
			reply_ephemeral(event, localization::user_text(event.command.guild_id, event.command.usr.id, "settings.language.unsupported"));
			return;
		}

		if (!ServerSettingsManager::set_secondary_language(event.command.guild_id, language)) {
			reply_ephemeral(event, "Could not update the secondary language.");
			return;
		}

		event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "settings.secondary_language.updated", { { "language", language } }));
	}

	void handle_modlog_set(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!is_settings_admin(event)) {
			reply_ephemeral(event, localization::user_text(event.command.guild_id, event.command.usr.id, "settings.permission.admin"));
			return;
		}

		const dpp::snowflake channel_id = get_snowflake_option(subcommand, "channel");
		if (!ServerSettingsManager::set_modlog_channel(event.command.guild_id, channel_id)) {
			reply_ephemeral(event, "Could not set the moderation log channel.");
			return;
		}

		event.reply("Moderation log channel updated: <#" + std::to_string(channel_id) + ">.");
	}

	void handle_modlog_clear(const dpp::slashcommand_t& event) {
		if (!is_settings_admin(event)) {
			reply_ephemeral(event, localization::user_text(event.command.guild_id, event.command.usr.id, "settings.permission.admin"));
			return;
		}

		if (!ServerSettingsManager::clear_modlog_channel(event.command.guild_id)) {
			reply_ephemeral(event, "Could not clear the moderation log channel.");
			return;
		}

		event.reply("Moderation log channel cleared.");
	}
}

void register_settings_commands(dpp::cluster& bot) {
	handlers["settings"] = [](const dpp::slashcommand_t& event) {
		const auto interaction = event.command.get_command_interaction();
		if (interaction.options.empty()) {
			return handle_dashboard(event);
		}

		const auto& subcommand = interaction.options.front();
		if (subcommand.name == "show") return handle_show(event);
		if (subcommand.name == "set_admin_role") return handle_set_admin_role(event, subcommand);
		if (subcommand.name == "set_moderator_role") return handle_set_moderator_role(event, subcommand);
		if (subcommand.name == "set_staff_role") return handle_set_staff_role(event, subcommand);
		if (subcommand.name == "language") return handle_language(event, subcommand);
		if (subcommand.name == "secondary_language") return handle_secondary_language(event, subcommand);
		if (subcommand.name == "modlog_set") return handle_modlog_set(event, subcommand);
		if (subcommand.name == "modlog_clear") return handle_modlog_clear(event);

		reply_ephemeral(event, localization::user_text(event.command.guild_id, event.command.usr.id, "settings.unknown"));
	};
}
