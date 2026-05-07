#include <dpp/dpp.h>
#include "commands/CommandRegistration.hpp"
#include "commands/Discord_Commands.hpp"
#include "core/Config.hpp"
#include "core/SqlReliabilityCheck.hpp"
#include "interactions/InteractionHandlers.hpp"
#include <cstdlib>
#include <string>

const dpp::snowflake MOD_CHANNEL_ID = 1498301392073396234; //bot management server

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--sql-reliability-check") {
        const std::string db_path = argc >= 3 ? argv[2] : "db/sql_reliability_check.db";
        _putenv_s("BOT_DB_PATH", db_path.c_str());
        return sql_reliability::run(db_path);
    }

    dpp::cluster bot(get_bot_token());

    register_general_commands(bot);
    register_fundamental_commands(bot);
    register_misc_commands(bot);
    register_player_commands(bot);
    register_tetrio_commands(bot);
    register_settings_commands(bot);
    register_moderation_commands(bot);
    register_tournament_commands(bot);

    register_discord_commands(bot, MOD_CHANNEL_ID);
    register_interaction_handlers(bot);

    bot.start(dpp::st_wait);
}
