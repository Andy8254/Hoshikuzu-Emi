#include "tournament/seeding.hpp"
#include "tetrio/TetrioService.hpp"
#include <algorithm>
#include <random>
#include <sstream>

namespace {
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
	std::vector<SeededPlayer> players = seed_general(std::move(participants));

	for (SeededPlayer& player : players) {
		if (player.tetrio_id.empty()) {
			continue;
		}

		auto profile = TetrioService::fetch_user(player.tetrio_id);
		if (!profile) {
			continue;
		}

		player.tetrio_id = profile->username;
		player.tetrio_rating = profile->rating;
		player.tetrio_world_rank = profile->world_rank;
		player.has_tetrio_data = profile->has_league_data;
	}

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
	return players;
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
	csv << "seed,discord_id,display_name,tetrio_id,tetrio_rating,tetrio_world_rank,has_tetrio_data\n";

	for (const SeededPlayer& player : seeded_players) {
		csv
			<< player.seed << ','
			<< csv_escape(player.discord_id) << ','
			<< csv_escape(player.display_name) << ','
			<< csv_escape(player.tetrio_id) << ','
			<< player.tetrio_rating << ','
			<< player.tetrio_world_rank << ','
			<< (player.has_tetrio_data ? "true" : "false") << '\n';
	}

	return csv.str();
}
