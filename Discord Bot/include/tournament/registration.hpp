#pragma once
#include <optional>
#include <string>
#include <vector>

namespace tournament_registration {
	inline constexpr int DEFAULT_CHECKIN_GRACE_TIME = 600;

	enum class ParticipantStatus {
		Registered,
		CheckedIn,
		LateCheckedIn,
		Dropped,
		Disqualified,
		NoShow
	};

	struct ParticipantRecord {
		int tournament_id = 0;
		std::string discord_id;
		std::string display_name;
		std::string provided_username;
		std::string tetrio_id;
		ParticipantStatus status = ParticipantStatus::Registered;
		int seed = 0;
		int registered_at = 0;
		int checked_in_at = 0;
	};

	struct RegistrationRequest {
		int tournament_id = 0;
		std::string discord_id;
		std::string display_name;
		std::string provided_username;
		std::string linked_tetrio_id;
		int registered_at = 0;
	};

	struct CheckInRequest {
		int tournament_id = 0;
		std::string discord_id;
		std::string provided_username;
		std::string linked_tetrio_id;
		int now = 0;
		int checkin_closes_at = 0;
		int grace_time = DEFAULT_CHECKIN_GRACE_TIME;
		bool staff_override = false;
	};

	struct ParticipantResult {
		bool ok = false;
		std::string message;
		std::optional<ParticipantRecord> participant;
	};

	bool init();

	ParticipantResult register_player(const RegistrationRequest& request);
	ParticipantResult unregister_player(int tournament_id, const std::string& discord_id);
	ParticipantResult check_in_player(const CheckInRequest& request);
	ParticipantResult undo_check_in(int tournament_id, const std::string& discord_id);
	bool set_participant_seed(int tournament_id, const std::string& discord_id, int seed);
	bool set_participant_status(int tournament_id, const std::string& discord_id, ParticipantStatus status);

	std::optional<ParticipantRecord> get_participant(int tournament_id, const std::string& discord_id);
	std::vector<ParticipantRecord> list_participants(int tournament_id);
	std::vector<ParticipantRecord> list_checked_in_participants(int tournament_id);

	std::string status_to_string(ParticipantStatus status);
	ParticipantStatus status_from_string(const std::string& status);
}
