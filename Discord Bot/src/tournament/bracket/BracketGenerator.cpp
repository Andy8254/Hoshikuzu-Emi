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

//Placeholder
void Bracket::generate_double_elimination(const std::vector<std::string>& seeded_players) {
	matches.clear();

	bracket_size = next_power_of_two(static_cast<int>(seeded_players.size()));
	rounds = log2_power_of_two(bracket_size);

	if (bracket_size <= 1) {
		return;
	}

	const int wb_rounds = rounds;
	const int wb_matches = bracket_size - 1;

	const int lb_rounds = std::max(0, 2 * rounds - 2);

	std::vector<int> wb_starts = make_round_starts(wb_rounds);

	std::vector<int> lb_counts;
	std::vector<int> lb_starts;

	int offset = wb_matches;

	for (int lb_round = 0; lb_round < lb_rounds; ++lb_round) {
		int count = 1 << (rounds - 2 - (lb_round / 2));
		lb_counts.push_back(count);
		lb_starts.push_back(offset);
		offset += count;
	}

	const int grand_final = offset++;
	const int grand_final_reset = offset++;

	matches.resize(offset);

	// Winners bracket
	for (int round = 0; round < wb_rounds; ++round) {
		const int match_count = 1 << (rounds - round - 1);

		for (int pos = 0; pos < match_count; ++pos) {
			const int index = wb_starts[round] + pos;
			Match& match = matches[index];

			match.round = round;
			match.position = pos;

			if (round + 1 < wb_rounds) {
				match.next_winner_match = wb_starts[round + 1] + (pos / 2);
				match.next_winner_slot = pos % 2;
			}
			else {
				match.next_winner_match = grand_final;
				match.next_winner_slot = 0;
			}

			if (round == 0) {
				match.next_loser_match = lb_starts[0] + (pos / 2);
				match.next_loser_slot = pos % 2;
			}
			else {
				const int lb_round = 2 * round - 1;
				match.next_loser_match = lb_starts[lb_round] + pos;
				match.next_loser_slot = 1;
			}
		}
	}

	// Losers bracket
	for (int lb_round = 0; lb_round < lb_rounds; ++lb_round) {
		const int count = lb_counts[lb_round];

		for (int pos = 0; pos < count; ++pos) {
			const int index = lb_starts[lb_round] + pos;
			Match& match = matches[index];

			match.round = wb_rounds + lb_round;
			match.position = pos;
			match.next_loser_match = DEST_ELIMINATED;

			if (lb_round + 1 < lb_rounds) {
				const int next_count = lb_counts[lb_round + 1];

				if (next_count == count) {
					match.next_winner_match = lb_starts[lb_round + 1] + pos;
					match.next_winner_slot = 0;
				}
				else {
					match.next_winner_match = lb_starts[lb_round + 1] + (pos / 2);
					match.next_winner_slot = pos % 2;
				}
			}
			else {
				match.next_winner_match = grand_final;
				match.next_winner_slot = 1;
			}
		}
	}

	// Grand final
	matches[grand_final].round = wb_rounds + lb_rounds;
	matches[grand_final].position = 0;
	matches[grand_final].next_winner_match = DEST_CHAMPION;
	matches[grand_final].next_loser_match = DEST_ELIMINATED;

	// Optional reset final placeholder
	matches[grand_final_reset].round = wb_rounds + lb_rounds + 1;
	matches[grand_final_reset].position = 0;
	matches[grand_final_reset].next_winner_match = DEST_CHAMPION;
	matches[grand_final_reset].next_loser_match = DEST_ELIMINATED;

	// Seed winners bracket
	std::vector<std::string> slots(bracket_size);
	const std::vector<int> seed_order = make_seed_order(bracket_size);

	for (int slot = 0; slot < bracket_size; ++slot) {
		const int seed_index = seed_order[slot] - 1;
		if (seed_index < static_cast<int>(seeded_players.size())) {
			slots[slot] = seeded_players[seed_index];
		}
	}

	const int first_round_matches = bracket_size / 2;

	for (int pos = 0; pos < first_round_matches; ++pos) {
		Match& match = matches[pos];

		match.playerA_id = slots[pos * 2];
		match.playerB_id = slots[pos * 2 + 1];

		const bool has_a = !match.playerA_id.empty();
		const bool has_b = !match.playerB_id.empty();

		if (has_a != has_b) {
			match.winner_id = has_a ? match.playerA_id : match.playerB_id;
			match.state = MatchState::Completed;
			place_winner(matches, pos, match.winner_id);
		}
	}
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

