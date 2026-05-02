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
		const std::string stroke = match.streamed ? "#e05252" : "#5f9ea0";
		out << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"190\" height=\"74\" rx=\"8\" fill=\"#151922\" stroke=\"" << stroke << "\" stroke-width=\"2\"/>"
			<< "<text x=\"" << (x + 12) << "\" y=\"" << (y + 20) << "\" fill=\"#dce3ea\" font-size=\"13\" font-family=\"Segoe UI,Arial\">"
			<< "M" << match.id << " R" << (match.round + 1) << "." << (match.position + 1) << "</text>"
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
	int max_position = 0;
	for (const auto& match : matches) {
		max_round = std::max(max_round, match.round);
		max_position = std::max(max_position, match.position);
	}

	const int width = 80 + (max_round + 1) * 250;
	const int height = 120 + (max_position + 1) * 110;
	std::ostringstream out;
	out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height << "\">"
		<< "<rect width=\"100%\" height=\"100%\" fill=\"#0f1218\"/>"
		<< "<text x=\"32\" y=\"42\" fill=\"#ffffff\" font-size=\"24\" font-family=\"Segoe UI,Arial\">Tournament Bracket</text>";

	for (const auto& match : matches) {
		const int x = 32 + match.round * 250;
		const int y = 72 + match.position * 110 + (match.round * 28);
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
