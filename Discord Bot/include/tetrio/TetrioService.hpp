#pragma once
#include <string>
#include <optional>

struct TetrioProfile {
	std::string id;
	std::string username;
	int rating = 0;
	std::string rank = "N/A";

	double apm = 0.0;
	double pps = 0.0;
	double vs = 0.0;
};

class TetrioService {
public:
	static std::optional<TetrioProfile> fetch_user(const std::string& username);
};