#pragma once
#include <dpp/dpp.h>

//This file contains all the command handlers for the Discord Bot, organized by category. Each command handler will be responsible for handling a specific set of commands related to its category. This allows for better organization and maintainability of the codebase.
void register_fundamental_commands(dpp::cluster& bot); //basic commands - done
void register_general_commands(dpp::cluster& bot); //help commands - done
void register_player_commands(dpp::cluster& bot);
void register_tetrio_commands(dpp::cluster& bot);
void register_tournament_commands(dpp::cluster& bot);
void register_bracket_commands(dpp::cluster& bot);
void register_moderation_commands(dpp::cluster& bot);
void register_misc_commands(dpp::cluster& bot);
