#pragma once
#include <dpp/dispatcher.h>   // slashcommand_t
#include <dpp/snowflake.h>

// --- Discord-level authority ---
enum class DiscordLevel {
    USER,
    MODERATOR,
    ADMIN,
    OWNER
};

// --- Tournament-level roles ---
enum class TournamentRole {
    NONE,
    PLAYER,
    STAFF,
    ADMIN
};

class PermissionManager {
public:
    // --- Discord authority ---
    static DiscordLevel get_discord_level(const dpp::slashcommand_t& event);

    static bool is_mod(const dpp::slashcommand_t& event);
    static bool is_admin(const dpp::slashcommand_t& event);

    // --- Tournament roles ---
    static TournamentRole get_tournament_role(const dpp::slashcommand_t& event);

    // --- Core permission checks ---
    static bool can_report_match(const dpp::slashcommand_t& event, bool is_override = false);

private:
    // --- Helpers ---
    static bool has_role(const dpp::slashcommand_t& event, dpp::snowflake role_id);

    // --- Role resolution (DB-ready abstraction) ---
    static dpp::snowflake get_tournament_staff_role_id(dpp::snowflake guild_id);
};