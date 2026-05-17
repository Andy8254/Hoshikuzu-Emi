#include "misc/isolated/youtube_randomizer.hpp"

#include "misc/sqlite-user.hpp"

#include <cstdlib>
#include <random>

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

	bool bind_text(sqlite3_stmt* stmt, int index, const std::string& value) {
		return sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
	}

	std::optional<misc_isolated_youtube_randomizer::MusicLink> row_to_link(sqlite3_stmt* stmt) {
		misc_isolated_youtube_randomizer::MusicLink link;
		link.id = sqlite3_column_int(stmt, 0);

		const unsigned char* guild_id = sqlite3_column_text(stmt, 1);
		const unsigned char* title = sqlite3_column_text(stmt, 2);
		const unsigned char* youtube_url = sqlite3_column_text(stmt, 3);
		const unsigned char* added_by = sqlite3_column_text(stmt, 4);

		if (!guild_id || !title || !youtube_url || !added_by) {
			return std::nullopt;
		}

		link.guild_id = reinterpret_cast<const char*>(guild_id);
		link.title = reinterpret_cast<const char*>(title);
		link.youtube_url = reinterpret_cast<const char*>(youtube_url);
		link.added_by = reinterpret_cast<const char*>(added_by);
		link.added_at = sqlite3_column_int(stmt, 5);
		link.enabled = sqlite3_column_int(stmt, 6) != 0;
		return link;
	}
}

namespace misc_isolated_youtube_randomizer {

	bool enabled() {
		return env_truthy("BOT_ENABLE_YOUTUBE_RANDOMIZER");
	}

	bool init() {
		if (!enabled()) {
			return false;
		}

		auto& db = misc_user_sqlite::user_db();
		if (!db.ok()) {
			return false;
		}

		const bool table_ok = db.execute(
			"CREATE TABLE IF NOT EXISTS misc_youtube_randomizer_links ("
			"id INTEGER PRIMARY KEY AUTOINCREMENT,"
			"guild_id TEXT NOT NULL,"
			"title TEXT NOT NULL,"
			"youtube_url TEXT NOT NULL,"
			"added_by TEXT NOT NULL,"
			"added_at INTEGER NOT NULL,"
			"enabled INTEGER NOT NULL DEFAULT 1,"
			"UNIQUE(guild_id, youtube_url)"
			");"
		);

		const bool index_ok = db.create_index_if_missing(
			"idx_misc_youtube_randomizer_links_guild_enabled",
			"misc_youtube_randomizer_links",
			"guild_id, enabled"
		);

		return table_ok && index_ok;
	}

	bool add_link(
		const std::string& guild_id,
		const std::string& title,
		const std::string& youtube_url,
		const std::string& added_by,
		int added_at
	) {
		if (guild_id.empty() || title.empty() || youtube_url.empty() || added_by.empty()) {
			return false;
		}

		auto& db = misc_user_sqlite::user_db();
		if (!db.ok()) {
			return false;
		}

		sqlite3_stmt* stmt = nullptr;
		const char* sql =
			"INSERT OR IGNORE INTO misc_youtube_randomizer_links "
			"(guild_id, title, youtube_url, added_by, added_at, enabled) "
			"VALUES (?, ?, ?, ?, ?, 1);";

		if (sqlite3_prepare_v2(db.get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
			return false;
		}

		const bool bound =
			bind_text(stmt, 1, guild_id) &&
			bind_text(stmt, 2, title) &&
			bind_text(stmt, 3, youtube_url) &&
			bind_text(stmt, 4, added_by) &&
			sqlite3_bind_int(stmt, 5, added_at) == SQLITE_OK;

		const bool ok = bound && sqlite3_step(stmt) == SQLITE_DONE;
		sqlite3_finalize(stmt);
		return ok;
	}

	std::vector<MusicLink> list_links(const std::string& guild_id, bool include_disabled) {
		std::vector<MusicLink> links;
		if (guild_id.empty()) {
			return links;
		}

		auto& db = misc_user_sqlite::user_db();
		if (!db.ok()) {
			return links;
		}

		sqlite3_stmt* stmt = nullptr;
		const char* sql =
			"SELECT id, guild_id, title, youtube_url, added_by, added_at, enabled "
			"FROM misc_youtube_randomizer_links "
			"WHERE guild_id = ? AND (? = 1 OR enabled = 1) "
			"ORDER BY id ASC;";

		if (sqlite3_prepare_v2(db.get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
			return links;
		}

		if (!bind_text(stmt, 1, guild_id) ||
			sqlite3_bind_int(stmt, 2, include_disabled ? 1 : 0) != SQLITE_OK) {
			sqlite3_finalize(stmt);
			return links;
		}

		while (sqlite3_step(stmt) == SQLITE_ROW) {
			auto link = row_to_link(stmt);
			if (link) {
				links.push_back(*link);
			}
		}

		sqlite3_finalize(stmt);
		return links;
	}

	std::optional<MusicLink> random_link(const std::string& guild_id) {
		auto links = list_links(guild_id, false);
		if (links.empty()) {
			return std::nullopt;
		}

		std::random_device device;
		std::mt19937 generator(device());
		std::uniform_int_distribution<std::size_t> distribution(0, links.size() - 1);
		return links[distribution(generator)];
	}

	bool set_link_enabled(const std::string& guild_id, int id, bool link_enabled) {
		if (guild_id.empty() || id <= 0) {
			return false;
		}

		auto& db = misc_user_sqlite::user_db();
		if (!db.ok()) {
			return false;
		}

		sqlite3_stmt* stmt = nullptr;
		const char* sql =
			"UPDATE misc_youtube_randomizer_links "
			"SET enabled = ? "
			"WHERE guild_id = ? AND id = ?;";

		if (sqlite3_prepare_v2(db.get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
			return false;
		}

		const bool bound =
			sqlite3_bind_int(stmt, 1, link_enabled ? 1 : 0) == SQLITE_OK &&
			bind_text(stmt, 2, guild_id) &&
			sqlite3_bind_int(stmt, 3, id) == SQLITE_OK;

		const bool ok = bound && sqlite3_step(stmt) == SQLITE_DONE;
		sqlite3_finalize(stmt);
		return ok;
	}

}
