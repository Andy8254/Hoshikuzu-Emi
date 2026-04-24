#pragma once
#include <dpp/dpp.h>
#include <string>

namespace md {
	std::string read_file(const std::string& path);
	dpp::message to_message(const std::string& markdown);
}