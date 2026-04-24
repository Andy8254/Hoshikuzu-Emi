#include "core/CommandRegistry.hpp"
#include "core/Markdown.hpp"

//General Section, even though this will be a FAQ section since the basic commands were already registered in Fundamentals.cpp...
void register_general_commands(dpp::cluster& bot) {
	//Section List
	handlers["help"] = [&bot](const dpp::slashcommand_t& event) {

		std::string category, command;
		if (event.get_parameter("category").index() != 0) {
			category = std::get<std::string>(event.get_parameter("category"));
		}
		if (event.get_parameter("command").index() != 0) {
			command = std::get<std::string>(event.get_parameter("command"));
		}

		std::transform(category.begin(), category.end(), category.begin(), ::tolower);
		std::transform(command.begin(), command.end(), command.begin(), ::tolower);

		if (category.empty()) {
			dpp::embed help_embed = dpp::embed()
				.set_title("Help Menu")
				.set_description("Select a category to view commands.")
				.set_thumbnail(bot.me.get_avatar_url())
				.set_color(0xB0D28F)
				.add_field("Basic", "`/help fundamentals`", true)
				.add_field("General", "`/help general`", true)
				.add_field("Player", "`/help player`", true)
				.add_field("TETR.IO", "`/help tetrio`", true)
				.add_field("Brackets", "`/help brackets`", true)
				.add_field("Miscellaneous", "`/help misc`", true)
				.set_footer(dpp::embed_footer().set_text("Use /help [category] for more information on commands in that category."))
				.set_timestamp(time(0));
			//moderation hidden for now since it's reserved for authorised users only and may be expanded in the future
				//.add_field("Moderation", "`/help moderation`", true);

			event.reply(dpp::message().add_embed(help_embed));
			return;
		}

		if (command.empty()) {
			std::string path = "resources/help/" + category + "list.md";
			std::string content = md::read_file(path);

			if (content.empty()) {
				event.reply("Category not found. Please check your spelling and try again.");
				return;
			}

			auto msg = md::to_message(content);
			if (!msg.embeds.empty()) {
				msg.embeds[0].set_thumbnail(bot.me.get_avatar_url());
			}

			event.reply(msg);
			return;
		}

		std::string path = "resources/help/" + category + "/" + command + ".md";
		std::string content = md::read_file(path);

		if (content.empty()) {
			event.reply("Command not found. Please check your spelling and try again.");
			return;
		}

		auto msg = md::to_message(content);
		if (!msg.embeds.empty()) {
			msg.embeds[0].set_timestamp(time(0));
		}
		event.reply(msg);
	};
}