#include "tournament/ruleset.hpp"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <sqlite3.h>

namespace {
	class RulesetDatabase {
	public:
		RulesetDatabase() {
			std::filesystem::path path("db/master.db");
			if (path.has_parent_path() && !std::filesystem::exists(path.parent_path())) {
				std::filesystem::create_directories(path.parent_path());
			}

			if (sqlite3_open(path.string().c_str(), &db) != SQLITE_OK) {
				std::cerr << "CRITICAL: SQLITE Open Failed: " << sqlite3_errmsg(db) << std::endl;
				sqlite3_close(db);
				db = nullptr;
				return;
			}

			sqlite3_busy_timeout(db, 5000);
			execute("PRAGMA foreign_keys = ON;");
		}

		~RulesetDatabase() {
			if (db) {
				sqlite3_close(db);
			}
		}

		RulesetDatabase(const RulesetDatabase&) = delete;
		RulesetDatabase& operator=(const RulesetDatabase&) = delete;

		bool execute(const std::string& sql) {
			if (!db) {
				return false;
			}

			char* error = nullptr;
			const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
			if (rc != SQLITE_OK) {
				std::cerr << "SQL Error: " << (error ? error : "unknown") << std::endl;
				sqlite3_free(error);
				return false;
			}

			return true;
		}

		sqlite3* handle() {
			return db;
		}

	private:
		sqlite3* db = nullptr;
	};

	RulesetDatabase& get_db() {
		static RulesetDatabase instance;
		return instance;
	}

	std::string scope_key(tournament_ruleset::RulesetScope scope) {
		return scope == tournament_ruleset::RulesetScope::SECONDARY ? "secondary" : "primary";
	}

	tournament_ruleset::RulesetScope parse_scope_key(const std::string& value) {
		return value == "secondary"
			? tournament_ruleset::RulesetScope::SECONDARY
			: tournament_ruleset::RulesetScope::PRIMARY;
	}

	std::string trigger_key(tournament_ruleset::SecondaryTrigger trigger) {
		switch (trigger) {
		case tournament_ruleset::SecondaryTrigger::TOP_8:
			return "top8";
		case tournament_ruleset::SecondaryTrigger::GRAND_FINALS:
			return "grand_finals";
		case tournament_ruleset::SecondaryTrigger::NONE:
			return "none";
		}

		return "none";
	}

	tournament_ruleset::SecondaryTrigger parse_trigger_key(const std::string& value) {
		if (value == "top8") {
			return tournament_ruleset::SecondaryTrigger::TOP_8;
		}

		if (value == "grand_finals") {
			return tournament_ruleset::SecondaryTrigger::GRAND_FINALS;
		}

		return tournament_ruleset::SecondaryTrigger::NONE;
	}

	std::string deuce_key(DeuceMode mode) {
		switch (mode) {
		case DeuceMode::WIN_BY_DIFF:
			return "win_by_diff";
		case DeuceMode::GOLDEN_POINT:
			return "golden_point";
		case DeuceMode::NONE:
			return "none";
		}

		return "none";
	}

	DeuceMode parse_deuce_key(const std::string& value) {
		if (value == "win_by_diff") {
			return DeuceMode::WIN_BY_DIFF;
		}

		if (value == "golden_point") {
			return DeuceMode::GOLDEN_POINT;
		}

		return DeuceMode::NONE;
	}

	std::string column_text(sqlite3_stmt* stmt, int column) {
		const unsigned char* value = sqlite3_column_text(stmt, column);
		if (!value) {
			return "";
		}

		return reinterpret_cast<const char*>(value);
	}

	tournament_ruleset::RulesetConfig read_config(sqlite3_stmt* stmt) {
		tournament_ruleset::RulesetConfig config;
		config.tournament_id = sqlite3_column_int(stmt, 0);
		config.scope = parse_scope_key(column_text(stmt, 1));
		config.trigger = parse_trigger_key(column_text(stmt, 2));
		config.rules.win_score = sqlite3_column_int(stmt, 3);
		config.rules.win_diff = sqlite3_column_int(stmt, 4);
		config.rules.score_cap = sqlite3_column_int(stmt, 5);
		config.rules.deuce_mode = parse_deuce_key(column_text(stmt, 6));
		config.rules.allow_draw = sqlite3_column_int(stmt, 7) != 0;
		return config;
	}

	bool bind_text(sqlite3_stmt* stmt, int index, const std::string& value) {
		return sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
	}

	bool validate_rules(const MatchRules& rules) {
		if (rules.win_score <= 0 || rules.win_diff < 0 || rules.score_cap < 0) {
			return false;
		}

		if (rules.score_cap > 0 && rules.score_cap < rules.win_score) {
			return false;
		}

		return true;
	}
}

bool tournament_ruleset::init() {
	const char* sql =
		"CREATE TABLE IF NOT EXISTS tournament_rulesets ("
		"tournament_id INTEGER NOT NULL,"
		"scope TEXT NOT NULL,"
		"secondary_trigger TEXT DEFAULT 'none',"
		"win_score INTEGER NOT NULL,"
		"win_diff INTEGER DEFAULT 0,"
		"score_cap INTEGER DEFAULT 0,"
		"deuce_mode TEXT DEFAULT 'none',"
		"allow_draw INTEGER DEFAULT 0,"
		"PRIMARY KEY (tournament_id, scope),"
		"FOREIGN KEY (tournament_id) REFERENCES tournaments(id) ON DELETE CASCADE"
		");";

	return get_db().execute(sql);
}

bool tournament_ruleset::set_ruleset(const RulesetConfig& config) {
	if (config.tournament_id <= 0 || !validate_rules(config.rules)) {
		return false;
	}

	if (config.scope == RulesetScope::PRIMARY && config.trigger != SecondaryTrigger::NONE) {
		return false;
	}

	if (config.scope == RulesetScope::SECONDARY && config.trigger == SecondaryTrigger::NONE) {
		return false;
	}

	if (!init()) {
		return false;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"INSERT INTO tournament_rulesets "
		"(tournament_id, scope, secondary_trigger, win_score, win_diff, score_cap, deuce_mode, allow_draw) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
		"ON CONFLICT(tournament_id, scope) DO UPDATE SET "
		"secondary_trigger = excluded.secondary_trigger,"
		"win_score = excluded.win_score,"
		"win_diff = excluded.win_diff,"
		"score_cap = excluded.score_cap,"
		"deuce_mode = excluded.deuce_mode,"
		"allow_draw = excluded.allow_draw;";

	if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	const bool bound =
		sqlite3_bind_int(stmt, 1, config.tournament_id) == SQLITE_OK
		&& bind_text(stmt, 2, scope_key(config.scope))
		&& bind_text(stmt, 3, trigger_key(config.trigger))
		&& sqlite3_bind_int(stmt, 4, config.rules.win_score) == SQLITE_OK
		&& sqlite3_bind_int(stmt, 5, config.rules.win_diff) == SQLITE_OK
		&& sqlite3_bind_int(stmt, 6, config.rules.score_cap) == SQLITE_OK
		&& bind_text(stmt, 7, deuce_key(config.rules.deuce_mode))
		&& sqlite3_bind_int(stmt, 8, config.rules.allow_draw ? 1 : 0) == SQLITE_OK;

	const bool success = bound && sqlite3_step(stmt) == SQLITE_DONE;
	sqlite3_finalize(stmt);
	return success;
}

bool tournament_ruleset::clear_secondary_ruleset(int tournament_id) {
	if (tournament_id <= 0 || !init()) {
		return false;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"DELETE FROM tournament_rulesets "
		"WHERE tournament_id = ? AND scope = 'secondary';";

	if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	sqlite3_bind_int(stmt, 1, tournament_id);
	const bool success = sqlite3_step(stmt) == SQLITE_DONE;
	sqlite3_finalize(stmt);
	return success;
}

std::optional<tournament_ruleset::RulesetConfig> tournament_ruleset::get_ruleset(
	int tournament_id,
	RulesetScope scope
) {
	if (tournament_id <= 0 || !init()) {
		return std::nullopt;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"SELECT tournament_id, scope, secondary_trigger, win_score, win_diff, score_cap, deuce_mode, allow_draw "
		"FROM tournament_rulesets "
		"WHERE tournament_id = ? AND scope = ? "
		"LIMIT 1;";

	if (sqlite3_prepare_v2(get_db().handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return std::nullopt;
	}

	sqlite3_bind_int(stmt, 1, tournament_id);
	bind_text(stmt, 2, scope_key(scope));

	std::optional<RulesetConfig> result;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		result = read_config(stmt);
	}

	sqlite3_finalize(stmt);
	return result;
}

tournament_ruleset::RulesetConfig tournament_ruleset::get_effective_primary_ruleset(int tournament_id) {
	if (auto configured = get_ruleset(tournament_id, RulesetScope::PRIMARY)) {
		return *configured;
	}

	RulesetConfig fallback;
	fallback.tournament_id = tournament_id;
	fallback.scope = RulesetScope::PRIMARY;
	fallback.trigger = SecondaryTrigger::NONE;
	fallback.rules = tetrio_default_rules();
	return fallback;
}

std::string tournament_ruleset::to_string(RulesetScope scope) {
	return scope == RulesetScope::SECONDARY ? "secondary" : "primary";
}

std::string tournament_ruleset::to_string(SecondaryTrigger trigger) {
	switch (trigger) {
	case SecondaryTrigger::TOP_8:
		return "Top 8";
	case SecondaryTrigger::GRAND_FINALS:
		return "Grand Finals";
	case SecondaryTrigger::NONE:
		return "None";
	}

	return "None";
}

std::string tournament_ruleset::to_string(DeuceMode mode) {
	switch (mode) {
	case DeuceMode::WIN_BY_DIFF:
		return "win by diff";
	case DeuceMode::GOLDEN_POINT:
		return "golden point";
	case DeuceMode::NONE:
		return "off";
	}

	return "off";
}

std::optional<tournament_ruleset::SecondaryTrigger> tournament_ruleset::parse_secondary_trigger(const std::string& value) {
	if (value == "top8") {
		return SecondaryTrigger::TOP_8;
	}

	if (value == "grand_finals") {
		return SecondaryTrigger::GRAND_FINALS;
	}

	if (value == "none") {
		return SecondaryTrigger::NONE;
	}

	return std::nullopt;
}

std::optional<DeuceMode> tournament_ruleset::parse_deuce_mode(const std::string& value) {
	if (value == "none") {
		return DeuceMode::NONE;
	}

	if (value == "win_by_diff") {
		return DeuceMode::WIN_BY_DIFF;
	}

	if (value == "golden_point") {
		return DeuceMode::GOLDEN_POINT;
	}

	return std::nullopt;
}

std::string tournament_ruleset::describe_ruleset(const RulesetConfig& config) {
	std::ostringstream out;
	out << to_string(config.scope) << ": FT" << config.rules.win_score;

	if (config.scope == RulesetScope::SECONDARY) {
		out << " from " << to_string(config.trigger);
	}

	if (config.rules.deuce_mode != DeuceMode::NONE) {
		out << ", deuce " << to_string(config.rules.deuce_mode);
		if (config.rules.win_diff > 0) {
			out << " +" << config.rules.win_diff;
		}
	}
	else {
		out << ", deuce off";
	}

	if (config.rules.score_cap > 0) {
		out << ", cap " << config.rules.score_cap;
	}

	if (config.rules.allow_draw) {
		out << ", draws allowed";
	}

	return out.str();
}
