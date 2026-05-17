#include "core/CommandRegistry.hpp"
#include "misc/isolated/youtube_randomizer.hpp"
#include "misc/sqlite-user.hpp"
#include "misc/trusted/mahjong/Module.hpp"

void register_misc_commands(dpp::cluster& bot) {
	(void)bot;
	misc_user_sqlite::init_user_database();
	misc_isolated_youtube_randomizer::init();
	misc_trusted_mahjong::init();
}
