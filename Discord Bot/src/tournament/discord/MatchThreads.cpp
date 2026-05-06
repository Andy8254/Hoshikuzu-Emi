#include "tournament/discord/MatchThreads.hpp"
#include "core/Localization.hpp"
#include "tournament/matchmaking.hpp"
#include <ctime>
#include <string>

namespace {
	std::string custom_id(const std::string& action, const tournament_bracket::StoredMatch& match) {
		return "tournament:" + action + ":" + std::to_string(match.tournament_id) + ":" + std::to_string(match.id);
	}

	bool actionable(const tournament_bracket::StoredMatch& match) {
		return match.state == tournament_bracket::StoredMatchState::Ready
			|| match.state == tournament_bracket::StoredMatchState::Ongoing;
	}

	std::string et(dpp::snowflake guild_id, const std::string& key, const localization::Params& params = {}) {
		return localization::shared_embed_text(guild_id, key, params);
	}
}

dpp::embed tournament_discord::build_match_embed(dpp::snowflake guild_id, const tournament_bracket::StoredMatch& match) {
	dpp::embed embed = dpp::embed()
		.set_title(et(guild_id, "embed.match.title", { { "match_id", std::to_string(match.id) } }))
		.set_description(et(guild_id, "embed.match.description"))
		.set_color(match.streamed ? 0xe05252 : 0x5f9ea0)
		.add_field(et(guild_id, "embed.match.field.round"), std::to_string(match.round + 1), true)
		.add_field(et(guild_id, "embed.match.field.position"), std::to_string(match.position + 1), true)
		.add_field(et(guild_id, "embed.match.field.state"), tournament_bracket::state_to_string(match.state), true)
		.add_field(et(guild_id, "embed.match.field.player_a"), tournament_bracket::player_mention(match.player_a_id), true)
		.add_field(et(guild_id, "embed.match.field.player_b"), tournament_bracket::player_mention(match.player_b_id), true)
		.add_field(et(guild_id, "embed.match.field.score"), std::to_string(match.score_a) + " - " + std::to_string(match.score_b), true)
		.add_field(et(guild_id, "embed.match.field.checkins"), std::string(match.player_a_checked_in ? "A yes" : "A no") + " / " +
			(match.player_b_checked_in ? "B yes" : "B no"), true);

	if (match.match_opened_at > 0) {
		embed.add_field(et(guild_id, "embed.match.field.grace_until"), std::to_string(match.match_opened_at + match.grace_time), true);
	}

	if (!match.winner_id.empty()) {
		embed.add_field(et(guild_id, "embed.match.field.winner"), tournament_bracket::player_mention(match.winner_id), false);
	}

	if (match.streamed) {
		embed.set_footer(dpp::embed_footer().set_text(et(guild_id, "embed.match.footer.stream")));
	}

	return embed;
}

dpp::message tournament_discord::build_match_message(dpp::snowflake guild_id, const tournament_bracket::StoredMatch& match, bool include_buttons) {
	dpp::message message(
		tournament_bracket::player_mention(match.player_a_id) + " " +
		tournament_bracket::player_mention(match.player_b_id)
	);
	message.add_embed(build_match_embed(guild_id, match));

	if (include_buttons) {
		dpp::component row;
		row.set_type(dpp::cot_action_row);
		const bool enabled = actionable(match);
		row.add_component(
			dpp::component()
			.set_label("Check in")
			.set_style(dpp::cos_primary)
			.set_id(custom_id("match_checkin", match))
			.set_disabled(!enabled)
		);
		row.add_component(
			dpp::component()
			.set_label("Report Score")
			.set_style(dpp::cos_success)
			.set_id(custom_id("match_report", match))
			.set_disabled(!enabled)
		);
		row.add_component(
			dpp::component()
			.set_label("Forfeit")
			.set_style(dpp::cos_danger)
			.set_id(custom_id("match_forfeit", match))
			.set_disabled(!enabled)
		);
		row.add_component(
			dpp::component()
			.set_label("Call Staff")
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
	dpp::snowflake guild_id,
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
		[&bot, guild_id, match, include_buttons](const dpp::confirmation_callback_t& cb) {
			if (cb.is_error()) {
				return;
			}

			const dpp::thread thread = cb.get<dpp::thread>();
			tournament_bracket::set_discord_thread(match.tournament_id, match.id, thread.id);
			tournament_bracket::mark_match_opened(
				match.tournament_id,
				match.id,
				static_cast<int>(time(nullptr)),
				tournament_matchmaking::DEFAULT_MATCH_GRACE_TIME
			);
			bot.message_create(build_match_message(guild_id, match, include_buttons).set_channel_id(thread.id));

			if (!match.player_a_id.empty()) {
				bot.thread_member_add(thread.id, dpp::snowflake(match.player_a_id));
			}
			if (!match.player_b_id.empty()) {
				bot.thread_member_add(thread.id, dpp::snowflake(match.player_b_id));
			}
		}
	);
}
