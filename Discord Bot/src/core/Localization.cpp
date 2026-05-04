#include "core/Localization.hpp"
#include "core/sqlite.hpp"
#include <filesystem>
#include <fstream>
#include <cctype>
#include <map>
#include <mutex>
#include <sstream>

namespace {
	using Dictionary = std::map<std::string, std::string>;

	std::once_flag load_once;
	std::map<std::string, Dictionary> dictionaries;

	std::string read_file(const std::filesystem::path& path) {
		std::ifstream file(path);
		if (!file) {
			return "";
		}

		std::ostringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}

	std::string unescape_json_string(const std::string& value) {
		std::string result;
		result.reserve(value.size());

		for (size_t i = 0; i < value.size(); ++i) {
			if (value[i] != '\\' || i + 1 >= value.size()) {
				result += value[i];
				continue;
			}

			const char next = value[++i];
			switch (next) {
			case 'n': result += '\n'; break;
			case 'r': result += '\r'; break;
			case 't': result += '\t'; break;
			case '"': result += '"'; break;
			case '\\': result += '\\'; break;
			default: result += next; break;
			}
		}

		return result;
	}

	Dictionary parse_flat_json_strings(const std::string& json) {
		Dictionary result;
		size_t pos = 0;

		while (true) {
			const size_t key_start = json.find('"', pos);
			if (key_start == std::string::npos) break;
			const size_t key_end = json.find('"', key_start + 1);
			if (key_end == std::string::npos) break;

			const std::string key = json.substr(key_start + 1, key_end - key_start - 1);
			pos = key_end + 1;

			const size_t colon = json.find(':', pos);
			if (colon == std::string::npos) break;
			pos = colon + 1;

			while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
				++pos;
			}

			if (pos >= json.size() || json[pos] != '"') {
				continue;
			}

			++pos;
			std::string value;
			bool escaped = false;
			for (; pos < json.size(); ++pos) {
				const char c = json[pos];
				if (!escaped && c == '"') {
					++pos;
					break;
				}
				value += c;
				escaped = !escaped && c == '\\';
				if (c != '\\') {
					escaped = false;
				}
			}

			if (key.rfind("_", 0) != 0) {
				result[key] = unescape_json_string(value);
			}
		}

		return result;
	}

	void load_dictionaries() {
		const std::filesystem::path lang_dir("resources/lang");
		if (!std::filesystem::exists(lang_dir)) {
			return;
		}

		for (const auto& entry : std::filesystem::directory_iterator(lang_dir)) {
			if (!entry.is_regular_file() || entry.path().extension() != ".json") {
				continue;
			}

			const std::string language = entry.path().stem().string();
			Dictionary dictionary = parse_flat_json_strings(read_file(entry.path()));
			if (!dictionary.empty()) {
				dictionaries[language] = std::move(dictionary);
			}
		}
	}

	std::string apply_params(std::string value, const localization::Params& params) {
		for (const auto& [key, replacement] : params) {
			const std::string token = "{" + key + "}";
			size_t pos = 0;
			while ((pos = value.find(token, pos)) != std::string::npos) {
				value.replace(pos, token.size(), replacement);
				pos += replacement.size();
			}
		}
		return value;
	}
}

bool localization::init() {
	std::call_once(load_once, load_dictionaries);
	return !dictionaries.empty();
}

bool localization::is_supported_language(const std::string& language) {
	init();
	return dictionaries.find(language) != dictionaries.end();
}

std::string localization::guild_language(dpp::snowflake guild_id) {
	const std::string language = ServerSettingsManager::get_language(guild_id);
	return is_supported_language(language) ? language : DEFAULT_LANGUAGE;
}

std::string localization::user_language(dpp::snowflake user_id) {
	const std::string language = UserSettingsManager::get_language(user_id);
	return is_supported_language(language) ? language : "";
}

std::string localization::primary_language(dpp::snowflake guild_id, dpp::snowflake user_id) {
	const std::string user_preference = user_language(user_id);
	if (!user_preference.empty()) {
		return user_preference;
	}

	return guild_language(guild_id);
}

std::string localization::secondary_language(dpp::snowflake guild_id) {
	const std::string language = ServerSettingsManager::get_secondary_language(guild_id);
	return is_supported_language(language) ? language : "";
}

std::string localization::text(const std::string& language, const std::string& key, const Params& params) {
	init();

	auto language_it = dictionaries.find(language);
	if (language_it != dictionaries.end()) {
		auto text_it = language_it->second.find(key);
		if (text_it != language_it->second.end()) {
			return apply_params(text_it->second, params);
		}
	}

	auto fallback_it = dictionaries.find(DEFAULT_LANGUAGE);
	if (fallback_it != dictionaries.end()) {
		auto text_it = fallback_it->second.find(key);
		if (text_it != fallback_it->second.end()) {
			return apply_params(text_it->second, params);
		}
	}

	return key;
}

std::string localization::text_or_empty(const std::string& language, const std::string& key, const Params& params) {
	init();

	auto language_it = dictionaries.find(language);
	if (language_it == dictionaries.end()) {
		return "";
	}

	auto text_it = language_it->second.find(key);
	if (text_it == language_it->second.end()) {
		return "";
	}

	return apply_params(text_it->second, params);
}

std::string localization::guild_text(dpp::snowflake guild_id, const std::string& key, const Params& params) {
	return text(guild_language(guild_id), key, params);
}

std::string localization::user_text(dpp::snowflake guild_id, dpp::snowflake user_id, const std::string& key, const Params& params) {
	return text(primary_language(guild_id, user_id), key, params);
}

std::string localization::message_text(dpp::snowflake guild_id, dpp::snowflake user_id, const std::string& key, const Params& params) {
	const std::string primary = primary_language(guild_id, user_id);
	std::string result = text(primary, key, params);

	const std::string secondary = secondary_language(guild_id);
	if (!secondary.empty() && secondary != primary) {
		const std::string secondary_value = text_or_empty(secondary, key, params);
		if (!secondary_value.empty() && secondary_value != result) {
			result += "\n" + secondary_value;
		}
	}

	return result;
}

std::string localization::embed_text(dpp::snowflake guild_id, const std::string& key, const Params& params) {
	return text(guild_language(guild_id), key, params);
}
