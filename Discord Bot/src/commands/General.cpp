#include "core/CommandRegistry.hpp"

//General Section, even though this will be a FAQ section since the basic commands were already registered in Fundamentals.cpp...
void register_general_commands(dpp::cluster& bot) {
	//Brief Command List
	handlers["help"] = [&bot](const dpp::slashcommand_t& event) {

	};

}