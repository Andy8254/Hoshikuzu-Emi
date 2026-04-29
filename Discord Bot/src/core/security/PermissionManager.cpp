#include "core/security/PermissionManager.hpp"
#include "core/sqlite.hpp" //for later use?
#include <algorithm>

// --- Discord authority ---

DiscordLevel PermissionManager::get_discord_level(const dpp::slashcommand_t & event) {
    // DM safety
    if (!event.command.guild_id) {
        return DiscordLevel::USER;
    }

    // ✅ This works in all modern DPP versions
    dpp::permission perms = event.command.get_resolved_permission(event.command.guild_id);

    if (perms & dpp::p_administrator) {
        return DiscordLevel::ADMIN;
    }

    if (perms & dpp::p_manage_guild ||
        perms & dpp::p_kick_members ||
        perms & dpp::p_ban_members) {
        return DiscordLevel::MODERATOR;
    }

    return DiscordLevel::USER;
}

bool PermissionManager::is_mod(const dpp::slashcommand_t& event) {
    return get_discord_level(event) >= DiscordLevel::MODERATOR;
}

bool PermissionManager::is_admin(const dpp::slashcommand_t& event) {
    return get_discord_level(event) >= DiscordLevel::ADMIN;
}

// --- Helpers ---

bool PermissionManager::has_role(const dpp::slashcommand_t& event, dpp::snowflake role_id) {
    const auto& roles = event.command.member.get_roles();
    return std::find(roles.begin(), roles.end(), role_id) != roles.end();
}

// --- Tournament role logic ---
TournamentRole PermissionManager::get_tournament_role(const dpp::slashcommand_t& event) {
    dpp::snowflake guild_id = event.command.guild_id;

    // DM safety
    if (!guild_id) {
        return TournamentRole::NONE;
    }

    // Get configured roles from DB
    dpp::snowflake staff_role = GuildConfigManager::get_staff_role(guild_id);
    dpp::snowflake admin_role = GuildConfigManager::get_admin_role(guild_id);

    // Check roles (admin first = higher priority)
    if (admin_role && has_role(event, admin_role)) {
        return TournamentRole::ADMIN;
    }

    if (staff_role && has_role(event, staff_role)) {
        return TournamentRole::STAFF;
    }

    return TournamentRole::PLAYER;
}

// --- Permission logic ---
bool PermissionManager::can_report_match(const dpp::slashcommand_t& event, bool is_override) {
    auto discord = get_discord_level(event);
    auto role = get_tournament_role(event);

    if (is_override) {
        // Mods OR tournament staff can override
        return discord >= DiscordLevel::MODERATOR || role == TournamentRole::STAFF;
    }

    // Normal reporting: players + staff allowed
    return role == TournamentRole::PLAYER || role == TournamentRole::STAFF;
}