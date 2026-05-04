#pragma once
#include <string>
#include <vector>
#include "Match.hpp"

class Bracket {
public:
	std::vector<Match> matches;

	void generate_single_elimination(const std::vector<std::string>& seeded_players);
	void generate_double_elimination(const std::vector<std::string>& seeded_players);
	void generate_round_robin(const std::vector<std::string>& seeded_players);
	void report_match(int match_index, int scoreA, int scoreB);

	Match& get_match(int index);
	const Match& get_match(int index) const;

private:
	int bracket_size = 0;
	int rounds = 0;

	static int next_power_of_two(int n);
};
