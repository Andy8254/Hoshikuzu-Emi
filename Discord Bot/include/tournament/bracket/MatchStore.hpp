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
	};

	bool init();
	bool generate_single_elimination(int tournament_id);
	bool clear_matches(int tournament_id);

	std::optional<StoredMatch> get_match(int tournament_id, int match_id);
	std::vector<StoredMatch> list_matches(int tournament_id);
	std::vector<StoredMatch> list_current_matches(int tournament_id);
	std::vector<StoredMatch> list_round_matches(int tournament_id, int round);
	std::vector<StoredMatch> list_streamed_matches(int tournament_id);

	bool assign_streamed(int tournament_id, int match_id, bool streamed);
	bool set_discord_thread(int tournament_id, int match_id, dpp::snowflake thread_id, dpp::snowflake message_id = 0);
	bool mark_checked_in(int tournament_id, int match_id, const std::string& discord_id);
	bool report_match(int tournament_id, int match_id, int score_a, int score_b);

	std::string state_to_string(StoredMatchState state);
	StoredMatchState state_from_string(const std::string& state);
	std::string player_mention(const std::string& discord_id);
	std::string describe_match(const StoredMatch& match);
}
