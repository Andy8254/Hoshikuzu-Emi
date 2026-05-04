#pragma once
#include <dpp/snowflake.h>
#include <optional>
#include <string>
#include <vector>

namespace tournament_bracket {
	enum class StoredMatchState {
		Pending,
		Ready,
		Ongoing,
		Completed
	};

	struct StoredMatch {
		int id = 0;
		int tournament_id = 0;
		int bracket_match_index = 0;
		int round = 0;
		int position = 0;
		std::string bracket = "winners";
		std::string player_a_id;
		std::string player_b_id;
		std::string winner_id;
		int score_a = 0;
		int score_b = 0;
		StoredMatchState state = StoredMatchState::Pending;
		bool streamed = false;
		dpp::snowflake thread_id = 0;
		dpp::snowflake message_id = 0;
		bool player_a_checked_in = false;
		bool player_b_checked_in = false;
		int match_opened_at = 0;
		int grace_time = 600;
		bool no_show_resolved = false;
		std::string no_show_reason;
		std::string pending_auto_dq_player_id;
		int next_winner_match = -1;
		int next_winner_slot = -1;
		int next_loser_match = -1;
		int next_loser_slot = -1;
	};

	struct FormatStanding {
		std::string player_id;
		int wins = 0;
		int losses = 0;
		int byes = 0;
		int points = 0;
		int seed = 0;
	};

	bool init();
	bool generate_single_elimination(int tournament_id);
	bool generate_double_elimination(int tournament_id);
	bool generate_round_robin(int tournament_id);
	bool generate_swiss_round(int tournament_id);
	bool clear_matches(int tournament_id);

	std::optional<StoredMatch> get_match(int tournament_id, int match_id);
	std::vector<StoredMatch> list_matches(int tournament_id);
	std::vector<StoredMatch> list_current_matches(int tournament_id);
	std::vector<StoredMatch> list_round_matches(int tournament_id, int round);
	std::vector<StoredMatch> list_streamed_matches(int tournament_id);
	std::vector<FormatStanding> list_format_standings(int tournament_id);

	bool assign_streamed(int tournament_id, int match_id, bool streamed);
	bool set_discord_thread(int tournament_id, int match_id, dpp::snowflake thread_id, dpp::snowflake message_id = 0);
	bool mark_match_opened(int tournament_id, int match_id, int opened_at, int grace_time = 600);
	bool mark_checked_in(int tournament_id, int match_id, const std::string& discord_id);
	bool report_match(int tournament_id, int match_id, int score_a, int score_b);
	bool correct_match_report(int tournament_id, int match_id, int score_a, int score_b);
	bool forfeit_player(int tournament_id, int match_id, const std::string& discord_id, const std::string& reason);
	int resolve_due_no_shows(int tournament_id, int now);

	std::string state_to_string(StoredMatchState state);
	StoredMatchState state_from_string(const std::string& state);
	std::string player_mention(const std::string& discord_id);
	std::string describe_match(const StoredMatch& match);
}
