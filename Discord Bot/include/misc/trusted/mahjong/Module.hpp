#ifndef MISC_TRUSTED_MAHJONG_MODULE_HPP
#define MISC_TRUSTED_MAHJONG_MODULE_HPP

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace misc_trusted_mahjong {

	inline constexpr int TABLE_SIZE = 4;
	inline constexpr int PLAYOFF_ADVANCERS = 2;

	struct PlayerSeat {
		std::string discord_id;
		std::string display_name;
		int seat = 0;
	};

	struct TableScore {
		std::string discord_id;
		int raw_score = 25000;
		int placement = 0;
		double league_points = 0.0;
	};

	struct TableResult {
		int table_id = 0;
		std::array<TableScore, TABLE_SIZE> scores{};
		std::array<std::string, PLAYOFF_ADVANCERS> advancing_discord_ids{};
	};

	struct LeagueStanding {
		std::string discord_id;
		std::string display_name;
		int games_played = 0;
		double total_points = 0.0;
		double average_rank = 0.0;
		int first_places = 0;
	};

	struct PlayoffTableDraft {
		int table_id = 0;
		std::array<PlayerSeat, TABLE_SIZE> players{};
		int advancing_count = PLAYOFF_ADVANCERS;
	};

	bool enabled();
	bool init();
	std::string module_status();
	std::string module_scope();
	bool valid_table_size(int player_count);
	std::optional<PlayoffTableDraft> make_playoff_table_draft(
		int table_id,
		const std::vector<PlayerSeat>& seeded_players
	);

}

#endif
