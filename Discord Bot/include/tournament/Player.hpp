#pragma once
#include <dpp/dpp.h>
#include <string>
#include <unordered_map>

//can be manually tinkered with (only when directly connected with Discord ID) 
enum class Platform {
	TETRIO,
	PPT2,
	NES,
	Tetra_eSports,
	TGM,
	Other
};

struct Player {
	dpp::snowflake discord_id;
	std::string discord_name;
	std::unordered_map<Platform, std::string> accounts;
	long double rank_point = 0.0L; //will be used for ranking point calculations

	std::string get_name(Platform platform) const {
		auto it = accounts.find(platform);
		return (it != accounts.end()) ? it->second : "N/A";
	}

	void set_name(Platform platform, const std::string& name) {
		accounts[platform] = name;
	}
};
