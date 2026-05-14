#include "tournament/tetrio/triangle/RoomAutomation.hpp"

#include "core/api_fetcher.hpp"

#include <cstdlib>
#include <sstream>

namespace {
	std::string getenv_string(const char* name, const std::string& fallback = "") {
		char* value = nullptr;
		size_t size = 0;
		const errno_t rc = _dupenv_s(&value, &size, name);
		std::string result = fallback;
		if (rc == 0 && value && *value) {
			result = value;
		}

		free(value);
		return result;
	}

	bool env_truthy(const char* name) {
		const std::string value = getenv_string(name);
		return value == "1" || value == "true" || value == "TRUE" || value == "yes" || value == "on";
	}

	std::string json_escape(const std::string& value) {
		std::string escaped;
		escaped.reserve(value.size() + 8);
		for (const char c : value) {
			switch (c) {
			case '\\': escaped += "\\\\"; break;
			case '"': escaped += "\\\""; break;
			case '\n': escaped += "\\n"; break;
			case '\r': escaped += "\\r"; break;
			case '\t': escaped += "\\t"; break;
			default: escaped += c; break;
			}
		}
		return escaped;
	}

	std::string extract_json_string(const std::string& json, const std::string& key) {
		const std::string needle = "\"" + key + "\"";
		std::size_t pos = json.find(needle);
		if (pos == std::string::npos) {
			return "";
		}

		pos = json.find(':', pos + needle.size());
		if (pos == std::string::npos) {
			return "";
		}

		pos = json.find('"', pos + 1);
		if (pos == std::string::npos) {
			return "";
		}

		std::string value;
		bool escaped = false;
		for (++pos; pos < json.size(); ++pos) {
			const char c = json[pos];
			if (escaped) {
				switch (c) {
				case 'n': value += '\n'; break;
				case 'r': value += '\r'; break;
				case 't': value += '\t'; break;
				default: value += c; break;
				}
				escaped = false;
				continue;
			}

			if (c == '\\') {
				escaped = true;
				continue;
			}

			if (c == '"') {
				break;
			}

			value += c;
		}

		return value;
	}

	std::string build_room_request(const tournament_bracket::StoredMatch& match) {
		std::ostringstream body;
		body << "{"
			<< "\"tournament_id\":" << match.tournament_id << ","
			<< "\"match_id\":" << match.id << ","
			<< "\"round\":" << match.round + 1 << ","
			<< "\"position\":" << match.position + 1 << ","
			<< "\"player_a_id\":\"" << json_escape(match.player_a_id) << "\","
			<< "\"player_b_id\":\"" << json_escape(match.player_b_id) << "\""
			<< "}";
		return body.str();
	}
}

namespace tournament_tetrio_triangle {

	bool room_automation_enabled() {
		return env_truthy("BOT_ENABLE_TETRIO_ROOM_AUTOMATION");
	}

	std::string bridge_url() {
		std::string url = getenv_string("TRIANGLE_BRIDGE_URL", "http://127.0.0.1:8787");
		while (!url.empty() && url.back() == '/') {
			url.pop_back();
		}
		return url;
	}

	RoomCreationResult create_room_for_match(const tournament_bracket::StoredMatch& match) {
		RoomCreationResult result;
		if (!room_automation_enabled()) {
			result.error = "TETR.IO room automation is disabled.";
			return result;
		}

		if (match.player_a_id.empty() || match.player_b_id.empty()) {
			result.error = "Match is missing one or more players.";
			return result;
		}

		result.attempted = true;
		const HttpResponse response = HttpClient::post_json(
			bridge_url() + "/rooms",
			build_room_request(match)
		);

		if (!response.error.empty()) {
			result.error = response.error;
			return result;
		}

		if (response.status_code < 200 || response.status_code >= 300) {
			result.error = "Triangle bridge returned HTTP " + std::to_string(response.status_code) + ".";
			return result;
		}

		result.room_id = extract_json_string(response.body, "room_id");
		result.room_url = extract_json_string(response.body, "room_url");
		if (result.room_url.empty()) {
			result.room_url = extract_json_string(response.body, "url");
		}

		if (result.room_id.empty() && result.room_url.empty()) {
			result.error = "Triangle bridge response did not include room_id or room_url.";
			return result;
		}

		result.ok = true;
		return result;
	}

	std::string room_message_text(const RoomCreationResult& result) {
		if (result.ok) {
			if (!result.room_url.empty()) {
				return "\nTETR.IO room: " + result.room_url;
			}
			return "\nTETR.IO room ID: `" + result.room_id + "`";
		}

		if (result.attempted) {
			return "\nTETR.IO room automation failed. Please create the room manually.";
		}

		return "";
	}

}
