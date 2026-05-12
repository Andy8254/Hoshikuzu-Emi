#pragma once
#pragma warning(disable : 4996) // Disable warning about getenv being unsafe in Visual Studio
#include <cstdlib>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

//Configuration for the Discord Bot(e.g. token, prefix, etc.)

namespace config_detail {
	inline std::string trim(std::string value) {
		const auto first = value.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) {
			return "";
		}

		const auto last = value.find_last_not_of(" \t\r\n");
		return value.substr(first, last - first + 1);
	}

	inline bool valid_env_name(const std::string& name) {
		if (name.empty()) {
			return false;
		}

		const unsigned char first = static_cast<unsigned char>(name.front());
		if (!(std::isalpha(first) || name.front() == '_')) {
			return false;
		}

		for (const char c : name) {
			const unsigned char value = static_cast<unsigned char>(c);
			if (!(std::isalnum(value) || c == '_')) {
				return false;
			}
		}

		return true;
	}

	inline std::string unquote(std::string value) {
		value = trim(value);
		if (value.size() >= 2) {
			const char quote = value.front();
			if ((quote == '"' || quote == '\'') && value.back() == quote) {
				value = value.substr(1, value.size() - 2);
			}
		}
		return value;
	}

	inline bool env_is_set(const std::string& name) {
		const char* value = std::getenv(name.c_str());
		return value != nullptr;
	}

	inline void set_env_value(const std::string& name, const std::string& value) {
#ifdef _WIN32
		_putenv_s(name.c_str(), value.c_str());
#else
		setenv(name.c_str(), value.c_str(), 1);
#endif
	}
}

inline bool load_env_file(const std::string& path, bool overwrite_existing = false) {
	if (path.empty() || !std::filesystem::exists(path)) {
		return false;
	}

	std::ifstream file(path);
	if (!file.is_open()) {
		return false;
	}

	std::string line;
	while (std::getline(file, line)) {
		line = config_detail::trim(line);
		if (line.empty() || line.front() == '#') {
			continue;
		}

		if (line.rfind("export ", 0) == 0) {
			line = config_detail::trim(line.substr(7));
		}

		const std::size_t separator = line.find('=');
		if (separator == std::string::npos) {
			continue;
		}

		const std::string name = config_detail::trim(line.substr(0, separator));
		const std::string value = config_detail::unquote(line.substr(separator + 1));
		if (!config_detail::valid_env_name(name)) {
			continue;
		}

		if (!overwrite_existing && config_detail::env_is_set(name)) {
			continue;
		}

		config_detail::set_env_value(name, value);
	}

	return true;
}

inline bool load_default_env_file() {
	const char* configured = std::getenv("BOT_ENV_FILE");
	if (configured && *configured) {
		return load_env_file(configured);
	}

	return load_env_file(".env");
}

inline std::string get_bot_token() {
	const char* token = std::getenv("BOT_TOKEN");
	if (!token || std::string(token).empty()) {
		throw std::runtime_error("Error: BOT_TOKEN environment variable is not set or is empty.");
	}

	std::string value(token);
	if (value.size() < 32) {
		throw std::runtime_error("Error: BOT_TOKEN looks too short. Check the injected environment value.");
	}

	return value;
}
