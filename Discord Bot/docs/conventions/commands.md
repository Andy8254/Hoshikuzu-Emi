# Command Conventions

## Command Shape

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

