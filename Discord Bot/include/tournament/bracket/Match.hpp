#pragma once
#include <string>

enum class MatchState {
    Pending,
    Ongoing,
    Completed
};

inline constexpr int DEST_NONE = -1;
inline constexpr int DEST_CHAMPION = -2;
inline constexpr int DEST_ELIMINATED = -3;

struct Match {
    std::string bracket = "winners";

    int round = 0;
    int position = 0;

    std::string playerA_id;
    std::string playerB_id;

    std::string winner_id;

    int scoreA = 0;
    int scoreB = 0;

    int next_winner_match = DEST_NONE;
    int next_winner_slot = -1;

    int next_loser_match = DEST_NONE;
    int next_loser_slot = -1;

    MatchState state = MatchState::Pending;
};
