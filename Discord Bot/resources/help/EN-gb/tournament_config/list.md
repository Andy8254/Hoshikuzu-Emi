# /tournament config
Tournament configuration tools for channels, audit logging, roles, default formats, and match rulesets.
## Who Uses This
Tournament admins and trusted staff can use these commands.
The commands below can affect how an event is operated, so use them deliberately.
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
- `log_channel_assign`: Sets the tournament log channel.
- `log_channel_clear`: Clears the tournament log channel.
- `set_format`: Sets a tournament default format.
- `ruleset_show`: Shows the tournament's overall rules.
- `ruleset_set_primary`: Sets the tournament's first ruleset, usually for pool matches.
- `ruleset_set_secondary`: Sets the tournament's second ruleset, usually for Top 8 or grand finals.
- `ruleset_clear_secondary`: Disables the tournament's second ruleset.
## Help
Use `/bot help category:tournament_config command:<command>` for a command-specific page.
