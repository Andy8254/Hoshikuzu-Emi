# /tournament config
Tournament-specific configuration for channels, audit logging, roles, default formats, and match rulesets.

## Who Uses This
Tournament admins and trusted staff. These settings shape event operations.

## Workflow
Set panel/log channels and roles first, then format and rulesets before generating matches.

## Commands
- `roles`: Shows tournament role configuration.
- `set_staff_role`: Sets the tournament staff role.
- `set_admin_role`: Sets the tournament admin role.
- `clear_staff_role`: Clears the tournament staff role.
- `clear_admin_role`: Clears the tournament admin role.
- `set_channel`: Sets the tournament panel channel.
- `clear_channel`: Clears the tournament panel channel.
- `log_channel_assign`: Sets the tournament audit log channel.
- `log_channel_clear`: Clears the tournament audit log channel.
- `set_format`: Sets a tournament default format.
- `ruleset_show`: Shows tournament rulesets.
- `ruleset_set_primary`: Sets the primary match ruleset.
- `ruleset_set_secondary`: Sets secondary match rules.
- `ruleset_clear_secondary`: Disables secondary match rules.

## Help
Use `/bot help category:tournament_config command:<command>` for a command-specific page.
