#include "misc/trusted/mahjong/Module.hpp"

#include "core/Log.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace {
	std::string getenv_string(const char* name) {
		char* value = nullptr;
		size_t size = 0;
		const errno_t rc = _dupenv_s(&value, &size, name);
		std::string result;
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

	void log_mahjong_action(const std::string& action, const std::string& detail = "") {
		bot_log::info("trusted-mahjong", action, detail);
	}
}

namespace misc_trusted_mahjong {

	bool enabled() {
		return env_truthy("BOT_ENABLE_TRUSTED_MAHJONG");
	}

	bool init() {
		const bool is_enabled = enabled();
		log_mahjong_action(is_enabled ? "init_enabled" : "init_skipped", is_enabled ? "commands=not_exposed" : "reason=disabled");
		return is_enabled;
	}

	std::string module_status() {
		return enabled()
			? "trusted_mahjong: scaffold enabled, commands not exposed"
			: "trusted_mahjong: disabled";
	}

	std::string module_scope() {
		return "Mahjong scaffold: league standings, four-player playoff tables, and two-player advancement contracts only.";
	}

	bool valid_table_size(int player_count) {
		return player_count == TABLE_SIZE;
	}

	std::optional<PlayoffTableDraft> make_playoff_table_draft(
		int table_id,
		const std::vector<PlayerSeat>& seeded_players
	) {
		if (seeded_players.size() != TABLE_SIZE) {
			log_mahjong_action(
				"playoff_table_draft_rejected",
				"table_id=" + std::to_string(table_id) + " player_count=" + std::to_string(seeded_players.size())
			);
			return std::nullopt;
		}

		PlayoffTableDraft draft;
		draft.table_id = table_id;
		draft.advancing_count = PLAYOFF_ADVANCERS;

		for (int i = 0; i < TABLE_SIZE; ++i) {
			draft.players[static_cast<std::size_t>(i)] = seeded_players[static_cast<std::size_t>(i)];
			draft.players[static_cast<std::size_t>(i)].seat = i + 1;
		}

		log_mahjong_action("playoff_table_draft_ok", "table_id=" + std::to_string(table_id));
		return draft;
	}

}
