#ifndef CORE_LOG_HPP
#define CORE_LOG_HPP

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace bot_log {

	inline std::string timestamp() {
		const auto now = std::chrono::system_clock::now();
		const std::time_t time = std::chrono::system_clock::to_time_t(now);
		std::tm local{};
		localtime_s(&local, &time);

		std::ostringstream out;
		out << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
		return out.str();
	}

	inline void write(const std::string& level, const std::string& module, const std::string& action, const std::string& detail = "") {
		static std::mutex log_mutex;
		std::lock_guard<std::mutex> lock(log_mutex);

		std::cerr << "[" << timestamp() << "]"
			<< "[" << level << "]"
			<< "[" << module << "] "
			<< action;

		if (!detail.empty()) {
			std::cerr << " " << detail;
		}

		std::cerr << std::endl;
	}

	inline void info(const std::string& module, const std::string& action, const std::string& detail = "") {
		write("INFO", module, action, detail);
	}

	inline void warn(const std::string& module, const std::string& action, const std::string& detail = "") {
		write("WARN", module, action, detail);
	}

	inline void error(const std::string& module, const std::string& action, const std::string& detail = "") {
		write("ERROR", module, action, detail);
	}

}

#endif
