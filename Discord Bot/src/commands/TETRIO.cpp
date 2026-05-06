#include "core/CommandRegistry.hpp"
#include "core/Localization.hpp"
#include "tetrio/TetrioService.hpp"
#include "tetrio/TetrioUtils.hpp"
#include "core/sqlite.hpp"
#include <thread>

/*
The following section deals with data retrieved from TETR.IO.
All API logic is abstracted into tetrio/TetrioService.
*/

void register_tetrio_commands(dpp::cluster& bot) {
	auto bot_ptr = &bot;

    handlers["tetrio"] = [bot_ptr](const dpp::slashcommand_t& event) {
        std::string username;

        auto username_param = event.get_parameter("username");

        if (username_param.index() != 0) {
            username = std::get<std::string>(username_param);
        }
        else {
            auto profile = PlayerManager::get_profile(event.command.usr.id);

            if (profile.empty() || !profile.count("tetrio_id") || profile["tetrio_id"].empty()) {
                event.reply(localization::message_text(event.command.guild_id, event.command.usr.id, "tetrio.link_required"));
                return;
            }

            username = profile["tetrio_id"];
        }

        event.thinking();

        // Copy event for async safety
        dpp::slashcommand_t ctx = event;

        // --- Async task ---
        auto fut = std::async(std::launch::async, [ctx, username]() mutable {
            try {
                auto profile = TetrioService::fetch_user(username);

                if (!profile) {
                    ctx.edit_response(localization::message_text(ctx.command.guild_id, ctx.command.usr.id, "tetrio.not_found"));
                    return;
                }

                // --- Formatting ---
                auto fmt = [](double v, int p = 2) {
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(p) << v;
                    return oss.str();
                    };

                std::string rank_display =
                    profile->rank.empty() || profile->rank == "Z"
                    ? "Unranked"
                    : profile->rank;

                std::string flag = profile->country.empty()
                    ? ""
                    : ":flag_" + profile->country + ":";

                //for lowercase letters
                std::transform(flag.begin(), flag.end(), flag.begin(),
                    [](unsigned char c) { return std::tolower(c); });

                std::string avatar_url =
                    "https://tetr.io/user-content/avatars/" + profile->id + ".jpg";

                // --- Embed ---
                dpp::embed embed;
                embed
                    .set_title("🎮 " + profile->username + "'s Profile")
                    .set_url("https://ch.tetr.io/u/" + profile->username)
                    .set_description(profile->bio.empty()
                        ? "*No introduction provided.*"
                        : profile->bio)
                    .set_thumbnail(avatar_url)
                    .set_color(get_rank_colour(profile->rank))

                    .add_field(
                        "🏆 RANK",
                        "**" + rank_display + "**\n"
                        "TR: " + fmt(profile->rating) + "\n"
                        "🌍" + ((profile->world_rank == -1) ? " Unranked" : " #" + std::to_string(profile->world_rank)) + "\n"
                        + (flag.empty() ? "" : flag) + ((profile->country_rank == -1) ? " Unranked" : +" #" + std::to_string(profile->country_rank)),
                        true
                    )

                    .add_field(
                        "📊 STATS",
                        "APM: " + fmt(profile->apm) + "\n"
                        "PPS: " + fmt(profile->pps) + "\n"
                        "VS : " + fmt(profile->vs),
                        true
                    )

                    .set_footer(dpp::embed_footer().set_text("Data from TETR.IO"))
                    .set_timestamp(time(nullptr));

                ctx.edit_response(dpp::message().add_embed(embed));
            }
            catch (const std::exception& e) {
                ctx.edit_response(std::string("❌ Error: ") + e.what());
            }
            catch (...) {
                ctx.edit_response(localization::message_text(ctx.command.guild_id, ctx.command.usr.id, "tetrio.unknown_error"));
            }
        });

        // ⚠️ IMPORTANT: keep future alive (prevents blocking fallback)
        static std::vector<std::future<void>> futures;
        futures.emplace_back(std::move(fut));
    };
}
