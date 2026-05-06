# Hami/Emi Bot Help
Tournament operations, player profiles, moderation tools, and server configuration are grouped into a small slash-command surface.

## Main Commands
- `/bot`: help, health checks, bot info, and privacy notice.
- `/profile`: player profile, linked accounts, personal language, and TETR.IO lookup.
- `/settings`: server roles, language settings, and moderation log routing.
- `/mod`: warnings, notes, history, timeouts, kicks, bans, and unbans.
- `/tournament`: tournament setup, registration, check-in, bracket operations, panels, and event configuration.

## Tournament Subgroups
- `/tournament bracket`: match generation, reports, corrections, no-shows, streams, standings, and SVG exports.
- `/tournament config`: event roles, channels, audit logs, format, and rulesets.

## Player Flow
Players normally use buttons where possible: registration panel, check-in panel, and match screen buttons. Slash commands remain available for staff recovery and audit-friendly operations.

## Language
Help pages are loaded from `resources/help/<language>/<module>/<command>.md`. Empty KO-kr pages intentionally fall back to this EN-gb text until translated.

## Examples
- `/bot help category:tournament`
- `/bot help category:tournament_bracket command:report`
- `/bot help category:settings command:secondary_language`
