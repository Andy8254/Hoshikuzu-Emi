#pragma once
#include <dpp/snowflake.h>
#include <map>
#include <string>

namespace localization {
	using Params = std::map<std::string, std::string>;

	inline constexpr const char* DEFAULT_LANGUAGE = "EN-gb";

	bool init();
	bool is_supported_language(const std::string& language);
	std::string guild_language(dpp::snowflake guild_id);
	std::string user_language(dpp::snowflake user_id);
	std::string primary_language(dpp::snowflake guild_id, dpp::snowflake user_id);
	std::string secondary_language(dpp::snowflake guild_id);
	std::string text(const std::string& language, const std::string& key, const Params& params = {});
	std::string text_or_empty(const std::string& language, const std::string& key, const Params& params = {});
	std::string guild_text(dpp::snowflake guild_id, const std::string& key, const Params& params = {});
	std::string user_text(dpp::snowflake guild_id, dpp::snowflake user_id, const std::string& key, const Params& params = {});
	std::string message_text(dpp::snowflake guild_id, dpp::snowflake user_id, const std::string& key, const Params& params = {});
	std::string embed_text(dpp::snowflake guild_id, const std::string& key, const Params& params = {});
}
