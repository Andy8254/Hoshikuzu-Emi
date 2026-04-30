#include "tournament/Tournaments.hpp"

Tournament Tournament::create(const std::string& name) {
	Tournament t;
	t.name = name;
	return t;
}

void Tournament::report_match(int match_index, int scoreA, int scoreB) {
	Match& m = bracket.get_match(match_index);

	m.scoreA = scoreA;
	m.scoreB = scoreB;
	m.state = MatchState::Completed;

	//Determine winner
	if (scoreA > scoreB) {
		m.winner_id = m.playerA_id;
	}
	else {
		m.winner_id = m.playerB_id;
	}

	//Advance winner (& loser if double elimination)
	if (m.next_winner_match != -1) {
		Match& next = bracket.get_match(m.next_winner_match);

		if (next.playerA_id.empty()) {
			next.playerA_id = m.winner_id;
		}
		else {
			next.playerB_id = m.winner_id;
		}
	}
}