# Misc Command Trust Model

Misc commands are split into isolated and trusted modules.

Use this split to keep playful or server-local features easy to add without letting them accidentally depend on tournament internals.

## Isolated Modules

Isolated modules are user-customizable features that do not need core tournament access.

Examples:

- YouTube link randomizer,
- mood or flavor text commands,
- server-local mini utilities,
- example extensions for documentation.

Allowed storage:

```text
BOT_USER_DB_PATH
db/user.db
```

Allowed dependencies:

- isolated misc SQLite wrapper,
- Discord command inputs,
- local validation helpers,
- feature-owned tables only.

Rules:

- Do not read or write core tournament tables.
- Do not add foreign keys to core tables.
- Do not assume a tournament, participant, profile, or match exists.
- Prefix tables with the feature name, such as `misc_youtube_randomizer_links`.
- Keep migrations idempotent.
- Keep commands disabled until explicitly registered and documented.

Typical env gate:

```text
BOT_ENABLE_MY_ISOLATED_FEATURE=false
```

## Trusted Modules

Trusted modules are extensions that may eventually need core bot behavior or tournament lifecycle access.

Examples:

- Mahjong tournament mode,
- ICPC-style scoreboard mode,
- custom tournament formats,
- automation that changes match state.

Allowed storage:

- none during scaffold phase,
- core DB only after design review,
- dedicated trusted tables only when schema ownership is clear.

Allowed dependencies:

- core tournament services,
- participant and match stores,
- standings/ruleset services,
- staff permission checks,
- audit/logging services.

Rules:

- Keep trusted modules env-gated by default.
- Start with domain contracts before commands.
- Add commands only after backend behavior is testable.
- Require staff-only permission checks for mutating commands.
- Record audit/log messages for tournament-impacting actions.
- Avoid mixing trusted and isolated tables.

Typical env gate:

```text
BOT_ENABLE_TRUSTED_MY_FEATURE=false
```

## Command Exposure

Adding a backend module is not the same as exposing a Discord command.

Recommended order:

1. Add domain contracts.
2. Add storage or pure backend helpers.
3. Add tests or reliability checks.
4. Add documentation.
5. Add command registration behind an env flag.
6. Run a canary trial.
7. Enable for stable only after staff approval.

## Current Modules

Trusted scaffold:

```text
BOT_ENABLE_TRUSTED_MAHJONG=false
include/misc/trusted/mahjong/Module.hpp
src/misc/trusted/mahjong/Module.cpp
```

Isolated scaffold:

```text
BOT_ENABLE_YOUTUBE_RANDOMIZER=false
include/misc/isolated/youtube_randomizer.hpp
src/misc/isolated/youtube_randomizer.cpp
```

Neither scaffold currently exposes slash commands.

## Quick Decision Rule

If the feature can be deleted without affecting tournament state, it is probably isolated.

If the feature changes registration, seeding, standings, match results, advancement, brackets, permissions, or official records, it is trusted.
