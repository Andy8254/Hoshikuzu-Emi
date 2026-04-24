#pragma once
#include <string>
#include "Bracket.hpp"

class Tournament {
public:
	std::string name;
	Bracket bracket;

	static Tournament create(const std::string& name);

	void report_match(int match_index, int scoreA, int scoreB);
};