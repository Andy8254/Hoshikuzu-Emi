#include <dpp/dpp.h>
#include <cstdlib>
#include "core/Config.hpp"
#include "commands/Discord_Commands.hpp"
#include "core/CommandRegistry.hpp";

int main() {
	dpp::cluster bot(get_bot_token());
	register_all_commands(bot);

	bot.on_log(dpp::utility::cout_logger());

	bot.on_ready([&bot](const dpp::ready_t& event) {
		if (dpp::run_once<struct register_bot_commands>()) {
			dpp::slashcommand help_command("help", "Show help message", bot.me.id);
			help_command.add_option(
				dpp::command_option(dpp::co_string, "category", "Help category", false)
				.add_choice(dpp::command_option_choice("Basic Commands", "fundamentals"))
				.add_choice(dpp::command_option_choice("General", "general"))
				.add_choice(dpp::command_option_choice("Player", "player"))
				.add_choice(dpp::command_option_choice("TETR.IO", "tetrio"))
				.add_choice(dpp::command_option_choice("Brackets", "brackets"))
				.add_choice(dpp::command_option_choice("Miscellaneous", "misc"))
			);

			help_command.add_option(
				dpp::command_option(dpp::co_string, "command", "Specific command", false)
			);

			bot.global_command_create(help_command);
			dpp::slashcommand command("ping", "A simple ping command", bot.me.id);
			bot.global_command_create(command);
			bot.global_command_create(
				dpp::slashcommand("info", "Show bot information", bot.me.id)
			);
		}
	});

	bot.on_slashcommand([](const dpp::slashcommand_t& event) {
		auto it = handlers.find(event.command.get_command_name());
		if (it != handlers.end()) {
			it->second(event);
		} else {
			event.reply(dpp::ir_channel_message_with_source, "Ummm... I'm afraid I don't know what you're saying... (՞•̥﹏•̥՞)");
		}
	});
	bot.start(dpp::st_wait);
}