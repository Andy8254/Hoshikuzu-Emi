#include "core/CommandRegistry.hpp"
#include "core/sqlite.hpp"
#include <algorithm>
#include <ctime>
#include <sstream>

namespace {
	bool has_role(const dpp::slashcommand_t& event, dpp::snowflake role_id) {
		if (!role_id) return false;
		const auto& roles = event.command.member.get_roles();
		return std::find(roles.begin(), roles.end(), role_id) != roles.end();
	}

	bool has_discord_permission(const dpp::slashcommand_t& event, dpp::permission permission) {
		const auto perm_it = event.command.resolved.member_permissions.find(event.command.usr.id);
		return perm_it != event.command.resolved.member_permissions.end()
			&& (perm_it->second & permission);
	}

	bool is_owner_or_developer(const dpp::slashcommand_t& event) {
		return event.command.usr.id == ServerSettingsManager::DEVELOPER_ID
			|| event.command.usr.id == ServerSettingsManager::get_owner(event.command.guild_id);
	}

	bool is_mod(const dpp::slashcommand_t& event) {
		return is_owner_or_developer(event)
			|| has_discord_permission(event, dpp::p_administrator)
			|| has_discord_permission(event, dpp::p_manage_guild)
			|| has_role(event, ServerSettingsManager::get_admin_role(event.command.guild_id))
			|| has_role(event, ServerSettingsManager::get_moderator_role(event.command.guild_id));
	}

	bool is_admin(const dpp::slashcommand_t& event) {
		return is_owner_or_developer(event)
			|| has_discord_permission(event, dpp::p_administrator)
			|| has_role(event, ServerSettingsManager::get_admin_role(event.command.guild_id));
	}

	void reply_ephemeral(const dpp::slashcommand_t& event, const std::string& content) {
		event.reply(dpp::message(content).set_flags(dpp::m_ephemeral));
	}

	const dpp::command_data_option* find_option(const dpp::command_data_option& parent, const std::string& name) {
		for (const auto& option : parent.options) {
			if (option.name == name) return &option;
		}
		return nullptr;
	}

	dpp::snowflake get_snowflake_option(const dpp::command_data_option& parent, const std::string& name) {
		const auto* option = find_option(parent, name);
		if (!option || !std::holds_alternative<dpp::snowflake>(option->value)) return 0;
		return std::get<dpp::snowflake>(option->value);
	}

	int get_int_option(const dpp::command_data_option& parent, const std::string& name, int fallback = 0) {
		const auto* option = find_option(parent, name);
		if (!option || !std::holds_alternative<int64_t>(option->value)) return fallback;
		return static_cast<int>(std::get<int64_t>(option->value));
	}

	std::string get_string_option(const dpp::command_data_option& parent, const std::string& name, const std::string& fallback = "") {
		const auto* option = find_option(parent, name);
		if (!option || !std::holds_alternative<std::string>(option->value)) return fallback;
		return std::get<std::string>(option->value);
	}

	std::string default_reason(const std::string& reason) {
		return reason.empty() ? "No reason provided." : reason;
	}

	std::string neutralise_mass_mentions(std::string value) {
		const std::pair<std::string, std::string> replacements[] = {
			{ "@everyone", "@ everyone" },
			{ "@here", "@ here" }
		};

		for (const auto& [needle, replacement] : replacements) {
			size_t pos = 0;
			while ((pos = value.find(needle, pos)) != std::string::npos) {
				value.replace(pos, needle.size(), replacement);
				pos += replacement.size();
			}
		}

		return value;
	}

	bool invalid_target(const dpp::slashcommand_t& event, dpp::snowflake target_id) {
		return !event.command.guild_id
			|| !target_id
			|| target_id == event.command.usr.id
			|| target_id == ServerSettingsManager::DEVELOPER_ID
			|| target_id == ServerSettingsManager::get_owner(event.command.guild_id);
	}

	void log_case(dpp::cluster& bot, const dpp::slashcommand_t& event, int case_id, dpp::snowflake target_id, const std::string& action, const std::string& reason, int duration = 0) {
		const dpp::snowflake channel_id = ServerSettingsManager::get_modlog_channel(event.command.guild_id);
		if (!channel_id) return;

		dpp::embed embed = dpp::embed()
			.set_title("Moderation Case #" + std::to_string(case_id))
			.set_color(0xc07070)
			.add_field("Action", action, true)
			.add_field("Target", "<@" + std::to_string(target_id) + ">", true)
			.add_field("Moderator", "<@" + std::to_string(event.command.usr.id) + ">", true)
			.add_field("Reason", neutralise_mass_mentions(reason), false)
			.set_timestamp(time(nullptr));

		if (duration > 0) {
			embed.add_field("Duration", std::to_string(duration) + " seconds", true);
		}

		bot.message_create(dpp::message(channel_id, "").add_embed(embed));
	}

	std::optional<int> create_and_log(dpp::cluster& bot, const dpp::slashcommand_t& event, dpp::snowflake target_id, const std::string& action, const std::string& reason, int duration = 0) {
		auto case_id = ModerationManager::create_case(
			event.command.guild_id,
			target_id,
			event.command.usr.id,
			action,
			neutralise_mass_mentions(reason),
			duration
		);
		if (case_id) {
			log_case(bot, event, *case_id, target_id, action, reason, duration);
		}
		return case_id;
	}

	void handle_warn(dpp::cluster& bot, const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!is_mod(event)) return reply_ephemeral(event, "You need moderator permission to warn users.");
		const dpp::snowflake target_id = get_snowflake_option(subcommand, "user");
		if (invalid_target(event, target_id)) return reply_ephemeral(event, "Invalid moderation target.");
		const std::string reason = neutralise_mass_mentions(default_reason(get_string_option(subcommand, "reason")));
		auto case_id = create_and_log(bot, event, target_id, "warn", reason);
		event.reply(dpp::message(case_id ? "Warning recorded as case `" + std::to_string(*case_id) + "`." : "Could not record warning.").set_flags(dpp::m_ephemeral));
	}

	void handle_note(dpp::cluster& bot, const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!is_mod(event)) return reply_ephemeral(event, "You need moderator permission to add notes.");
		const dpp::snowflake target_id = get_snowflake_option(subcommand, "user");
		if (invalid_target(event, target_id)) return reply_ephemeral(event, "Invalid moderation target.");
		const std::string note = neutralise_mass_mentions(default_reason(get_string_option(subcommand, "note")));
		auto case_id = create_and_log(bot, event, target_id, "note", note);
		event.reply(dpp::message(case_id ? "Note recorded as case `" + std::to_string(*case_id) + "`." : "Could not record note.").set_flags(dpp::m_ephemeral));
	}

	void handle_history(const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!is_mod(event)) return reply_ephemeral(event, "You need moderator permission to view history.");
		const dpp::snowflake target_id = get_snowflake_option(subcommand, "user");
		if (!target_id) return reply_ephemeral(event, "Choose a user.");

		const auto cases = ModerationManager::list_cases(event.command.guild_id, target_id, 10);
		if (cases.empty()) return event.reply(dpp::message("No moderation history found for <@" + std::to_string(target_id) + ">.").set_flags(dpp::m_ephemeral));

		std::ostringstream out;
		out << "Moderation history for <@" << target_id << ">:\n";
		for (const auto& entry : cases) {
			out << "- #" << entry.id << " `" << entry.action << "` by <@" << entry.actor_id << ">: " << neutralise_mass_mentions(entry.reason) << "\n";
		}
		event.reply(dpp::message(out.str()).set_flags(dpp::m_ephemeral));
	}

	void live_action_callback(dpp::cluster& bot, const dpp::slashcommand_t& event, dpp::snowflake target_id, const std::string& action, const std::string& reason, int duration = 0) {
		create_and_log(bot, event, target_id, action, reason, duration);
		event.edit_response(action + " completed for <@" + std::to_string(target_id) + ">.");
	}

	void handle_timeout(dpp::cluster& bot, const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!is_mod(event)) return reply_ephemeral(event, "You need moderator permission to timeout users.");
		const dpp::snowflake target_id = get_snowflake_option(subcommand, "user");
		const int duration = get_int_option(subcommand, "duration");
		if (invalid_target(event, target_id) || duration <= 0 || duration > 2419200) return reply_ephemeral(event, "Invalid target or duration.");
		const std::string reason = default_reason(get_string_option(subcommand, "reason"));
		event.thinking(false);
		bot.guild_member_timeout(event.command.guild_id, target_id, time(nullptr) + duration, [&bot, event, target_id, reason, duration](const dpp::confirmation_callback_t& cb) mutable {
			if (cb.is_error()) return event.edit_response("Timeout failed.");
			live_action_callback(bot, event, target_id, "timeout", reason, duration);
		});
	}

	void handle_clear_timeout(dpp::cluster& bot, const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!is_mod(event)) return reply_ephemeral(event, "You need moderator permission to clear timeouts.");
		const dpp::snowflake target_id = get_snowflake_option(subcommand, "user");
		if (invalid_target(event, target_id)) return reply_ephemeral(event, "Invalid moderation target.");
		const std::string reason = default_reason(get_string_option(subcommand, "reason"));
		event.thinking(false);
		bot.guild_member_timeout_remove(event.command.guild_id, target_id, [&bot, event, target_id, reason](const dpp::confirmation_callback_t& cb) mutable {
			if (cb.is_error()) return event.edit_response("Clear timeout failed.");
			live_action_callback(bot, event, target_id, "clear_timeout", reason);
		});
	}

	void handle_kick(dpp::cluster& bot, const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!is_mod(event)) return reply_ephemeral(event, "You need moderator permission to kick users.");
		const dpp::snowflake target_id = get_snowflake_option(subcommand, "user");
		if (invalid_target(event, target_id)) return reply_ephemeral(event, "Invalid moderation target.");
		const std::string reason = default_reason(get_string_option(subcommand, "reason"));
		event.thinking(false);
		bot.guild_member_kick(event.command.guild_id, target_id, [&bot, event, target_id, reason](const dpp::confirmation_callback_t& cb) mutable {
			if (cb.is_error()) return event.edit_response("Kick failed.");
			live_action_callback(bot, event, target_id, "kick", reason);
		});
	}

	void handle_ban(dpp::cluster& bot, const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!is_admin(event)) return reply_ephemeral(event, "You need admin permission to ban users.");
		const dpp::snowflake target_id = get_snowflake_option(subcommand, "user");
		if (invalid_target(event, target_id)) return reply_ephemeral(event, "Invalid moderation target.");
		const std::string reason = default_reason(get_string_option(subcommand, "reason"));
		event.thinking(false);
		bot.guild_ban_add(event.command.guild_id, target_id, 0, [&bot, event, target_id, reason](const dpp::confirmation_callback_t& cb) mutable {
			if (cb.is_error()) return event.edit_response("Ban failed.");
			live_action_callback(bot, event, target_id, "ban", reason);
		});
	}

	void handle_unban(dpp::cluster& bot, const dpp::slashcommand_t& event, const dpp::command_data_option& subcommand) {
		if (!is_admin(event)) return reply_ephemeral(event, "You need admin permission to unban users.");
		const dpp::snowflake target_id = get_snowflake_option(subcommand, "user");
		if (!target_id) return reply_ephemeral(event, "Choose a user ID.");
		const std::string reason = default_reason(get_string_option(subcommand, "reason"));
		event.thinking(false);
		bot.guild_ban_delete(event.command.guild_id, target_id, [&bot, event, target_id, reason](const dpp::confirmation_callback_t& cb) mutable {
			if (cb.is_error()) return event.edit_response("Unban failed.");
			live_action_callback(bot, event, target_id, "unban", reason);
		});
	}
}

void register_moderation_commands(dpp::cluster& bot) {
	handlers["mod"] = [&bot](const dpp::slashcommand_t& event) {
		const auto interaction = event.command.get_command_interaction();
		if (interaction.options.empty()) return reply_ephemeral(event, "Choose a moderation subcommand.");
		const auto& subcommand = interaction.options.front();

		if (subcommand.name == "warn") return handle_warn(bot, event, subcommand);
		if (subcommand.name == "note") return handle_note(bot, event, subcommand);
		if (subcommand.name == "history") return handle_history(event, subcommand);
		if (subcommand.name == "timeout") return handle_timeout(bot, event, subcommand);
		if (subcommand.name == "clear_timeout") return handle_clear_timeout(bot, event, subcommand);
		if (subcommand.name == "kick") return handle_kick(bot, event, subcommand);
		if (subcommand.name == "ban") return handle_ban(bot, event, subcommand);
		if (subcommand.name == "unban") return handle_unban(bot, event, subcommand);

		reply_ephemeral(event, "Unknown moderation subcommand.");
	};
}
