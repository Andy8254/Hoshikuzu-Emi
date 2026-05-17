#include "core/CommandRegistry.hpp"
#include "core/Log.hpp"
#include "misc/isolated/youtube_randomizer.hpp"
#include "misc/sqlite-user.hpp"
#include "misc/trusted/mahjong/Module.hpp"

void register_misc_commands(dpp::cluster& bot) {
	(void)bot;
	const bool user_db_ok = misc_user_sqlite::init_user_database();
	bot_log::write(user_db_ok ? "INFO" : "ERROR", "misc", "user_db_init", user_db_ok ? "status=ok" : "status=failed");

	const bool youtube_ok = misc_isolated_youtube_randomizer::init();
	bot_log::info("misc", "youtube_randomizer_init", youtube_ok ? "status=ok" : "status=skipped_or_failed");

	const bool mahjong_ok = misc_trusted_mahjong::init();
	bot_log::info("misc", "trusted_mahjong_init", mahjong_ok ? "status=enabled" : "status=disabled");
}
