#include "tournament/matchmaking.hpp"

namespace {
	tournament_matchmaking::MatchAssignment make_assignment(
		int tournament_id,
		int match_index,
		const Match& match,
		int assigned_at,
		int grace_time
	) {
		tournament_matchmaking::MatchAssignment assignment;
		assignment.tournament_id = tournament_id;
		assignment.match_index = match_index;
		assignment.round = match.round;
		assignment.position = match.position;
		assignment.player_a_id = match.playerA_id;
		assignment.player_b_id = match.playerB_id;
		assignment.assigned_at = assigned_at;
		assignment.grace_time = grace_time;
		assignment.status = tournament_matchmaking::MatchAssignmentStatus::Active;
		return assignment;
	}
}

std::vector<tournament_matchmaking::MatchAssignment> tournament_matchmaking::create_round_assignments(
	int tournament_id,
	const Bracket& bracket,
	int round,
	int assigned_at,
	int grace_time
) {
	std::vector<MatchAssignment> assignments;

	for (int i = 0; i < static_cast<int>(bracket.matches.size()); ++i) {
		const Match& match = bracket.matches[i];
		if (match.round == round && is_match_ready(match)) {
			assignments.push_back(make_assignment(tournament_id, i, match, assigned_at, grace_time));
		}
	}

	return assignments;
}

std::vector<tournament_matchmaking::MatchAssignment> tournament_matchmaking::create_ready_assignments(
	int tournament_id,
	const Bracket& bracket,
	int assigned_at,
	int grace_time
) {
	std::vector<MatchAssignment> assignments;

	for (int i = 0; i < static_cast<int>(bracket.matches.size()); ++i) {
		const Match& match = bracket.matches[i];
		if (is_match_ready(match)) {
			assignments.push_back(make_assignment(tournament_id, i, match, assigned_at, grace_time));
		}
	}

	return assignments;
}

bool tournament_matchmaking::is_match_ready(const Match& match) {
	return match.state == MatchState::Pending
		&& !match.playerA_id.empty()
		&& !match.playerB_id.empty();
}

bool tournament_matchmaking::is_within_grace_time(const MatchAssignment& assignment, int now) {
	return now <= assignment.assigned_at + assignment.grace_time;
}

bool tournament_matchmaking::is_past_grace_time(const MatchAssignment& assignment, int now) {
	return now > assignment.assigned_at + assignment.grace_time;
}

std::optional<std::string> tournament_matchmaking::no_show_candidate(
	const MatchAssignment& assignment,
	bool player_a_present,
	bool player_b_present,
	int now
) {
	if (!is_past_grace_time(assignment, now)) {
		return std::nullopt;
	}

	if (player_a_present == player_b_present) {
		return std::nullopt;
	}

	return player_a_present ? assignment.player_b_id : assignment.player_a_id;
}
