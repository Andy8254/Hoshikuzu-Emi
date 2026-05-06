#include "core/CommandRegistry.hpp"
#include "core/Localization.hpp"
#include "core/Markdown.hpp"
#include <algorithm>
#include <cctype>

namespace {
	std::string normalize_help_token(std::string value) {
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
			if (c == '-') {
				return static_cast<char>('_');
			}

			return static_cast<char>(std::tolower(c));
		});

		std::string normalized;
		for (const char c : value) {
			if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
				normalized.push_back(c);
			}
		}

		return normalized;
	}

	std::string help_path(const std::string& language, const std::string& module, const std::string& command) {
		return "resources/help/" + language + "/" + module + "/" + command + ".md";
	}

	std::string read_help_file(const std::string& language, const std::string& module, const std::string& command) {
		const std::string localized_path = help_path(language, module, command);
		std::string content = md::read_file(localized_path);
		if (!content.empty()) {
			return content;
		}

		if (language != localization::DEFAULT_LANGUAGE) {
			return md::read_file(help_path(localization::DEFAULT_LANGUAGE, module, command));
		}

		return "";
	}
}

//General Section, even though this will be a FAQ section since the basic commands were already registered in Fundamentals.cpp...
void register_general_commands(dpp::cluster& bot) {
	//Section List
	auto bot_ptr = &bot;

	handlers["codex"] = [bot_ptr](const dpp::slashcommand_t& event) {
		std::string category, command;
		auto cat_param = event.get_parameter("category");

		if (std::holds_alternative<std::string>(cat_param)) {
			category = std::get<std::string>(event.get_parameter("category"));
		}

		auto cmd_param = event.get_parameter("command");

		if (std::holds_alternative<std::string>(cmd_param)) {
			command = std::get<std::string>(event.get_parameter("command"));
		}

		category = normalize_help_token(category);
		command = normalize_help_token(command);
		const std::string language = localization::primary_language(event.command.guild_id, event.command.usr.id);

		if (category.empty()) {
			std::string content = read_help_file(language, "bot", "list");
			if (content.empty()) {
				event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "help.unavailable"));
				return;
			}

			auto msg = md::to_message(content);
			if (!msg.embeds.empty()) {
				msg.embeds[0].set_thumbnail(bot_ptr->me.get_avatar_url()).set_timestamp(time(0));
			}

			event.reply(msg);
			return;
		}

		if (command.empty()) {
			std::string content = read_help_file(language, category, "list");

			if (content.empty()) {
				event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "help.category_not_found"));
				return;
			}

			auto msg = md::to_message(content);
			if (!msg.embeds.empty()) {
				msg.embeds[0].set_thumbnail(bot_ptr->me.get_avatar_url());
			}

			event.reply(msg);
			return;
		}

		std::string content = read_help_file(language, category, command);

		if (content.empty()) {
			event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "help.command_not_found"));
			return;
		}

		auto msg = md::to_message(content);
		if (!msg.embeds.empty()) {
			msg.embeds[0].set_timestamp(time(0));
		}
		event.reply(msg);
	};
}
