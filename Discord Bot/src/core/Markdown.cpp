#include "core/Markdown.hpp"
#include <fstream>
#include <sstream>

namespace {
	void trim_trailing_blank_lines(std::string& value) {
		while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
			value.pop_back();
		}
	}
}

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
	std::string current_field_name;
	std::string current_field_value;

	auto flush_field = [&]() {
		trim_trailing_blank_lines(current_field_value);
		if (!current_field_name.empty() && !current_field_value.empty()) {
			embed.add_field(current_field_name, current_field_value, false);
		}

		current_field_name.clear();
		current_field_value.clear();
	};

	while (std::getline(stream, line)) {
		if (line.starts_with("# ")) {
			flush_field();
			embed.set_title(line.substr(2));
		}

		else if (line.starts_with("## ")) {
			flush_field();
			current_field_name = line.substr(3);
		}

		else if (!current_field_name.empty()) {
			if (current_field_value.empty() && line.empty()) {
				continue;
			}

			if (!current_field_value.empty()) {
				current_field_value += "\n";
			}
			current_field_value += line;
		}

		else if (!line.empty() && !has_description) {
			embed.set_description(line);
			has_description = true;
		}
	}

	flush_field();

	return dpp::message().add_embed(embed);
}
