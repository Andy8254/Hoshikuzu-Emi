# Command Conventions

## Command Shape

Current grouped slash command surface:

- `/bot`: help, hello, ping, info, privacy
- `/profile`: player profile, linked accounts, personal language, TETR.IO lookup
- `/settings`: server settings
- `/mod`: moderation tools
- `/tournament`: tournament workflows, bracket operations, tournament config

Help files live under `resources/help/<language>/<module>/<command>.md`.
`EN-gb` is the fallback. `KO-kr` placeholder files may be empty; empty localized files intentionally fall back to `EN-gb`.

Use direct slash commands for:

- Staff/admin actions
- Debuggable operational paths
- Destructive actions with confirmation
- Workflows that must be auditable

Use buttons/select menus for:

- Player-facing match actions
- Registration/check-in panels
- Staff dashboard shortcuts
- Actions where IDs can be inferred from context

Registration/check-in panels should keep the player workflow short:

- Registration button opens a username modal.
- Check-in button reuses the registered username.
- Staff slash commands remain available for overrides and recovery.

Secondary display language is disabled by default. Use `/settings secondary_language language:KO-kr` to enable the Korean secondary display.

Player match screens should expose the same four actions everywhere:

- `Check in`
- `Report Score`
- `Forfeit`
- `Call Staff`

Report and forfeit actions should use modals. Forfeit must require explicit confirmation.

## Naming

Prefer:

- `snake_case` option names
- Clear verbs: `set`, `clear`, `show`, `generate`, `report`, `correct`
- Explicit destructive commands: `delete`, `clear`, `forfeit`

Avoid:

- Ambiguous `do`, `run`, `handle`
- Hidden destructive behavior
- Reusing one option name for different meanings in the same group

## Response Rules

Player-facing errors:

- Ephemeral

Private staff records:

- Ephemeral reply plus staff/mod log

Routine staff operational actions:

- Visible response or logged response, depending on channel

Destructive actions:

- Require confirmation keyword
- Log to configured audit channel

## Permission Layers

Keep these separate:

- Discord administrator/moderator permissions
- Server settings roles
- Tournament staff/admin roles
- Player/match participant status

Do not assume Discord admin and tournament admin are the same concept.

## Direct Commands vs UI

Embed UI should reduce friction, not remove direct command paths.

Direct staff commands should remain available for recovery and debugging.

When adding a new command, prefer fitting it into an existing group before creating a new top-level command. New top-level commands should represent a durable product area, not a single action.
