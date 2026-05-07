#pragma once
#include <optional>
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
		std::string tetrio_current_rank = "Z";
		std::string tetrio_top_rank = "Z";
		bool has_tetrio_data = false;
		std::string tetrio_status = "not_checked";
		std::string rating_bucket;
		double rating_points = 0.0;
		bool has_rating_points = false;
	};

	struct TetrioSeedFilters {
		std::optional<std::string> current_rank_min;
		std::optional<std::string> current_rank_max;
		std::optional<std::string> top_rank_min;
		std::optional<std::string> top_rank_max;
		std::optional<double> tr_min;
		std::optional<double> tr_max;
		bool allow_unranked = false;
	};

	struct ExcludedPlayer {
		std::string discord_id;
		std::string display_name;
		std::string tetrio_id;
		std::string reason;
	};

	struct TetrioSeedResult {
		std::vector<SeededPlayer> seeded;
		std::vector<ExcludedPlayer> excluded;
	};

	struct RatingSeedResult {
		std::vector<SeededPlayer> seeded;
		std::vector<ExcludedPlayer> excluded;
	};

	std::vector<SeededPlayer> seed_general(
		std::vector<tournament_registration::ParticipantRecord> participants,
		SeedingMode mode = SeedingMode::RegistrationOrder
	);

	std::vector<SeededPlayer> seed_tetrio(
		std::vector<tournament_registration::ParticipantRecord> participants
	);

	TetrioSeedResult seed_tetrio_with_filters(
		std::vector<tournament_registration::ParticipantRecord> participants,
		const TetrioSeedFilters& filters
	);

	RatingSeedResult seed_by_rating(
		std::vector<tournament_registration::ParticipantRecord> participants,
		const std::string& rating_bucket,
		bool exclude_missing = true
	);

	std::vector<std::string> to_bracket_player_ids(const std::vector<SeededPlayer>& seeded_players);
	std::string export_seed_csv(const std::vector<SeededPlayer>& seeded_players);
	std::string export_excluded_csv(const std::vector<ExcludedPlayer>& excluded_players);
}
