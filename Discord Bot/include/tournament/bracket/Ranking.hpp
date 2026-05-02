#pragma once
#include <string>
#include <vector>
#include "Match.hpp"

struct StandingRecord {
	std::string player_id;

	int seed = 0;
	int placement = 0;

	int wins = 0;
	int losses = 0;

	int games_won = 0;
	int games_lost = 0;
	int score_diff = 0;

	int eliminated_round = -1;
	int eliminated_match = -1;

	bool is_champion = false;
	bool is_eliminated = false;
	bool is_disqualified = false;
};

std::vector<StandingRecord> calculate_single_elimination_standings(
	const std::vector<Match>& matches,
	const std::vector<std::string>& seeded_players
);

std::string export_standings_csv(const std::vector<StandingRecord>& standings);
