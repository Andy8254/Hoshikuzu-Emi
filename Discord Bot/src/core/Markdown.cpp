#include "core/Markdown.hpp"
#include <fstream>
#include <sstream>

namespace md {
	std::string read_file(const std::string& path) {
		std::ifstream file(path);

		//cannot open file, return empty string
		if (!file.is_open()) {
			return "";
		}

		//if else, read the file into a string and return it
		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}
}

dpp::message md::to_message(const std::string& markdown) {
	dpp::embed embed;
	embed.set_color(0x5865F2);

	if (markdown.empty()) {
		embed.set_title("Error")
			.set_description("Help file could not be loaded.");
		return dpp::message().add_embed(embed);
	}

	std::istringstream stream(markdown);
	std::string line;

	bool has_description = false;

	while (std::getline(stream, line)) {
		if (line.starts_with("# ")) {
			embed.set_title(line.substr(2));
		}

		else if (line.starts_with("## ")) {
			std::string field_name = line.substr(3);
			std::string field_value;

			if (std::getline(stream, field_value)) {
				if (!field_value.empty()) {
					embed.add_field(field_name, field_value, false);
				}
			}
		}

		else if (!line.empty() && !has_description) {
			embed.set_description(line);
			has_description = true;
		}
	}

	return dpp::message().add_embed(embed);
}