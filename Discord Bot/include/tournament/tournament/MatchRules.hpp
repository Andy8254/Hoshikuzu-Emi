#pragma once
#include "tournament/tournament/base.hpp"
#include <cstdlib>

enum class DeuceMode {
	NONE,
	WIN_BY_DIFF,
	GOLDEN_POINT
};

struct MatchRules {
	int win_score = 7; //Open TETR.IO Tournament - FT7 for pools by default.
	int win_diff = 0; // 0 = false(NO requirement)
	int score_cap = 0; //0 = no cap
	DeuceMode deuce_mode = DeuceMode::NONE;
	bool allow_draw = false;
};

inline MatchRules tetrio_default_rules() {
	return MatchRules{ 7, 0, 0, DeuceMode::NONE, false };
}

inline MatchRules tetrio_default_top8_rules() {
	return MatchRules{ 11, 2, 0, DeuceMode::WIN_BY_DIFF, false };
}

inline MatchRules fallback_rules() {
	return MatchRules{ 7, 0, 0, DeuceMode::NONE, false };
}

inline bool is_match_over(const Score& score, const MatchRules& rules) {
    int a = score.p1;
    int b = score.p2;

    if (a < 0 || b < 0) return false;

    int high = std::max(a, b);
    int diff = std::abs(a - b);

    if (rules.allow_draw && a == b && high >= rules.win_score) {
        return true;
    }

    if (rules.score_cap > 0 && high >= rules.score_cap && diff >= 1) {
        return true; // cap reached: next point wins
    }

    if (high < rules.win_score) {
        return false;
    }

    switch (rules.deuce_mode) {
    case DeuceMode::NONE:
        return diff >= 1;

    case DeuceMode::WIN_BY_DIFF:
        return diff >= std::max(1, rules.win_diff);

    case DeuceMode::GOLDEN_POINT:
        return diff >= 1;
    }

    return false;
}

inline MatchResult get_match_result(const Score& score, const MatchRules& rules) {
	if (!is_match_over(score, rules)) {
		return MatchResult::NONE;
	}

	if (score.p1 > score.p2) {
		return MatchResult::PLAYER1_WIN;
	}

	if (score.p2 > score.p1) {
		return MatchResult::PLAYER2_WIN;
	}

	return rules.allow_draw ? MatchResult::DRAW : MatchResult::NONE;
}