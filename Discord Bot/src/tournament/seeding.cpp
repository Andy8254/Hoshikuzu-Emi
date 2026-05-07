#include "tournament/seeding.hpp"
#include "tetrio/TetrioService.hpp"
#include <algorithm>
#include <random>
#include <sstream>

namespace {
	int rank_value(const std::string& rank) {
		const std::pair<const char*, int> ranks[] = {
			{ "Z", 0 },
			{ "D", 1 },
			{ "D+", 2 },
			{ "C-", 3 },
			{ "C", 4 },
			{ "C+", 5 },
			{ "B-", 6 },
			{ "B", 7 },
			{ "B+", 8 },
			{ "A-", 9 },
			{ "A", 10 },
			{ "A+", 11 },
			{ "S-", 12 },
			{ "S", 13 },
			{ "S+", 14 },
			{ "SS", 15 },
			{ "U", 16 },
			{ "X", 17 },
			{ "X+", 18 }
		};

		for (const auto& [name, value] : ranks) {
			if (rank == name) {
				return value;
			}
		}

		return -1;
	}

	bool has_filters(const tournament_seeding::TetrioSeedFilters& filters) {
		return filters.current_rank_min
			|| filters.current_rank_max
			|| filters.top_rank_min
			|| filters.top_rank_max
			|| filters.tr_min
			|| filters.tr_max
			|| filters.allow_unranked;
	}

	bool rank_below_min(const std::string& rank, const std::optional<std::string>& minimum) {
		return minimum && rank_value(rank) < rank_value(*minimum);
	}

	bool rank_above_max(const std::string& rank, const std::optional<std::string>& maximum) {
		return maximum && rank_value(rank) > rank_value(*maximum);
	}

	bool is_seedable(const tournament_registration::ParticipantRecord& participant) {
		return participant.status == tournament_registration::ParticipantStatus::CheckedIn
			|| participant.status == tournament_registration::ParticipantStatus::LateCheckedIn;
	}

	tournament_seeding::SeededPlayer to_seeded_player(
		const tournament_registration::ParticipantRecord& participant
	) {
		tournament_seeding::SeededPlayer player;
		player.discord_id = participant.discord_id;
		player.display_name = participant.display_name;
		player.tetrio_id = participant.tetrio_id.empty() ? participant.provided_username : participant.tetrio_id;
		player.seed = participant.seed;
		return player;
	}

	void assign_seeds(std::vector<tournament_seeding::SeededPlayer>& players) {
		for (int i = 0; i < static_cast<int>(players.size()); ++i) {
			players[i].seed = i + 1;
		}
	}

	std::string csv_escape(const std::string& value) {
		bool needs_quotes = false;
		for (char c : value) {
			if (c == ',' || c == '"' || c == '\n' || c == '\r') {
				needs_quotes = true;
				break;
			}
		}

		if (!needs_quotes) {
			return value;
		}

		std::string escaped = "\"";
		for (char c : value) {
			if (c == '"') {
				escaped += "\"\"";
			}
			else {
				escaped += c;
			}
		}
		escaped += "\"";
		return escaped;
	}
}

std::vector<tournament_seeding::SeededPlayer> tournament_seeding::seed_general(
	std::vector<tournament_registration::ParticipantRecord> participants,
	SeedingMode mode
) {
	std::vector<SeededPlayer> players;
	for (const auto& participant : participants) {
		if (is_seedable(participant)) {
			players.push_back(to_seeded_player(participant));
		}
	}

	if (mode == SeedingMode::Manual) {
		std::stable_sort(
			players.begin(),
			players.end(),
			[](const SeededPlayer& a, const SeededPlayer& b) {
				if (a.seed != b.seed) {
					if (a.seed == 0) return false;
					if (b.seed == 0) return true;
					return a.seed < b.seed;
				}

				return a.discord_id < b.discord_id;
			}
		);
	}
	else if (mode == SeedingMode::Random) {
		std::random_device rd;
		std::mt19937 rng(rd());
		std::shuffle(players.begin(), players.end(), rng);
	}

	assign_seeds(players);
	return players;
}

std::vector<tournament_seeding::SeededPlayer> tournament_seeding::seed_tetrio(
	std::vector<tournament_registration::ParticipantRecord> participants
) {
	return seed_tetrio_with_filters(std::move(participants), {}).seeded;
}

tournament_seeding::TetrioSeedResult tournament_seeding::seed_tetrio_with_filters(
	std::vector<tournament_registration::ParticipantRecord> participants,
	const TetrioSeedFilters& filters
) {
	TetrioSeedResult result;
	std::vector<SeededPlayer> players = seed_general(std::move(participants));
	const bool filter_active = has_filters(filters);

	for (SeededPlayer& player : players) {
		if (player.tetrio_id.empty()) {
			player.tetrio_status = "missing_username";
			if (filter_active) {
				result.excluded.push_back({ player.discord_id, player.display_name, player.tetrio_id, "Missing TETR.IO username" });
			}
			continue;
		}

		auto profile = TetrioService::fetch_user(player.tetrio_id);
		if (!profile) {
			player.tetrio_status = "profile_fetch_failed";
			if (filter_active) {
				result.excluded.push_back({ player.discord_id, player.display_name, player.tetrio_id, "Could not fetch TETR.IO profile" });
			}
			continue;
		}

		player.tetrio_id = profile->username;
		player.tetrio_rating = profile->rating;
		player.tetrio_world_rank = profile->world_rank;
		player.tetrio_current_rank = profile->rank;
		player.tetrio_top_rank = profile->top_rank;
		player.has_tetrio_data = profile->has_league_data;
		player.tetrio_status = profile->league_status;

		if (filter_active && !player.has_tetrio_data && !filters.allow_unranked) {
			result.excluded.push_back({ player.discord_id, player.display_name, player.tetrio_id, "No TETRA LEAGUE data" });
			player.discord_id.clear();
			continue;
		}

		if (filter_active && !player.has_tetrio_data) {
			continue;
		}

		const bool allowed_unranked_rank = filters.allow_unranked && player.tetrio_current_rank == "Z";
		if (!allowed_unranked_rank && rank_below_min(player.tetrio_current_rank, filters.current_rank_min)) {
			result.excluded.push_back({ player.discord_id, player.display_name, player.tetrio_id, "Current rank below minimum" });
			player.discord_id.clear();
			continue;
		}

		if (!allowed_unranked_rank && rank_above_max(player.tetrio_current_rank, filters.current_rank_max)) {
			result.excluded.push_back({ player.discord_id, player.display_name, player.tetrio_id, "Current rank above maximum" });
			player.discord_id.clear();
			continue;
		}

		if (!allowed_unranked_rank && rank_below_min(player.tetrio_top_rank, filters.top_rank_min)) {
			result.excluded.push_back({ player.discord_id, player.display_name, player.tetrio_id, "Top rank below minimum" });
			player.discord_id.clear();
			continue;
		}

		if (!allowed_unranked_rank && rank_above_max(player.tetrio_top_rank, filters.top_rank_max)) {
			result.excluded.push_back({ player.discord_id, player.display_name, player.tetrio_id, "Top rank above maximum" });
			player.discord_id.clear();
			continue;
		}

		if (filters.tr_min && player.tetrio_rating < *filters.tr_min) {
			result.excluded.push_back({ player.discord_id, player.display_name, player.tetrio_id, "TR below minimum" });
			player.discord_id.clear();
			continue;
		}

		if (filters.tr_max && player.tetrio_rating > *filters.tr_max) {
			result.excluded.push_back({ player.discord_id, player.display_name, player.tetrio_id, "TR above maximum" });
			player.discord_id.clear();
			continue;
		}
	}

	players.erase(
		std::remove_if(players.begin(), players.end(), [](const SeededPlayer& player) {
			return player.discord_id.empty();
		}),
		players.end()
	);

	std::stable_sort(
		players.begin(),
		players.end(),
		[](const SeededPlayer& a, const SeededPlayer& b) {
			if (a.has_tetrio_data != b.has_tetrio_data) {
				return a.has_tetrio_data;
			}

			if (a.tetrio_rating != b.tetrio_rating) {
				return a.tetrio_rating > b.tetrio_rating;
			}

			if (a.tetrio_world_rank != b.tetrio_world_rank) {
				if (a.tetrio_world_rank <= 0) return false;
				if (b.tetrio_world_rank <= 0) return true;
				return a.tetrio_world_rank < b.tetrio_world_rank;
			}

			return a.discord_id < b.discord_id;
		}
	);

	assign_seeds(players);
	result.seeded = std::move(players);
	return result;
}

std::vector<std::string> tournament_seeding::to_bracket_player_ids(
	const std::vector<SeededPlayer>& seeded_players
) {
	std::vector<std::string> ids;
	ids.reserve(seeded_players.size());

	for (const SeededPlayer& player : seeded_players) {
		ids.push_back(player.discord_id);
	}

	return ids;
}

std::string tournament_seeding::export_seed_csv(const std::vector<SeededPlayer>& seeded_players) {
	std::ostringstream csv;
	csv << "seed,discord_id,display_name,tetrio_id,tetrio_rating,tetrio_current_rank,tetrio_top_rank,tetrio_world_rank,has_tetrio_data,tetrio_status\n";

	for (const SeededPlayer& player : seeded_players) {
		csv
			<< player.seed << ','
			<< csv_escape(player.discord_id) << ','
			<< csv_escape(player.display_name) << ','
			<< csv_escape(player.tetrio_id) << ','
			<< player.tetrio_rating << ','
			<< csv_escape(player.tetrio_current_rank) << ','
			<< csv_escape(player.tetrio_top_rank) << ','
			<< player.tetrio_world_rank << ','
			<< (player.has_tetrio_data ? "true" : "false") << ','
			<< csv_escape(player.tetrio_status) << '\n';
	}

	return csv.str();
}

std::string tournament_seeding::export_excluded_csv(const std::vector<ExcludedPlayer>& excluded_players) {
	std::ostringstream csv;
	csv << "discord_id,display_name,tetrio_id,reason\n";

	for (const ExcludedPlayer& player : excluded_players) {
		csv
			<< csv_escape(player.discord_id) << ','
			<< csv_escape(player.display_name) << ','
			<< csv_escape(player.tetrio_id) << ','
			<< csv_escape(player.reason) << '\n';
	}

	return csv.str();
}
