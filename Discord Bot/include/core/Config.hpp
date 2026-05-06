#pragma once
#pragma warning(disable : 4996) // Disable warning about getenv being unsafe in Visual Studio
#include <cstdlib>
#include <stdexcept>
#include <string>

//Configuration for the Discord Bot(e.g. token, prefix, etc.)

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
