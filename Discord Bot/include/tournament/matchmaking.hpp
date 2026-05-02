#pragma once
#include <optional>
#include <string>
#include <vector>
#include "tournament/bracket/Bracket.hpp"

namespace tournament_matchmaking {
	inline constexpr int DEFAULT_MATCH_GRACE_TIME = 600;

	enum class MatchAssignmentStatus {
		Pending,
		Active,
		Completed,
		NoShow,
		Disputed,
		Cancelled
	};

	struct MatchAssignment {
		int tournament_id = 0;
		int match_index = -1;
		int round = 0;
		int position = 0;

		std::string player_a_id;
		std::string player_b_id;

		std::string channel_id;
		std::string thread_id;

		int assigned_at = 0;
		int grace_time = DEFAULT_MATCH_GRACE_TIME;

		MatchAssignmentStatus status = MatchAssignmentStatus::Pending;
	};

	std::vector<MatchAssignment> create_round_assignments(
		int tournament_id,
		const Bracket& bracket,
		int round,
		int assigned_at,
		int grace_time = DEFAULT_MATCH_GRACE_TIME
	);

	std::vector<MatchAssignment> create_ready_assignments(
		int tournament_id,
		const Bracket& bracket,
		int assigned_at,
		int grace_time = DEFAULT_MATCH_GRACE_TIME
	);

	bool is_match_ready(const Match& match);
	bool is_within_grace_time(const MatchAssignment& assignment, int now);
	bool is_past_grace_time(const MatchAssignment& assignment, int now);

	std::optional<std::string> no_show_candidate(
		const MatchAssignment& assignment,
		bool player_a_present,
		bool player_b_present,
		int now
	);
}
