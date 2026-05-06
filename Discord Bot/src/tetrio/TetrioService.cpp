#include "tetrio/TetrioService.hpp"
#include "core/api_fetcher.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>

using json = nlohmann::json;

static std::string get_string(const json& obj, const std::string& key, const std::string& fallback = "") {
    if (obj.contains(key) && obj[key].is_string())
        return obj[key].get<std::string>();
    return fallback;
}

static double get_double(const json& obj, const std::string& key, double fallback = 0.0) {
    if (obj.contains(key) && obj[key].is_number())
        return obj[key].get<double>();
    return fallback;
}

static int get_int(const json& obj, const std::string& key, int fallback = 0) {
    if (obj.contains(key) && obj[key].is_number_integer())
        return obj[key].get<int>();
    return fallback;
}

static bool ok_response(const json& j) {
    return j.contains("success") && j["success"].is_boolean() && j["success"].get<bool>();
}

std::optional<TetrioProfile> TetrioService::fetch_user(const std::string& username) {
    TetrioProfile profile;

    auto user_res = HttpClient::get("https://ch.tetr.io/api/users/" + username);

    if (!user_res.error.empty() || user_res.status_code != 200) {
        return std::nullopt;
    }

    json user_j = json::parse(user_res.body, nullptr, false);
    if (user_j.is_discarded() || !ok_response(user_j)) {
        return std::nullopt;
    }

    if (!user_j.contains("data") || !user_j["data"].is_object()) {
        return std::nullopt;
    }

    const auto& user = user_j["data"];

    profile.id = get_string(user, "_id");
    profile.username = get_string(user, "username", username);
    profile.bio = get_string(user, "bio", "No introduction provided.");
    profile.country = get_string(user, "country");

    auto league_res = HttpClient::get(
        "https://ch.tetr.io/api/users/" + profile.id + "/summaries/league"
    );

    if (league_res.error.empty() && league_res.status_code == 200) {
        json league_j = json::parse(league_res.body, nullptr, false);

        if (!league_j.is_discarded()
            && ok_response(league_j)
            && league_j.contains("data")
            && league_j["data"].is_object()) {

            const auto& league = league_j["data"];

            profile.has_league_data = true;

            profile.rating = get_double(league, "tr");
            profile.rank = get_string(league, "rank", "Z");
            profile.top_rank = get_string(league, "bestrank", profile.rank);

            std::transform(
                profile.rank.begin(),
                profile.rank.end(),
                profile.rank.begin(),
                ::toupper
            );

            std::transform(
                profile.top_rank.begin(),
                profile.top_rank.end(),
                profile.top_rank.begin(),
                ::toupper
            );

            profile.world_rank = get_int(league, "standing");
            profile.country_rank = get_int(league, "standing_local");

            profile.apm = get_double(league, "apm");
            profile.pps = get_double(league, "pps");
            profile.vs = get_double(league, "vs");
        }
    }

    return profile;
}
