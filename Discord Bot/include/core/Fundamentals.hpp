#pragma once
#include <string>
#include <vector>
#include <cstdlib>
#include <random>

const std::string MASTER_ID = "543676141177798676";

/*
Useless header file for a simple hello message.XD
Since it's useless, This should be a good place to store an easter egg, right?

Maybe I should ask other TOs how they want Emi to react when she happens to bump into them, huh...?
*/

inline std::string get_hello_message(const std::string& mention = "") {
    // Extract ID from mention format <@123> or <@!123>
    std::string id;

    if (!mention.empty()) {
        size_t start = mention.find_first_of("0123456789");
        size_t end = mention.find_last_of("0123456789");

        if (start != std::string::npos && end != std::string::npos) {
            id = mention.substr(start, end - start + 1);
        }
    }

    // 🎯 Easter egg (check FIRST)
    if (!id.empty() && id == MASTER_ID) {
        return "Eh—wait... it's you?! Σ(･ω･ﾉ)ﾉ！\nWhat are you doing here ? (^▽^;)";
    }

    // Normal mention reply
    if (!mention.empty()) {
        return "Welcome back, " + mention + "! How's your day today? (^▽^)\n"
            "Try `/codex` if you need help!";
    }

    // Random fallback
    static std::vector<std::string> msgs = {
        "I'm here! Ready to get started? ✨",
        "Hi there! Emi here! (^▽^)",
        "Hi hi~ I'm here and ready! 🌼🌼",
        "Hey! Good to see you! (^▽^)"
    };

    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, msgs.size() - 1);

    return msgs[dist(rng)];
}