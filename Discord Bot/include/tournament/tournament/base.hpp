#pragma once
#include <string>
#include <cstdint>

using PlayerID = uint64_t;
using MatchID = uint64_t;
using GuildID = uint64_t;

struct PlayerRef {
	PlayerID id;
	std::string display_name;
};

enum class MatchResult {
	NONE,
	PLAYER1_WIN,
	PLAYER2_WIN,
	DRAW
};

struct Score {
	int p1 = 0;
	int p2 = 0;
};

enum class Stage {
	ROUND,
	QUARTERFINAL,
	SEMIFINAL,
	FINAL,
	GRAND_FINAL
};

enum class BracketType {
	WINNERS,
	LOSERS,
	GRAND_FINAL
};

struct RoundInfo {
	BracketType bracket;
	int round_index;
};