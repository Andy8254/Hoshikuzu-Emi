#include "core/CommandRegistry.hpp"
#include "misc/sqlite-user.hpp"

void register_misc_commands(dpp::cluster& bot) {
	(void)bot;
	misc_user_sqlite::init_user_database();
}
