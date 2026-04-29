#include "tetrio/TetrioService.hpp"
#include "core/api_fetcher.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::optional<TetrioProfile> TetrioService::fetch_user(const std::string& username) {
	std::string url = "https://ch.tetr.io/api/users/" + username;

	auto res = HttpClient::get(url);

	if (!res.error.empty() || res.status_code != 200) {
		return std::nullopt;
	}

	json j = json::parse(res.body, nullptr, false);
	if (j.is_discarded() || !j.value("success", false)) {
		return std::nullopt;
	}

	auto user = j["data"]["user"];

	TetrioProfile profile;
	profile.username = user.value("username", "unknown");

	if (user.contains("league")) {
		auto league = user["league"];

		profile.rating = league.value("rating", 0);
		profile.rank = league.value("rank", "N/A");

		profile.apm = league.value("apm", 0.0);
		profile.pps = league.value("pps", 0.0);
		profile.vs = league.value("vs", 0.0);
	}
	return profile;
}