#pragma once
#include "tournament/bracket/MatchStore.hpp"
#include <dpp/dpp.h>

namespace tournament_discord {
	dpp::embed build_match_embed(dpp::snowflake guild_id, const tournament_bracket::StoredMatch& match);
	dpp::message build_match_message(dpp::snowflake guild_id, const tournament_bracket::StoredMatch& match, bool include_buttons);
	std::string match_thread_name(const tournament_bracket::StoredMatch& match);

	void create_match_thread(
		dpp::cluster& bot,
		dpp::snowflake guild_id,
		dpp::snowflake channel_id,
		const tournament_bracket::StoredMatch& match,
		bool include_buttons,
		bool create_tetrio_room = false
	);
}
