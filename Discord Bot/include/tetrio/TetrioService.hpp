#pragma once
#include <string>
#include <optional>

struct TetrioProfile {
	std::string id;
	std::string username;
	std::string bio;
	std::string country; //絵文字のコードを基準による - Random Japanese Comment from a South Korean programmer. XD

	double rating = 0;
	std::string rank = "Z";
	std::string top_rank = "Z";

	int world_rank = 0;
	int country_rank = 0;

	double apm = 0.0;
	double pps = 0.0;
	double vs = 0.0;

	bool has_league_data = false;
};

class TetrioService {
public:
	static std::optional<TetrioProfile> fetch_user(const std::string& username);
};
