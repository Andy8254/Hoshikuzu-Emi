#include <dpp/dpp.h>
#include <cstdlib>
#include "core/Config.hpp"
#include "commands/Discord_Commands.hpp"
#include "core/CommandRegistry.hpp";

int main() {
	/* disabled for now */
	//const std::string token = std::getenv("BOT_TOKEN");
	
	//for test purposes only
	dpp::cluster bot(get_bot_token());
	register_all_commands(bot);

	bot.on_log(dpp::utility::cout_logger());

	bot.on_ready([&bot](const dpp::ready_t& event) {
		if (dpp::run_once<struct register_bot_commands>()) {
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