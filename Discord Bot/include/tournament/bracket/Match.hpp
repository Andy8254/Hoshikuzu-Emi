#pragma once
#include <string>

enum class MatchState {
	Pending,
	Ongoing,
	Completed
};

struct Match {
	std::string playerA_id;
	std::string playerB_id;

	std::string winner_id;

	int scoreA = 0;
	int scoreB = 0;

	int next_winner_match = -1;
	int next_loser_match = -1;

	MatchState state = MatchState::Pending;
};