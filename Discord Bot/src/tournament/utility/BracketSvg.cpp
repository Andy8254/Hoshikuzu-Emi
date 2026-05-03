#include "tournament/utility/BracketSvg.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
	std::string escape_xml(const std::string& value) {
		std::string out;
		for (char c : value) {
			switch (c) {
			case '&': out += "&amp;"; break;
			case '<': out += "&lt;"; break;
			case '>': out += "&gt;"; break;
			case '"': out += "&quot;"; break;
			case '\'': out += "&apos;"; break;
			default: out += c; break;
			}
		}
		return out;
	}

	std::string short_player(const std::string& id) {
		if (id.empty()) return "TBD";
		if (id.size() <= 8) return id;
		return id.substr(0, 4) + "..." + id.substr(id.size() - 4);
	}

	void render_match_card(std::ostringstream& out, const tournament_bracket::StoredMatch& match, int x, int y) {
		std::string stroke = "#5f9ea0";
		if (match.bracket == "losers") stroke = "#c48b45";
		if (match.bracket == "grand_finals") stroke = "#d8c45d";
		if (match.streamed) stroke = "#e05252";

		out << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"190\" height=\"74\" rx=\"8\" fill=\"#151922\" stroke=\"" << stroke << "\" stroke-width=\"2\"/>"
			<< "<text x=\"" << (x + 12) << "\" y=\"" << (y + 20) << "\" fill=\"#dce3ea\" font-size=\"13\" font-family=\"Segoe UI,Arial\">"
			<< "M" << match.id << " " << escape_xml(match.bracket) << " R" << (match.round + 1) << "." << (match.position + 1) << "</text>"
			<< "<text x=\"" << (x + 12) << "\" y=\"" << (y + 42) << "\" fill=\"#ffffff\" font-size=\"14\" font-family=\"Segoe UI,Arial\">"
			<< escape_xml(short_player(match.player_a_id)) << "</text>"
			<< "<text x=\"" << (x + 130) << "\" y=\"" << (y + 42) << "\" fill=\"#ffffff\" font-size=\"14\" font-family=\"Segoe UI,Arial\">"
			<< match.score_a << "</text>"
			<< "<text x=\"" << (x + 12) << "\" y=\"" << (y + 62) << "\" fill=\"#ffffff\" font-size=\"14\" font-family=\"Segoe UI,Arial\">"
			<< escape_xml(short_player(match.player_b_id)) << "</text>"
			<< "<text x=\"" << (x + 130) << "\" y=\"" << (y + 62) << "\" fill=\"#ffffff\" font-size=\"14\" font-family=\"Segoe UI,Arial\">"
			<< match.score_b << "</text>";
	}
}

std::string tournament_utility::render_match_svg(const tournament_bracket::StoredMatch& match) {
	std::ostringstream out;
	out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"430\" height=\"170\" viewBox=\"0 0 430 170\">"
		<< "<rect width=\"430\" height=\"170\" fill=\"#0f1218\"/>"
		<< "<text x=\"24\" y=\"34\" fill=\"#ffffff\" font-size=\"20\" font-family=\"Segoe UI,Arial\">Match "
		<< match.id << "</text>";
	render_match_card(out, match, 24, 56);
	out << "</svg>";
	return out.str();
}

std::string tournament_utility::render_bracket_svg(const std::vector<tournament_bracket::StoredMatch>& matches) {
	int max_round = 0;
	int max_winners_position = 0;
	int max_losers_position = 0;
	int max_grand_position = 0;
	int min_losers_round = 9999;
	int min_grand_round = 9999;
	bool has_losers = false;
	bool has_grand = false;

	for (const auto& match : matches) {
		max_round = std::max(max_round, match.round);
		if (match.bracket == "losers") {
			has_losers = true;
			min_losers_round = std::min(min_losers_round, match.round);
			max_losers_position = std::max(max_losers_position, match.position);
		}
		else if (match.bracket == "grand_finals") {
			has_grand = true;
			min_grand_round = std::min(min_grand_round, match.round);
			max_grand_position = std::max(max_grand_position, match.position);
		}
		else {
			max_winners_position = std::max(max_winners_position, match.position);
		}
	}

	const int width = 80 + (max_round + 1) * 250;
	const int winners_height = 110 + (max_winners_position + 1) * 110;
	const int losers_y = has_losers ? winners_height + 60 : 0;
	const int losers_height = has_losers ? 110 + (max_losers_position + 1) * 110 : 0;
	const int grand_y = has_grand ? winners_height + losers_height + (has_losers ? 110 : 60) : 0;
	const int grand_height = has_grand ? 110 + (max_grand_position + 1) * 110 : 0;
	const int height = std::max(220, winners_height + losers_height + grand_height + (has_losers ? 110 : 0) + (has_grand ? 80 : 0));

	std::ostringstream out;
	out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height << "\">"
		<< "<rect width=\"100%\" height=\"100%\" fill=\"#0f1218\"/>"
		<< "<text x=\"32\" y=\"42\" fill=\"#ffffff\" font-size=\"24\" font-family=\"Segoe UI,Arial\">Tournament Bracket</text>"
		<< "<text x=\"32\" y=\"78\" fill=\"#8fd8da\" font-size=\"17\" font-family=\"Segoe UI,Arial\">Winners Bracket</text>";

	if (has_losers) {
		out << "<text x=\"32\" y=\"" << losers_y + 6 << "\" fill=\"#e3ad69\" font-size=\"17\" font-family=\"Segoe UI,Arial\">Losers Bracket</text>";
	}

	if (has_grand) {
		out << "<text x=\"32\" y=\"" << grand_y + 6 << "\" fill=\"#e8d96d\" font-size=\"17\" font-family=\"Segoe UI,Arial\">Grand Finals</text>";
	}

	for (const auto& match : matches) {
		int local_round = match.round;
		int band_y = 92;

		if (match.bracket == "losers") {
			local_round = match.round - min_losers_round;
			band_y = losers_y + 24;
		}
		else if (match.bracket == "grand_finals") {
			local_round = match.round - min_grand_round;
			band_y = grand_y + 24;
		}

		const int x = 32 + local_round * 250;
		const int y = band_y + match.position * 110 + (local_round * 18);
		render_match_card(out, match, x, y);
	}

	out << "</svg>";
	return out.str();
}

bool tournament_utility::write_svg_file(const std::string& path, const std::string& svg) {
	std::filesystem::path file_path(path);
	if (file_path.has_parent_path()) {
		std::filesystem::create_directories(file_path.parent_path());
	}

	std::ofstream out(path, std::ios::binary);
	if (!out) return false;
	out << svg;
	return true;
}
