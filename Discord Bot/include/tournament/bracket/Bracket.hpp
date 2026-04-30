#pragma once
#include <vector>
#include "Match.hpp"

class Bracket {
public:
	std::vector<Match> matches;

	Match& get_match(int index) {
		return matches.at(index);
	}
};