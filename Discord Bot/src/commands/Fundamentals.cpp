#include "core/CommandRegistry.hpp"
#include "core/Fundamentals.hpp"
#include "core/Localization.hpp"

//Register all command handlers here (This will include hundreds, or even thousands of command handlers, so it's best to keep them organized in a separate file like this)
void register_fundamental_commands(dpp::cluster& bot) {
    auto bot_ptr = &bot;

    handlers["ping"] = [](const dpp::slashcommand_t& event) {
        event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "bot.ping"));
        };

    handlers["info"] = [bot_ptr](const dpp::slashcommand_t& event) {
        dpp::embed info_embed = dpp::embed()
            .set_title("Bot Information")
            .set_description("Emi here! Hope I can assist you even with my modest power! (^▽^)")
            .set_color(0xB0D28F)
            .set_thumbnail(bot_ptr->me.get_avatar_url())
            .add_field("Version", "alpha1", true)
            .add_field("Library", "DPP 10.1.4", true)
            .add_field("Platform", "C++20", true)
            .add_field("Latency", std::to_string(bot_ptr->rest_ping) + " ms", true)
            .add_field("Developer", "Kyoung-Hwan \"Andy8254\" Choi(Stacking Arena)", true)
            .add_field("Contact(E-mail)", "ajm8254@gmail.com", true)
            .set_footer(dpp::embed_footer().set_text("Stacking Arena : We connect the world with puzzle games."))
            .set_timestamp(time(0));
        event.reply(dpp::message().add_embed(info_embed));
    };

    handlers["hello"] = [](const dpp::slashcommand_t& event) {
        event.reply(get_hello_message(event.command.usr.get_mention()));
    };

    handlers["privacy"] = [](const dpp::slashcommand_t& event) {
        event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "bot.privacy"));
    };

    handlers["bot"] = [](const dpp::slashcommand_t& event) {
        const auto interaction = event.command.get_command_interaction();
        if (interaction.options.empty()) {
            event.reply(dpp::message(localization::user_text(event.command.guild_id, event.command.usr.id, "bot.choose_subcommand")).set_flags(dpp::m_ephemeral));
            return;
        }

        const std::string& subcommand = interaction.options.front().name;
        const std::string target = subcommand == "help" ? "codex" : subcommand;
        const auto it = handlers.find(target);
        if (it != handlers.end()) {
            it->second(event);
            return;
        }

        event.reply(dpp::message(localization::user_text(event.command.guild_id, event.command.usr.id, "bot.unknown_subcommand")).set_flags(dpp::m_ephemeral));
    };
}
