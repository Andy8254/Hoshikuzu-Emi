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
    return discord_rank(get_discord_level(event)) >= discord_rank(DiscordLevel::MODERATOR);
}

bool PermissionManager::is_admin(const dpp::slashcommand_t& event) {
    return discord_rank(get_discord_level(event)) >= discord_rank(DiscordLevel::ADMIN);
}

// --- Helpers ---

bool PermissionManager::has_role(const dpp::slashcommand_t& event, dpp::snowflake role_id) {
    if (!role_id) {
        return false;
    }

    const auto& roles = event.command.member.get_roles();
    return std::find(roles.begin(), roles.end(), role_id) != roles.end();
}

int PermissionManager::discord_rank(DiscordLevel level) {
    switch (level) {
    case DiscordLevel::USER:
        return 0;
    case DiscordLevel::MODERATOR:
        return 1;
    case DiscordLevel::ADMIN:
        return 2;
    case DiscordLevel::OWNER:
        return 3;
    }

    return 0;
}

int PermissionManager::tournament_rank(TournamentRole role) {
    switch (role) {
    case TournamentRole::NONE:
        return 0;
    case TournamentRole::PLAYER:
        return 1;
    case TournamentRole::STAFF:
        return 2;
    case TournamentRole::ADMIN:
        return 3;
    }

    return 0;
}

dpp::snowflake PermissionManager::get_tournament_staff_role_id(dpp::snowflake guild_id) {
    GuildConfigManager::init();
    return GuildConfigManager::get_staff_role(guild_id);
}

dpp::snowflake PermissionManager::get_tournament_admin_role_id(dpp::snowflake guild_id) {
    GuildConfigManager::init();
    return GuildConfigManager::get_admin_role(guild_id);
}

// --- Tournament role logic ---
TournamentRole PermissionManager::get_tournament_role(const dpp::slashcommand_t& event) {
    dpp::snowflake guild_id = event.command.guild_id;

    // DM safety
    if (!guild_id) {
        return TournamentRole::NONE;
    }

    // Get configured roles from DB
    dpp::snowflake staff_role = get_tournament_staff_role_id(guild_id);
    dpp::snowflake admin_role = get_tournament_admin_role_id(guild_id);

    // Check roles (admin first = higher priority)
    if (admin_role && has_role(event, admin_role)) {
        return TournamentRole::ADMIN;
    }

    if (staff_role && has_role(event, staff_role)) {
        return TournamentRole::STAFF;
    }

    return TournamentRole::PLAYER;
}

bool PermissionManager::is_tournament_staff(const dpp::slashcommand_t& event) {
    return tournament_rank(get_tournament_role(event)) >= tournament_rank(TournamentRole::STAFF);
}

bool PermissionManager::is_tournament_admin(const dpp::slashcommand_t& event) {
    return tournament_rank(get_tournament_role(event)) >= tournament_rank(TournamentRole::ADMIN);
}

// --- Permission logic ---
bool PermissionManager::can_configure_tournament_roles(const dpp::slashcommand_t& event) {
    return is_admin(event);
}

bool PermissionManager::can_manage_tournament(const dpp::slashcommand_t& event) {
    return is_mod(event) || is_tournament_staff(event);
}

bool PermissionManager::can_admin_tournament(const dpp::slashcommand_t& event) {
    return is_admin(event) || is_tournament_admin(event);
}

bool PermissionManager::can_report_match(const dpp::slashcommand_t& event, bool is_override) {
    auto discord = get_discord_level(event);
    auto role = get_tournament_role(event);

    if (is_override) {
        // Mods OR tournament staff can override
        return discord_rank(discord) >= discord_rank(DiscordLevel::MODERATOR)
            || tournament_rank(role) >= tournament_rank(TournamentRole::STAFF);
    }

    // Normal reporting: players + staff allowed
    return tournament_rank(role) >= tournament_rank(TournamentRole::PLAYER);
}
