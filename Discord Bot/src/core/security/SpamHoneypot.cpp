#include "core/security/SpamHoneypot.hpp"
#include "core/sqlite.hpp"
#include <algorithm>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

namespace {
	const std::string HONEYPOT_REASON = "Automated honeypot ban: message sent in configured honeypot channel.";

	bool has_role(const dpp::guild_member& member, dpp::snowflake role_id) {
		if (!role_id) {
			return false;
		}

		const auto& roles = member.get_roles();
		return std::find(roles.begin(), roles.end(), role_id) != roles.end();
	}

	bool is_exempt(const dpp::message& message) {
		if (message.author.is_bot()) {
			return true;
		}

		const dpp::snowflake guild_id = message.guild_id;
		const dpp::snowflake user_id = message.author.id;
		if (!guild_id || !user_id) {
			return true;
		}

		if (user_id == ServerSettingsManager::DEVELOPER_ID
			|| user_id == ServerSettingsManager::get_owner(guild_id)) {
			return true;
		}

		return has_role(message.member, ServerSettingsManager::get_admin_role(guild_id))
			|| has_role(message.member, ServerSettingsManager::get_moderator_role(guild_id))
			|| has_role(message.member, ServerSettingsManager::get_staff_role(guild_id));
	}

	void log_honeypot_case(dpp::cluster& bot, const dpp::message& message, int case_id) {
		const dpp::snowflake modlog_channel = ServerSettingsManager::get_modlog_channel(message.guild_id);
		if (!modlog_channel) {
			return;
		}

		dpp::embed embed = dpp::embed()
			.set_title("Moderation Case #" + std::to_string(case_id))
			.set_color(0xc07070)
			.add_field("Action", "auto_ban", true)
			.add_field("Trigger", "Message sent in honeypot channel", false)
			.add_field("Channel", "<#" + std::to_string(message.channel_id) + ">", true)
			.add_field("Target", "<@" + std::to_string(message.author.id) + ">", true)
			.add_field("Moderator", "<@" + std::to_string(bot.me.id) + ">", true)
			.add_field("Reason", HONEYPOT_REASON, false)
			.set_timestamp(time(nullptr));

		bot.message_create(dpp::message(modlog_channel, "").add_embed(embed));
	}

	void release_pending(std::shared_ptr<std::mutex> mutex, std::shared_ptr<std::unordered_set<dpp::snowflake>> pending, dpp::snowflake user_id) {
		std::lock_guard<std::mutex> lock(*mutex);
		pending->erase(user_id);
	}
}

void register_spam_honeypot(dpp::cluster& bot) {
	auto pending_mutex = std::make_shared<std::mutex>();
	auto pending_bans = std::make_shared<std::unordered_set<dpp::snowflake>>();

	bot.on_message_create([&bot, pending_mutex, pending_bans](const dpp::message_create_t& event) {
		const dpp::message& message = event.msg;
		if (!message.guild_id || !message.author.id) {
			return;
		}

		const dpp::snowflake honeypot_channel = ServerSettingsManager::get_honeypot_channel(message.guild_id);
		if (!honeypot_channel || message.channel_id != honeypot_channel || is_exempt(message)) {
			return;
		}

		{
			std::lock_guard<std::mutex> lock(*pending_mutex);
			if (!pending_bans->insert(message.author.id).second) {
				return;
			}
		}

		bot.message_delete(message.id, message.channel_id);
		bot.guild_ban_add(message.guild_id, message.author.id, 0, [&bot, message, pending_mutex, pending_bans](const dpp::confirmation_callback_t& cb) {
			if (cb.is_error()) {
				release_pending(pending_mutex, pending_bans, message.author.id);
				return;
			}

			auto case_id = ModerationManager::create_case(
				message.guild_id,
				message.author.id,
				bot.me.id,
				"auto_ban",
				HONEYPOT_REASON
			);

			if (case_id) {
				log_honeypot_case(bot, message, *case_id);
			}

			release_pending(pending_mutex, pending_bans, message.author.id);
		});
	});
}
