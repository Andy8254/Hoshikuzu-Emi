#ifndef TOURNAMENT_TETRIO_TRIANGLE_ROOM_AUTOMATION_HPP
#define TOURNAMENT_TETRIO_TRIANGLE_ROOM_AUTOMATION_HPP

#include "tournament/bracket/MatchStore.hpp"

#include <string>

namespace tournament_tetrio_triangle {

	struct RoomCreationResult {
		bool ok = false;
		bool attempted = false;
		std::string room_id;
		std::string room_url;
		std::string error;
	};

	bool room_automation_enabled();
	std::string bridge_url();
	RoomCreationResult create_room_for_match(const tournament_bracket::StoredMatch& match);
	std::string room_message_text(const RoomCreationResult& result);

}

#endif
