#include "tournament/bracket/Bracket.hpp"
#include <stdexcept>

namespace {
	int log2_power_of_two(int n) {
		int result = 0;
		while (n > 1) {
			n >>= 1;
			++result;
		}
		return result;
	}

	std::vector<int> make_seed_order(int size) {
		std::vector<int> order = { 1 };
		for (int field_size = 2; field_size <= size; field_size <<= 1) {
			std::vector<int> next;
			next.reserve(field_size);

			for (int seed : order) {
				next.push_back(seed);
				next.push_back(field_size + 1 - seed);
			}

			order = std::move(next);
		}

		return order;
	}

	std::vector<int> make_round_starts(int rounds) {
		std::vector<int> starts(rounds, 0);
		int offset = 0;

		for (int round = 0; round < rounds; ++round) {
			starts[round] = offset;
			offset += 1 << (rounds - round - 1);
		}

		return starts;
	}

	void place_winner(std::vector<Match>& matches, int source_index, const std::string& winner_id) {
		if (winner_id.empty()) {
			return;
		}

		Match& source = matches.at(source_index);
		if (source.next_winner_match < 0) {
			return;
		}

		Match& next = matches.at(source.next_winner_match);
		if (source.position % 2 == 0) {
			next.playerA_id = winner_id;
		}
		else {
			next.playerB_id = winner_id;
		}
	}
}

int Bracket::next_power_of_two(int n) {
	if (n <= 1) {
		return n;
	}

	int power = 1;
	while (power < n) {
		power <<= 1;
	}
	return power;
}

void Bracket::generate_single_elimination(const std::vector<std::string>& seeded_players) {
	matches.clear();
	bracket_size = next_power_of_two(static_cast<int>(seeded_players.size()));
	rounds = log2_power_of_two(bracket_size);

	if (bracket_size <= 1) {
		return;
	}

	matches.resize(bracket_size - 1);

	const std::vector<int> round_starts = make_round_starts(rounds);
	for (int round = 0; round < rounds; ++round) {
		const int match_count = 1 << (rounds - round - 1);

		for (int position = 0; position < match_count; ++position) {
			const int index = round_starts[round] + position;
			Match& match = matches[index];

			match.round = round;
			match.position = position;

			if (round + 1 < rounds) {
				match.next_winner_match = round_starts[round + 1] + (position / 2);
			}
			else {
				match.next_winner_match = DEST_CHAMPION;
			}

			match.next_loser_match = DEST_ELIMINATED;
		}
	}

	std::vector<std::string> slots(bracket_size);
	const std::vector<int> seed_order = make_seed_order(bracket_size);

	for (int slot = 0; slot < bracket_size; ++slot) {
		const int seed_index = seed_order[slot] - 1;
		if (seed_index < static_cast<int>(seeded_players.size())) {
			slots[slot] = seeded_players[seed_index];
		}
	}

	const int first_round_matches = bracket_size / 2;
	for (int position = 0; position < first_round_matches; ++position) {
		Match& match = matches[position];
		match.playerA_id = slots[position * 2];
		match.playerB_id = slots[position * 2 + 1];

		const bool has_a = !match.playerA_id.empty();
		const bool has_b = !match.playerB_id.empty();

		if (has_a != has_b) {
			match.winner_id = has_a ? match.playerA_id : match.playerB_id;
			match.state = MatchState::Completed;
			place_winner(matches, position, match.winner_id);
		}
	}
}

void Bracket::generate_double_elimination(const std::vector<std::string>& seeded_players) {
	(void)seeded_players;
	throw std::logic_error("Double-elimination bracket generation is not implemented yet.");
}

void Bracket::report_match(int match_index, int scoreA, int scoreB) {
	Match& match = matches.at(match_index);

	if (match.playerA_id.empty() || match.playerB_id.empty()) {
		throw std::logic_error("Cannot report a match without two players.");
	}

	if (scoreA == scoreB) {
		throw std::logic_error("Single-elimination matches cannot end in a draw.");
	}

	match.scoreA = scoreA;
	match.scoreB = scoreB;
	match.winner_id = scoreA > scoreB ? match.playerA_id : match.playerB_id;
	match.state = MatchState::Completed;

	place_winner(matches, match_index, match.winner_id);
}

Match& Bracket::get_match(int index) {
	return matches.at(index);
}

const Match& Bracket::get_match(int index) const {
	return matches.at(index);
}

