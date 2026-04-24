#pragma once
#pragma warning(disable : 4996) // Disable warning about getenv being unsafe in Visual Studio
#include <cstdlib>
#include <stdexcept>
#include <string>

//Configuration for the Discord Bot(e.g. token, prefix, etc.)

std::string get_bot_token() {
	const char* token = std::getenv("BOT_TOKEN");
	if (!token || std::string(token).empty()) {
		throw std::runtime_error("Error: BOT_TOKEN environment variable is not set or is empty.");
	}
	return std::string(token);
}