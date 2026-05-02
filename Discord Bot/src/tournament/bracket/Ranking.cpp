#include "tournament/bracket/Ranking.hpp"
#include <algorithm>
#include <map>
#include <sstream>
#include <unordered_map>

namespace {
	int highest_round(const std::vector<Match>& matches) {
		int result = -1;
		for (const Match& match : matches) {
			result = std::max(result, match.round);
		}
		return result;
	}

	std::string get_loser_id(const Match& match) {
		if (match.winner_id == match.playerA_id) {
			return match.playerB_id;
		}

		if (match.winner_id == match.playerB_id) {
			return match.playerA_id;
		}

		return "";
	}

	void add_match_stats(StandingRecord& record, int games_won, int games_lost) {
		record.games_won += games_won;
		record.games_lost += games_lost;
		record.score_diff += games_won - games_lost;
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

	std::string status_for(const StandingRecord& record) {
		if (record.is_disqualified) {
			return "disqualified";
		}

		if (record.is_champion) {
			return "champion";
		}

		if (record.is_eliminated) {
			return "eliminated";
		}

		return "active";
	}
}

std::vector<StandingRecord> calculate_single_elimination_standings(
	const std::vector<Match>& matches,
	const std::vector<std::string>& seeded_players
) {
	std::vector<StandingRecord> standings;
	std::unordered_map<std::string, std::size_t> index_by_player;

	standings.reserve(seeded_players.size());
	for (std::size_t i = 0; i < seeded_players.size(); ++i) {
		if (seeded_players[i].empty() || index_by_player.contains(seeded_players[i])) {
			continue;
		}

		StandingRecord record;
		record.player_id = seeded_players[i];
		record.seed = static_cast<int>(i + 1);

		index_by_player[record.player_id] = standings.size();
		standings.push_back(record);
	}

	const int rounds = highest_round(matches) + 1;

	for (std::size_t match_index = 0; match_index < matches.size(); ++match_index) {
		const Match& match = matches[match_index];
		if (match.state != MatchState::Completed || match.winner_id.empty()) {
			continue;
		}

		const std::string loser_id = get_loser_id(match);
		if (loser_id.empty()) {
			continue;
		}

		auto winner_it = index_by_player.find(match.winner_id);
		auto loser_it = index_by_player.find(loser_id);
		if (winner_it == index_by_player.end() || loser_it == index_by_player.end()) {
			continue;
		}

		StandingRecord& winner = standings[winner_it->second];
		StandingRecord& loser = standings[loser_it->second];

		const bool player_a_won = match.winner_id == match.playerA_id;
		const int winner_score = player_a_won ? match.scoreA : match.scoreB;
		const int loser_score = player_a_won ? match.scoreB : match.scoreA;

		++winner.wins;
		add_match_stats(winner, winner_score, loser_score);

		++loser.losses;
		add_match_stats(loser, loser_score, winner_score);

		loser.is_eliminated = true;
		loser.eliminated_round = match.round;
		loser.eliminated_match = static_cast<int>(match_index);
		loser.placement = 1 + (1 << (rounds - match.round - 1));

		if (match.next_winner_match == DEST_CHAMPION) {
			winner.is_champion = true;
			winner.placement = 1;
		}
	}

	std::stable_sort(
		standings.begin(),
		standings.end(),
		[](const StandingRecord& a, const StandingRecord& b) {
			if (a.placement != b.placement) {
				if (a.placement == 0) return false;
				if (b.placement == 0) return true;
				return a.placement < b.placement;
			}

			if (a.eliminated_round != b.eliminated_round) {
				return a.eliminated_round > b.eliminated_round;
			}

			if (a.wins != b.wins) {
				return a.wins > b.wins;
			}

			if (a.score_diff != b.score_diff) {
				return a.score_diff > b.score_diff;
			}

			return a.seed < b.seed;
		}
	);

	return standings;
}

std::string export_standings_csv(const std::vector<StandingRecord>& standings) {
	std::ostringstream csv;
	csv << "placement,player_id,seed,wins,losses,games_won,games_lost,score_diff,eliminated_round,eliminated_match,status\n";

	for (const StandingRecord& record : standings) {
		csv
			<< record.placement << ','
			<< csv_escape(record.player_id) << ','
			<< record.seed << ','
			<< record.wins << ','
			<< record.losses << ','
			<< record.games_won << ','
			<< record.games_lost << ','
			<< record.score_diff << ','
			<< record.eliminated_round << ','
			<< record.eliminated_match << ','
			<< status_for(record) << '\n';
	}

	return csv.str();
}
