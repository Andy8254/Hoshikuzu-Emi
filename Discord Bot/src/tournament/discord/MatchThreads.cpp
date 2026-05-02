#include "tournament/discord/MatchThreads.hpp"
#include <string>

namespace {
	std::string custom_id(const std::string& action, const tournament_bracket::StoredMatch& match) {
		return "tournament:" + action + ":" + std::to_string(match.tournament_id) + ":" + std::to_string(match.id);
	}
}

dpp::embed tournament_discord::build_match_embed(const tournament_bracket::StoredMatch& match) {
	dpp::embed embed = dpp::embed()
		.set_title("Match " + std::to_string(match.id))
		.set_color(match.streamed ? 0xe05252 : 0x5f9ea0)
		.add_field("Round", std::to_string(match.round + 1), true)
		.add_field("Position", std::to_string(match.position + 1), true)
		.add_field("State", tournament_bracket::state_to_string(match.state), true)
		.add_field("Player A", tournament_bracket::player_mention(match.player_a_id), true)
		.add_field("Player B", tournament_bracket::player_mention(match.player_b_id), true)
		.add_field("Score", std::to_string(match.score_a) + " - " + std::to_string(match.score_b), true);

	if (!match.winner_id.empty()) {
		embed.add_field("Winner", tournament_bracket::player_mention(match.winner_id), false);
	}

	if (match.streamed) {
		embed.set_footer(dpp::embed_footer().set_text("Assigned to stream"));
	}

	return embed;
}

dpp::message tournament_discord::build_match_message(const tournament_bracket::StoredMatch& match, bool include_buttons) {
	dpp::message message(
		tournament_bracket::player_mention(match.player_a_id) + " " +
		tournament_bracket::player_mention(match.player_b_id)
	);
	message.add_embed(build_match_embed(match));

	if (include_buttons) {
		dpp::component row;
		row.set_type(dpp::cot_action_row);
		row.add_component(
			dpp::component()
			.set_label("Check in")
			.set_style(dpp::cos_primary)
			.set_id(custom_id("match_checkin", match))
		);
		row.add_component(
			dpp::component()
			.set_label("Report")
			.set_style(dpp::cos_success)
			.set_id(custom_id("match_report", match))
		);
		row.add_component(
			dpp::component()
			.set_label("Call staff")
			.set_style(dpp::cos_secondary)
			.set_id(custom_id("call_staff", match))
		);
		message.add_component(row);
	}

	return message;
}

std::string tournament_discord::match_thread_name(const tournament_bracket::StoredMatch& match) {
	return "match-" + std::to_string(match.id) + "-r" + std::to_string(match.round + 1) + "p" + std::to_string(match.position + 1);
}

void tournament_discord::create_match_thread(
	dpp::cluster& bot,
	dpp::snowflake channel_id,
	const tournament_bracket::StoredMatch& match,
	bool include_buttons
) {
	bot.thread_create(
		match_thread_name(match),
		channel_id,
		1440,
		dpp::CHANNEL_PUBLIC_THREAD,
		true,
		0,
		[&bot, match, include_buttons](const dpp::confirmation_callback_t& cb) {
			if (cb.is_error()) {
				return;
			}

			const dpp::thread thread = cb.get<dpp::thread>();
			tournament_bracket::set_discord_thread(match.tournament_id, match.id, thread.id);
			bot.message_create(build_match_message(match, include_buttons).set_channel_id(thread.id));

			if (!match.player_a_id.empty()) {
				bot.thread_member_add(thread.id, dpp::snowflake(match.player_a_id));
			}
			if (!match.player_b_id.empty()) {
				bot.thread_member_add(thread.id, dpp::snowflake(match.player_b_id));
			}
		}
	);
}
