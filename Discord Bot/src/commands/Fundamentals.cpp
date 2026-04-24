#include "core/CommandRegistry.hpp"

//Register all command handlers here (This will include hundreds, or even thousands of command handlers, so it's best to keep them organized in a separate file like this)
void register_all_commands(dpp::cluster& bot) {	
	//Pinging
	handlers["ping"] = [](const dpp::slashcommand_t& event) {
		event.reply("Pong!");
	};

	//Discord Bot Info
	handlers["info"] = [&bot](const dpp::slashcommand_t& event) {
		if (event.command.get_command_name() == "info") {
			dpp::embed info_embed = dpp::embed()
				.set_title("Bot Information")
				.set_description("Emi here! Hope I can assist you even with my modest power! (^▽^)")
				.set_color(0xB0D28F)
				.set_thumbnail(bot.me.get_avatar_url())
				.add_field("Version", "alpha1", true)
				.add_field("Library", "DPP(C++20)", true)
				.add_field("Latency", std::to_string(bot.rest_ping) + " ms", true)
				.set_footer(dpp::embed_footer().set_text("Stacking Arena : We connect the world with puzzle games."))
				.set_timestamp(time(0));
			event.reply(dpp::message().add_embed(info_embed));
		}
	};
}