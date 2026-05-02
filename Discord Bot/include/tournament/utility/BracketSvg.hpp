#pragma once
#include "tournament/bracket/MatchStore.hpp"
#include <string>
#include <vector>

namespace tournament_utility {
	std::string render_match_svg(const tournament_bracket::StoredMatch& match);
	std::string render_bracket_svg(const std::vector<tournament_bracket::StoredMatch>& matches);
	bool write_svg_file(const std::string& path, const std::string& svg);
}
