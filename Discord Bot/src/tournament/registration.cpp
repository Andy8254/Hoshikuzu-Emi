#include "core/sqlite.hpp"
#include "tournament/registration.hpp"
#include <algorithm>
#include <cctype>
#include <sqlite3.h>

namespace {
	Database& get_db() {
		static Database instance("db/master.db");
		return instance;
	}

	bool bind_text(sqlite3_stmt* stmt, int index, const std::string& value) {
		return sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
	}

	std::string column_text(sqlite3_stmt* stmt, int column) {
		const unsigned char* value = sqlite3_column_text(stmt, column);
		if (!value) return "";
		return reinterpret_cast<const char*>(value);
	}

	std::string lower_copy(std::string value) {
		std::transform(
			value.begin(),
			value.end(),
			value.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); }
		);
		return value;
	}

	bool same_username(const std::string& a, const std::string& b) {
		return !a.empty() && !b.empty() && lower_copy(a) == lower_copy(b);
	}

	tournament_registration::ParticipantRecord read_participant(sqlite3_stmt* stmt) {
		tournament_registration::ParticipantRecord record;
		record.tournament_id = sqlite3_column_int(stmt, 0);
		record.discord_id = column_text(stmt, 1);
		record.display_name = column_text(stmt, 2);
		record.provided_username = column_text(stmt, 3);
		record.tetrio_id = column_text(stmt, 4);
		record.status = tournament_registration::status_from_string(column_text(stmt, 5));
		record.seed = sqlite3_column_int(stmt, 6);
		record.registered_at = sqlite3_column_int(stmt, 7);
		record.checked_in_at = sqlite3_column_int(stmt, 8);
		return record;
	}

	tournament_registration::RatingRecord read_rating(sqlite3_stmt* stmt) {
		tournament_registration::RatingRecord record;
		record.tournament_id = sqlite3_column_int(stmt, 0);
		record.discord_id = column_text(stmt, 1);
		record.rating_bucket = column_text(stmt, 2);
		record.rating_points = sqlite3_column_double(stmt, 3);
		record.source = column_text(stmt, 4);
		record.updated_by = column_text(stmt, 5);
		record.updated_at = sqlite3_column_int(stmt, 6);
		record.note = column_text(stmt, 7);
		return record;
	}

	tournament_registration::ParticipantResult fail(const std::string& message) {
		return tournament_registration::ParticipantResult{ false, message, std::nullopt };
	}

	tournament_registration::ParticipantResult ok(
		const std::string& message,
		const tournament_registration::ParticipantRecord& participant
	) {
		return tournament_registration::ParticipantResult{ true, message, participant };
	}
}

bool tournament_registration::init() {
	const char* participants_sql =
		"CREATE TABLE IF NOT EXISTS tournament_participants ("
		"tournament_id INTEGER NOT NULL, "
		"discord_id TEXT NOT NULL, "
		"display_name TEXT, "
		"provided_username TEXT, "
		"tetrio_id TEXT, "
		"status TEXT NOT NULL DEFAULT 'registered', "
		"seed INTEGER DEFAULT 0, "
		"registered_at INTEGER DEFAULT 0, "
		"checked_in_at INTEGER DEFAULT 0, "
		"PRIMARY KEY (tournament_id, discord_id), "
		"FOREIGN KEY (tournament_id) REFERENCES tournaments(id) ON DELETE CASCADE"
		");";

	const char* ratings_sql =
		"CREATE TABLE IF NOT EXISTS tournament_participant_ratings ("
		"tournament_id INTEGER NOT NULL, "
		"discord_id TEXT NOT NULL, "
		"rating_bucket TEXT NOT NULL, "
		"rating_points REAL NOT NULL, "
		"source TEXT DEFAULT 'staff', "
		"updated_by TEXT, "
		"updated_at INTEGER DEFAULT 0, "
		"note TEXT DEFAULT '', "
		"PRIMARY KEY (tournament_id, discord_id, rating_bucket), "
		"FOREIGN KEY (tournament_id, discord_id) REFERENCES tournament_participants(tournament_id, discord_id) ON DELETE CASCADE"
		");";

	return get_db().execute(participants_sql)
		&& get_db().execute(ratings_sql)
		&& get_db().create_index_if_missing(
			"idx_tournament_participants_tournament_status",
			"tournament_participants",
			"tournament_id, status"
		)
		&& get_db().create_index_if_missing(
			"idx_tournament_participants_tournament_seed",
			"tournament_participants",
			"tournament_id, seed"
		)
		&& get_db().create_index_if_missing(
			"idx_tournament_participant_ratings_bucket",
			"tournament_participant_ratings",
			"tournament_id, rating_bucket, rating_points"
		)
		&& get_db().set_schema_version(1);
}

tournament_registration::ParticipantResult tournament_registration::register_player(const RegistrationRequest& request) {
	if (request.tournament_id <= 0 || request.discord_id.empty()) {
		return fail("Invalid tournament or player.");
	}

	if (request.provided_username.empty()) {
		return fail("Please provide the username you want to register with.");
	}

	if (!request.linked_tetrio_id.empty() && !same_username(request.provided_username, request.linked_tetrio_id)) {
		return fail("That username does not match the TETR.IO account linked to your bot profile.");
	}

	if (!init()) {
		return fail("Could not initialize participant storage.");
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"INSERT INTO tournament_participants "
		"(tournament_id, discord_id, display_name, provided_username, tetrio_id, status, registered_at) "
		"VALUES (?, ?, ?, ?, ?, 'registered', ?) "
		"ON CONFLICT(tournament_id, discord_id) DO UPDATE SET "
		"display_name = excluded.display_name, "
		"provided_username = excluded.provided_username, "
		"tetrio_id = excluded.tetrio_id, "
		"status = CASE "
		"WHEN tournament_participants.status IN ('dropped', 'noshow') THEN 'registered' "
		"ELSE tournament_participants.status END;";

	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return fail("Could not prepare registration.");
	}

	const bool bound =
		sqlite3_bind_int(stmt, 1, request.tournament_id) == SQLITE_OK
		&& bind_text(stmt, 2, request.discord_id)
		&& bind_text(stmt, 3, request.display_name)
		&& bind_text(stmt, 4, request.provided_username)
		&& bind_text(stmt, 5, request.linked_tetrio_id)
		&& sqlite3_bind_int(stmt, 6, request.registered_at) == SQLITE_OK;

	const bool success = bound && sqlite3_step(stmt) == SQLITE_DONE;
	sqlite3_finalize(stmt);

	if (!success) {
		return fail("Could not register you for this tournament.");
	}

	auto participant = get_participant(request.tournament_id, request.discord_id);
	if (!participant) {
		return fail("Registration saved, but could not reload your participant record.");
	}

	return ok("You are registered for this tournament.", *participant);
}

tournament_registration::ParticipantResult tournament_registration::unregister_player(int tournament_id, const std::string& discord_id) {
	if (tournament_id <= 0 || discord_id.empty()) {
		return fail("Invalid tournament or player.");
	}

	if (!init()) {
		return fail("Could not initialize participant storage.");
	}

	auto participant = get_participant(tournament_id, discord_id);
	if (!participant) {
		return fail("You are not registered for this tournament.");
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"UPDATE tournament_participants "
		"SET status = 'dropped' "
		"WHERE tournament_id = ? AND discord_id = ?;";

	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return fail("Could not prepare unregister request.");
	}

	sqlite3_bind_int(stmt, 1, tournament_id);
	bind_text(stmt, 2, discord_id);

	const bool success = sqlite3_step(stmt) == SQLITE_DONE;
	sqlite3_finalize(stmt);

	if (!success) {
		return fail("Could not unregister you from this tournament.");
	}

	participant = get_participant(tournament_id, discord_id);
	return ok("You have been removed from this tournament.", *participant);
}

tournament_registration::ParticipantResult tournament_registration::check_in_player(const CheckInRequest& request) {
	if (request.tournament_id <= 0 || request.discord_id.empty()) {
		return fail("Invalid tournament or player.");
	}

	auto participant = get_participant(request.tournament_id, request.discord_id);
	if (!participant) {
		return fail("You are not registered for this tournament.");
	}

	if (!request.linked_tetrio_id.empty() && !same_username(request.provided_username, request.linked_tetrio_id)) {
		return fail("That username does not match the TETR.IO account linked to your bot profile.");
	}

	const int late_until = request.checkin_closes_at + request.grace_time;
	ParticipantStatus next_status = ParticipantStatus::CheckedIn;

	if (!request.staff_override && request.checkin_closes_at > 0) {
		if (request.now > late_until) {
			return fail("Check-in is closed, including the grace period.");
		}

		if (request.now > request.checkin_closes_at) {
			next_status = ParticipantStatus::LateCheckedIn;
		}
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"UPDATE tournament_participants "
		"SET status = ?, checked_in_at = ?, provided_username = ?, tetrio_id = ? "
		"WHERE tournament_id = ? AND discord_id = ?;";

	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return fail("Could not prepare check-in.");
	}

	const bool bound =
		bind_text(stmt, 1, status_to_string(next_status))
		&& sqlite3_bind_int(stmt, 2, request.now) == SQLITE_OK
		&& bind_text(stmt, 3, request.provided_username.empty() ? participant->provided_username : request.provided_username)
		&& bind_text(stmt, 4, request.linked_tetrio_id.empty() ? participant->tetrio_id : request.linked_tetrio_id)
		&& sqlite3_bind_int(stmt, 5, request.tournament_id) == SQLITE_OK
		&& bind_text(stmt, 6, request.discord_id);

	const bool success = bound && sqlite3_step(stmt) == SQLITE_DONE;
	sqlite3_finalize(stmt);

	if (!success) {
		return fail("Could not check you in.");
	}

	participant = get_participant(request.tournament_id, request.discord_id);
	return ok(next_status == ParticipantStatus::LateCheckedIn
		? "You are checked in during grace time."
		: "You are checked in.",
		*participant);
}

tournament_registration::ParticipantResult tournament_registration::undo_check_in(int tournament_id, const std::string& discord_id) {
	if (tournament_id <= 0 || discord_id.empty()) {
		return fail("Invalid tournament or player.");
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"UPDATE tournament_participants "
		"SET status = 'registered', checked_in_at = 0 "
		"WHERE tournament_id = ? AND discord_id = ?;";

	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return fail("Could not prepare check-in rollback.");
	}

	sqlite3_bind_int(stmt, 1, tournament_id);
	bind_text(stmt, 2, discord_id);

	const bool success = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().get_handle()) > 0;
	sqlite3_finalize(stmt);

	if (!success) {
		return fail("Could not undo check-in.");
	}

	auto participant = get_participant(tournament_id, discord_id);
	return ok("Check-in has been undone.", *participant);
}

bool tournament_registration::set_participant_seed(int tournament_id, const std::string& discord_id, int seed) {
	if (tournament_id <= 0 || discord_id.empty() || seed <= 0 || !init()) {
		return false;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"UPDATE tournament_participants "
		"SET seed = ? "
		"WHERE tournament_id = ? AND discord_id = ?;";

	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	sqlite3_bind_int(stmt, 1, seed);
	sqlite3_bind_int(stmt, 2, tournament_id);
	bind_text(stmt, 3, discord_id);

	const bool success = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().get_handle()) > 0;
	sqlite3_finalize(stmt);
	return success;
}

bool tournament_registration::set_participant_status(
	int tournament_id,
	const std::string& discord_id,
	ParticipantStatus status
) {
	if (tournament_id <= 0 || discord_id.empty() || !init()) {
		return false;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"UPDATE tournament_participants "
		"SET status = ? "
		"WHERE tournament_id = ? AND discord_id = ?;";

	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	bind_text(stmt, 1, status_to_string(status));
	sqlite3_bind_int(stmt, 2, tournament_id);
	bind_text(stmt, 3, discord_id);

	const bool success = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().get_handle()) > 0;
	sqlite3_finalize(stmt);
	return success;
}

bool tournament_registration::set_participant_rating(
	int tournament_id,
	const std::string& discord_id,
	const std::string& rating_bucket,
	double rating_points,
	const std::string& source,
	const std::string& updated_by,
	int updated_at,
	const std::string& note
) {
	if (tournament_id <= 0 || discord_id.empty() || rating_bucket.empty() || rating_points < 0.0 || !init()) {
		return false;
	}

	if (!get_participant(tournament_id, discord_id)) {
		return false;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"INSERT INTO tournament_participant_ratings "
		"(tournament_id, discord_id, rating_bucket, rating_points, source, updated_by, updated_at, note) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
		"ON CONFLICT(tournament_id, discord_id, rating_bucket) DO UPDATE SET "
		"rating_points = excluded.rating_points, "
		"source = excluded.source, "
		"updated_by = excluded.updated_by, "
		"updated_at = excluded.updated_at, "
		"note = excluded.note;";

	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	const bool bound =
		sqlite3_bind_int(stmt, 1, tournament_id) == SQLITE_OK
		&& bind_text(stmt, 2, discord_id)
		&& bind_text(stmt, 3, rating_bucket)
		&& sqlite3_bind_double(stmt, 4, rating_points) == SQLITE_OK
		&& bind_text(stmt, 5, source.empty() ? "staff" : source)
		&& bind_text(stmt, 6, updated_by)
		&& sqlite3_bind_int(stmt, 7, updated_at) == SQLITE_OK
		&& bind_text(stmt, 8, note);

	const bool success = bound && sqlite3_step(stmt) == SQLITE_DONE;
	sqlite3_finalize(stmt);
	return success;
}

bool tournament_registration::clear_participant_rating(
	int tournament_id,
	const std::string& discord_id,
	const std::string& rating_bucket
) {
	if (tournament_id <= 0 || discord_id.empty() || rating_bucket.empty() || !init()) {
		return false;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"DELETE FROM tournament_participant_ratings "
		"WHERE tournament_id = ? AND discord_id = ? AND rating_bucket = ?;";

	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	sqlite3_bind_int(stmt, 1, tournament_id);
	bind_text(stmt, 2, discord_id);
	bind_text(stmt, 3, rating_bucket);

	const bool success = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(get_db().get_handle()) > 0;
	sqlite3_finalize(stmt);
	return success;
}

std::optional<tournament_registration::ParticipantRecord> tournament_registration::get_participant(int tournament_id, const std::string& discord_id) {
	if (tournament_id <= 0 || discord_id.empty() || !init()) {
		return std::nullopt;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"SELECT tournament_id, discord_id, display_name, provided_username, tetrio_id, status, seed, registered_at, checked_in_at "
		"FROM tournament_participants "
		"WHERE tournament_id = ? AND discord_id = ? "
		"LIMIT 1;";

	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return std::nullopt;
	}

	sqlite3_bind_int(stmt, 1, tournament_id);
	bind_text(stmt, 2, discord_id);

	std::optional<tournament_registration::ParticipantRecord> result;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		result = read_participant(stmt);
	}

	sqlite3_finalize(stmt);
	return result;
}

std::optional<tournament_registration::RatingRecord> tournament_registration::get_participant_rating(
	int tournament_id,
	const std::string& discord_id,
	const std::string& rating_bucket
) {
	if (tournament_id <= 0 || discord_id.empty() || rating_bucket.empty() || !init()) {
		return std::nullopt;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"SELECT tournament_id, discord_id, rating_bucket, rating_points, source, updated_by, updated_at, note "
		"FROM tournament_participant_ratings "
		"WHERE tournament_id = ? AND discord_id = ? AND rating_bucket = ? "
		"LIMIT 1;";

	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return std::nullopt;
	}

	sqlite3_bind_int(stmt, 1, tournament_id);
	bind_text(stmt, 2, discord_id);
	bind_text(stmt, 3, rating_bucket);

	std::optional<tournament_registration::RatingRecord> result;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		result = read_rating(stmt);
	}

	sqlite3_finalize(stmt);
	return result;
}

std::vector<tournament_registration::ParticipantRecord> tournament_registration::list_participants(int tournament_id) {
	std::vector<tournament_registration::ParticipantRecord> result;
	if (tournament_id <= 0 || !init()) {
		return result;
	}

	sqlite3_stmt* stmt = nullptr;
	const char* sql =
		"SELECT tournament_id, discord_id, display_name, provided_username, tetrio_id, status, seed, registered_at, checked_in_at "
		"FROM tournament_participants "
		"WHERE tournament_id = ? "
		"ORDER BY seed = 0, seed ASC, registered_at ASC;";

	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return result;
	}

	sqlite3_bind_int(stmt, 1, tournament_id);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		result.push_back(read_participant(stmt));
	}

	sqlite3_finalize(stmt);
	return result;
}

std::vector<tournament_registration::ParticipantRecord> tournament_registration::list_checked_in_participants(int tournament_id) {
	std::vector<tournament_registration::ParticipantRecord> participants = list_participants(tournament_id);
	std::vector<tournament_registration::ParticipantRecord> result;

	for (const tournament_registration::ParticipantRecord& participant : participants) {
		if (participant.status == ParticipantStatus::CheckedIn
			|| participant.status == ParticipantStatus::LateCheckedIn) {
			result.push_back(participant);
		}
	}

	return result;
}

std::vector<tournament_registration::RatingRecord> tournament_registration::list_ratings(
	int tournament_id,
	const std::string& rating_bucket
) {
	std::vector<tournament_registration::RatingRecord> result;
	if (tournament_id <= 0 || !init()) {
		return result;
	}

	sqlite3_stmt* stmt = nullptr;
	const bool filter_bucket = !rating_bucket.empty();
	const char* sql_all =
		"SELECT tournament_id, discord_id, rating_bucket, rating_points, source, updated_by, updated_at, note "
		"FROM tournament_participant_ratings "
		"WHERE tournament_id = ? "
		"ORDER BY rating_bucket ASC, rating_points DESC, discord_id ASC;";
	const char* sql_bucket =
		"SELECT tournament_id, discord_id, rating_bucket, rating_points, source, updated_by, updated_at, note "
		"FROM tournament_participant_ratings "
		"WHERE tournament_id = ? AND rating_bucket = ? "
		"ORDER BY rating_points DESC, discord_id ASC;";
	const char* sql = filter_bucket ? sql_bucket : sql_all;

	if (sqlite3_prepare_v2(get_db().get_handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return result;
	}

	sqlite3_bind_int(stmt, 1, tournament_id);
	if (filter_bucket) {
		bind_text(stmt, 2, rating_bucket);
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		result.push_back(read_rating(stmt));
	}

	sqlite3_finalize(stmt);
	return result;
}

std::string tournament_registration::status_to_string(ParticipantStatus status) {
	switch (status) {
	case ParticipantStatus::Registered:
		return "registered";
	case ParticipantStatus::CheckedIn:
		return "checked_in";
	case ParticipantStatus::LateCheckedIn:
		return "late_checked_in";
	case ParticipantStatus::Dropped:
		return "dropped";
	case ParticipantStatus::Disqualified:
		return "disqualified";
	case ParticipantStatus::NoShow:
		return "noshow";
	}

	return "registered";
}

tournament_registration::ParticipantStatus tournament_registration::status_from_string(const std::string& status) {
	if (status == "checked_in") return ParticipantStatus::CheckedIn;
	if (status == "late_checked_in") return ParticipantStatus::LateCheckedIn;
	if (status == "dropped") return ParticipantStatus::Dropped;
	if (status == "disqualified") return ParticipantStatus::Disqualified;
	if (status == "noshow") return ParticipantStatus::NoShow;
	return ParticipantStatus::Registered;
}
