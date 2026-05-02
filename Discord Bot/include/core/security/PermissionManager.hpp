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
    static bool is_tournament_staff(const dpp::slashcommand_t& event);
    static bool is_tournament_admin(const dpp::slashcommand_t& event);

    // --- Core permission checks ---
    static bool can_configure_tournament_roles(const dpp::slashcommand_t& event);
    static bool can_manage_tournament(const dpp::slashcommand_t& event);
    static bool can_admin_tournament(const dpp::slashcommand_t& event);
    static bool can_report_match(const dpp::slashcommand_t& event, bool is_override = false);

private:
    // --- Helpers ---
    static bool has_role(const dpp::slashcommand_t& event, dpp::snowflake role_id);
    static int discord_rank(DiscordLevel level);
    static int tournament_rank(TournamentRole role);

    // --- Role resolution (DB-ready abstraction) ---
    static dpp::snowflake get_tournament_staff_role_id(dpp::snowflake guild_id);
    static dpp::snowflake get_tournament_admin_role_id(dpp::snowflake guild_id);
};
