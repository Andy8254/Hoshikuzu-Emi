# /settings honeypot_set
Arms an automatic-ban honeypot channel.
## Usage
`/settings honeypot_set channel:<channel>`
## Details
When a non-exempt user sends any message in the selected channel, the bot deletes the message, bans the user, records an `auto_ban` moderation case, and posts the case to the moderation log channel if one is configured.
## Exemptions
Bots, the server owner, the bot developer, and users with the configured admin, moderator, or staff role are exempt.
## Notes
This command should only be used on a channel that normal users are not expected to use.
Use `/settings honeypot_show` after arming the channel.
