#pragma once
#include <optional>
#include <string>
#include <vector>

namespace tournament_manage {
	struct TournamentRecord {
		int id = 0;
		std::string name;
		std::string game_type;
		std::string status;
	};

	struct TournamentUpdate {
		std::optional<std::string> name;
		std::optional<std::string> game_type;
		std::optional<std::string> status;
	};

	bool init();

	std::optional<int> create_tournament(
		const std::string& name,
		const std::string& game_type,
		const std::string& status = "open"
	);

	bool update_tournament(int tournament_id, const TournamentUpdate& update);
	bool delete_tournament(int tournament_id);

	std::optional<TournamentRecord> get_tournament(int tournament_id);
	std::vector<TournamentRecord> list_tournaments();
	std::vector<TournamentRecord> list_tournaments_by_status(const std::string& status);
}
