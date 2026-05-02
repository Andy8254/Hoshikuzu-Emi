#pragma once
#include <string>
#include <vector>
#include "tournament/registration.hpp"

namespace tournament_seeding {
	enum class SeedingMode {
		Manual,
		RegistrationOrder,
		Random,
		Tetrio
	};

	struct SeededPlayer {
		std::string discord_id;
		std::string display_name;
		std::string tetrio_id;
		int seed = 0;
		double tetrio_rating = 0.0;
		int tetrio_world_rank = 0;
		bool has_tetrio_data = false;
	};

	std::vector<SeededPlayer> seed_general(
		std::vector<tournament_registration::ParticipantRecord> participants,
		SeedingMode mode = SeedingMode::RegistrationOrder
	);

	std::vector<SeededPlayer> seed_tetrio(
		std::vector<tournament_registration::ParticipantRecord> participants
	);

	std::vector<std::string> to_bracket_player_ids(const std::vector<SeededPlayer>& seeded_players);
	std::string export_seed_csv(const std::vector<SeededPlayer>& seeded_players);
}
