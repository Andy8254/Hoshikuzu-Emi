#include "core/CommandRegistry.hpp"
#include "core/Fundamentals.hpp"

//Register all command handlers here (This will include hundreds, or even thousands of command handlers, so it's best to keep them organized in a separate file like this)
void register_fundamental_commands(dpp::cluster& bot) {
    auto bot_ptr = &bot;

    handlers["ping"] = [](const dpp::slashcommand_t& event) {
        event.reply("Pong!");
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
        event.reply(
            "📄 **Privacy Notice**\n"
            "Hi there~ Emi here! 🌸\n"
            "\n"
            "I only keep a tiny bit of information so I can help you properly:\n"
            "• Your Discord ID\n"
            "• Your linked TETR.IO username\n"
            "• Server settings (like roles)\n"
            "\n"
            "I don't collect anything personal, and I would never sell or share your data.\n"
            "Everything is safely kept on the host machine I live in~ ✨\n"
            "\n"
            "If you ever want your data removed, just use `/unlink` or `/unlink_platform`.\n"
            "\n"
            "Full details: <GitHub link>"
        );
    };
}