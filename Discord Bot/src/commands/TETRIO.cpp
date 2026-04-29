#include "core/CommandRegistry.hpp"
#include "tetrio/TetrioService.hpp"
#include "tetrio/TetrioUtils.hpp"
#include <thread>

/*
The following section deals with data retrieved from TETR.IO.
All API logic is abstracted into tetrio/TetrioService.
*/

void register_tetrio_commands(dpp::cluster& bot) {
	auto bot_ptr = &bot;

	handlers["tetrio"] = [bot_ptr](const dpp::slashcommand_t& event) {
		std::string username = std::get<std::string>(event.get_parameter("username"));

		event.thinking();

		std::thread([event, username]() mutable {
			auto profile = TetrioService::fetch_user(username);

			if (!profile.has_value()) {
				event.edit_response("❌ User not found or API error.");
				return;
			}

			//embed generation
			std::string avatar_url = "https://tetr.io/user-content/avatars/" + profile->id + ".jpg";

			dpp::embed embed = dpp::embed()
				.set_title(profile->username + "'s Profile")
				.set_url("https://tetr.io/#u/" + profile->username)
				.set_thumbnail(avatar_url)
				.set_color(get_rank_colour(profile->rank))
				.add_field("Rank", profile->rank, true)
				.add_field("Rating", std::to_string(profile->rating), true)
				.add_field(
					"Stats",
					"**APM**: " + format_double(profile->apm) + "\n" +
					"**PPS**: " + format_double(profile->pps) + "\n" +
					"**VS**: " + format_double(profile->vs),
					true
				)
				.set_footer(dpp::embed_footer().set_text("Data retrieved from TETR.IO's TETRA CHANNEL"))
				.set_timestamp(time(nullptr));

			event.edit_response(dpp::message().add_embed(embed));
		});
	};
}