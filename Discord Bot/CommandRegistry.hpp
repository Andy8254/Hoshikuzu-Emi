#pragma once
#include <dpp/dpp.h>
#include <unordered_map>
#include <functional>
using CommandHandler = std::function<void(const dpp::slashcommand_t&)>;
extern std::unordered_map<std::string, CommandHandler> handlers;