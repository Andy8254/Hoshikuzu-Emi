#pragma once

#include <string>

namespace sensitive_text {
	bool encryption_available();
	bool is_encrypted(const std::string& value);
	std::string protect(const std::string& plaintext);
	std::string reveal(const std::string& stored);
}
