# Beta1 Code Stream Tree

This document explains how the bot works for someone who has never seen this codebase before.

Think of the bot as five layers:

```text
Discord
-> interaction router
-> command file
-> backend service
-> SQLite database or external API
-> Discord response and terminal log
```

Discord is the user interface. The backend decides the rules. SQLite stores the state.

## 1. Program Startup

When the bot starts, it enters through:

```text
src/commands/main.cpp
```

Startup has two possible paths:

```text
Program starts
|
+-- SQL reliability check mode
|   |
|   +-- Used by developers and CI-style checks
|   +-- Runs database and tournament safety tests
|   +-- Does not connect to Discord
|
+-- Normal Discord bot mode
    |
    +-- Load .env or selected env file
    +-- Read the bot token
    +-- Create the DPP Discord bot client
    +-- Register local command handlers
    +-- Register slash commands with Discord
    +-- Register button and modal handlers
    +-- Start listening for Discord events
```

Important files:

- `src/commands/main.cpp`: program entry point.
- `include/core/Config.hpp`: env-file loading and runtime settings.
- `src/core/SqlReliabilityCheck.cpp`: database and tournament reliability check mode.

## 2. Command Registration

The bot has to register commands in two places.

First, it registers C++ functions inside the bot. These are the functions that actually run when a command is used.

```text
register_general_commands()
register_fundamental_commands()
register_misc_commands()
register_player_commands()
register_tetrio_commands()
register_settings_commands()
register_moderation_commands()
register_tournament_commands()
```

Second, it tells Discord what slash commands should appear in the client.

```text
Discord command definitions
|
+-- /bot
+-- /profile
+-- /settings
+-- /mod
+-- /tournament
+-- other registered command groups
```

Important files:

- `src/core/CommandRegistry.cpp`: stores command-name to function mappings.
- `include/core/CommandRegistry.hpp`: command registry interface.
- `src/commands/CommandRegistration.cpp`: slash-command shapes sent to Discord.
- `src/commands/*.cpp`: command implementations.

## 3. Slash Command Flow

When a user runs a slash command, the bot does not jump directly into tournament code. It goes through a router first.

```text
User runs a slash command in Discord
|
+-- DPP receives the interaction
|
+-- src/interactions/InteractionHandlers.cpp
    |
    +-- Log that the command started
    +-- Read the command name
    +-- Ask CommandRegistry for the matching handler
    +-- Run the command handler
    +-- Send a Discord reply
    +-- Log success or failure in the terminal
```

Example:

```text
/tournament register
|
+-- InteractionHandlers.cpp
|
+-- CommandRegistry
|
+-- src/commands/Tournament.cpp
|
+-- src/tournament/registration.cpp
|
+-- src/core/sqlite.cpp
|
+-- Discord reply: registered, rejected, or error
```

Important files:

- `src/interactions/InteractionHandlers.cpp`: receives slash commands, buttons, and modal submissions.
- `src/commands/Tournament.cpp`: parses `/tournament` command options and calls tournament backend code.
- `src/commands/Player.cpp`: player profile commands.
- `src/commands/TETRIO.cpp`: TETR.IO profile commands.
- `src/commands/Settings.cpp`: server settings commands.
- `src/commands/Moderation.cpp`: moderation commands.
- `src/commands/Misc.cpp`: custom and example misc modules.

## 4. Button and Modal Flow

Some workflows use Discord buttons or forms instead of slash commands.

Buttons are used for actions such as match check-in, match reporting, and staff dashboard shortcuts.

Modals are Discord pop-up forms. They are used when the bot needs typed input.

```text
User clicks a button or submits a modal
|
+-- src/interactions/InteractionHandlers.cpp
    |
    +-- Read the custom_id
    +-- Identify which workflow owns it
    +-- Read any submitted fields
    +-- Call the matching backend function
    +-- Reply to the user
    +-- Optionally send a staff log message
```

The `custom_id` is the hidden identifier attached to a Discord button or modal. It tells the bot what the interaction means.

## 5. Core Backend Layer

The core layer contains shared systems that many commands use.

```text
Core backend
|
+-- SQLite database wrapper
+-- environment config
+-- command registry
+-- localization
+-- permission checks
+-- terminal logging
+-- HTTP fetcher
+-- sensitive text helpers
```

Important files:

- `src/core/sqlite.cpp`: shared database setup, player links, server settings, moderation records, and schema setup.
- `include/core/sqlite.hpp`: database classes and public database APIs.
- `include/core/Log.hpp`: structured terminal logs.
- `src/core/Localization.cpp`: text lookup for supported languages.
- `src/core/security/PermissionManager.cpp`: role and permission checks.
- `src/core/api_fetcher.cpp`: HTTP GET/POST helper used by services such as TETR.IO and Triangle automation.
- `src/core/SensitiveText.cpp`: helper for avoiding accidental sensitive output.

## 6. Tournament Backend Flow

The tournament module is the largest backend area. Its job is to store tournament state and enforce tournament rules.

```text
Tournament command
|
+-- command parser
|
+-- tournament backend
    |
    +-- tournament record
    +-- participant list
    +-- check-in state
    +-- seed order
    +-- ruleset
    +-- bracket matches
    +-- match reports
    +-- standings
    +-- SVG output
```

The normal tournament lifecycle is:

```text
Create tournament
|
+-- Open registration
|
+-- Players register
|
+-- Close registration
|
+-- Open check-in
|
+-- Players check in
|
+-- Close check-in
|
+-- Seed checked-in players
|
+-- Generate bracket
|
+-- Run and report matches
|
+-- Produce standings/SVG outputs
```

Important files:

- `src/tournament/manage.cpp`: create/edit/delete tournaments, open/close registration, open/close check-in, store tournament settings.
- `src/tournament/registration.cpp`: register players, unregister players, check players in, undo check-in, set participant state.
- `src/tournament/seeding.cpp`: seed players using general order, TETR.IO data, manual rating points, or CSV import.
- `src/tournament/ruleset.cpp`: match rules such as first-to, win-by, deuce, score cap, and draw policy.
- `src/tournament/bracket/BracketGenerator.cpp`: creates bracket structures in memory.
- `src/tournament/bracket/MatchStore.cpp`: saves matches, reports results, routes winners, handles forfeits, resolves no-shows, and calculates standings.
- `src/tournament/discord/MatchThreads.cpp`: creates Discord match threads and match buttons.
- `src/tournament/utility/BracketSvg.cpp`: creates bracket and match SVG files.

## 7. Match State Flow

A match usually moves through these states:

```text
pending
|
+-- both players are known
v
ready
|
+-- match starts or players check in
v
ongoing
|
+-- score is reported
v
completed
```

Important rule:

```text
Scores should normally be reported only when a match is ready or ongoing.
```

Different formats behave differently after a report:

```text
Single/double elimination
|
+-- winner advances
+-- loser may drop or move to lower bracket
+-- downstream matches can block later correction

Round robin
|
+-- result updates standings
+-- no player routing is needed

Swiss
|
+-- result updates standings
+-- next round is generated later from standings
+-- later rounds can block correction of earlier rounds
```

## 8. TETR.IO Data Flow

TETR.IO data is used for profile display, restrictions, and TETR.IO seeding.

```text
TETR.IO request
|
+-- TetrioService::fetch_user(username)
    |
    +-- Call TETR.IO user API
    +-- Parse account/profile data
    +-- Call TETR.IO league summary API
    +-- Parse rank, TR, APM, PPS, VS, and standings data
    +-- Return normalized profile data to the command or tournament workflow
```

Important files:

- `src/tetrio/TetrioService.cpp`: TETR.IO API calls and response parsing.
- `include/tetrio/TetrioService.hpp`: public TETR.IO profile structures.
- `src/tetrio/TetrioUtils.cpp`: display and helper utilities.
- `src/core/api_fetcher.cpp`: HTTP helper used by the service.

## 9. Manual Rating Flow

For games without automatic API-based rank data, staff can enter rank points manually.

Supported rating buckets include:

```text
TE:C
|
+-- Overall
+-- Connected VS
+-- Zone Battle
+-- Score Attack
+-- Classic Score Attack

PPT2
|
+-- Puzzle
+-- Puyo Puyo
+-- Tetris

General
|
+-- Single Rank Point
```

Flow:

```text
Staff enters rating points
|
+-- points are stored for a tournament participant
|
+-- seed command reads selected bucket
|
+-- players with valid points are sorted
|
+-- seed order is applied or exported for review
```

Main file:

- `src/tournament/seeding.cpp`

## 10. Triangle Room Automation Flow

Triangle automation is optional and disabled by default.

It is split into two parts:

```text
C++ bot
|
+-- decides whether automation is enabled
+-- sends room request to local bridge

Node bridge
|
+-- talks to Triangle.js
+-- controls TETR.IO room behavior
```

Full flow:

```text
Tournament match needs automated TETR.IO room
|
+-- C++ bot checks BOT_ENABLE_TETRIO_ROOM_AUTOMATION
|
+-- C++ bot sends request to TRIANGLE_BRIDGE_URL
|
+-- Node bridge creates room through Triangle.js
|
+-- Bridge invites both players
|
+-- Bridge waits grace time
|
+-- Bridge runs FT1 warm-up if enabled
|
+-- Bridge starts official match
|
+-- Bridge saves replay locally
|
+-- Bot receives room/replay status
```

Important files:

- `src/tournament/tetrio/triangle/RoomAutomation.cpp`: C++ side of Triangle integration.
- `include/tournament/tetrio/triangle/RoomAutomation.hpp`: C++ request/response structures.
- `tools/triangle-bridge/server.mjs`: local Node bridge that talks to Triangle.js.
- `docs/operations/tetrio_triangle_room_automation.md`: operations guide.
- `docs/third_party/triangle.md`: third-party notes.

## 11. Misc Extension Flow

Misc modules are example/custom extension modules.

There are two trust levels:

```text
Trusted module
|
+-- may use core tournament concepts
+-- suitable for controlled modules such as Mahjong scoreboard scaffolding

Isolated module
|
+-- uses separate user DB file
+-- should avoid touching core tournament/backend state
+-- suitable for examples such as a YouTube link randomizer
```

Flow:

```text
Bot starts
|
+-- src/commands/Misc.cpp
    |
    +-- initialize isolated user database
    |
    +-- if enabled, initialize isolated YouTube randomizer schema
    |
    +-- if enabled, initialize trusted Mahjong scaffold
```

Important files:

- `src/commands/Misc.cpp`: misc module startup.
- `src/misc/sqlite-user.cpp`: isolated user database.
- `src/misc/isolated/youtube_randomizer.cpp`: isolated example module.
- `src/misc/trusted/mahjong/Module.cpp`: trusted Mahjong scaffold.
- `docs/operations/misc_command_trust_model.md`: trusted vs isolated module guide.

## 12. Logging and Error Flow

Most backend actions now log to the terminal.

The log shape is:

```text
[timestamp][level][module] action detail
```

Examples:

```text
[2026-05-17 14:18:30][INFO][match-store] report_ok tournament_id=1 match_id=1 winner_id=9001
[2026-05-17 14:18:30][WARN][tournament-registration] operation_failed message="Registration is closed."
```

General behavior:

```text
Successful action
|
+-- Discord reply
+-- optional staff log message
+-- INFO terminal log

Expected rejection
|
+-- user-safe Discord reply
+-- WARN terminal log
+-- no state change where possible

Unexpected failure
|
+-- generic user-safe Discord reply
+-- ERROR terminal log
+-- transaction rollback if a DatabaseTransaction is active
```

Important files:

- `include/core/Log.hpp`: terminal logger.
- `docs/operations/log_capture.md`: how staff can copy or redirect terminal logs.

## 13. Database Safety Pattern

Most database functions follow this pattern:

```text
Open or reuse database
|
+-- prepare SQL statement
|
+-- bind values
|
+-- step statement
|
+-- read result or check changed row count
|
+-- finalize statement
|
+-- return success, rejection, or error
```

For multi-step changes, the bot uses a transaction:

```text
Start transaction
|
+-- perform several database changes
|
+-- if all succeed: commit
|
+-- if anything fails: return without commit
    |
    +-- transaction rolls back automatically
```

This matters for workflows such as bracket generation, match reporting, correction, and seed import.

## 14. Where to Start When Debugging

Use this table when you do not know where a bug lives.

| Symptom | Start Reading |
| --- | --- |
| Bot does not start | `src/commands/main.cpp`, `include/core/Config.hpp` |
| Slash command missing in Discord | `src/commands/CommandRegistration.cpp` |
| Slash command appears but does nothing | `src/interactions/InteractionHandlers.cpp`, `src/core/CommandRegistry.cpp` |
| Command option parsing is wrong | matching file in `src/commands/` |
| Tournament state is wrong | `src/tournament/manage.cpp` |
| Registration or check-in is wrong | `src/tournament/registration.cpp` |
| Seeding or CSV import is wrong | `src/tournament/seeding.cpp` |
| Bracket generation is wrong | `src/tournament/bracket/BracketGenerator.cpp` |
| Score reporting, forfeit, or no-show is wrong | `src/tournament/bracket/MatchStore.cpp` |
| TETR.IO profile or seed data is wrong | `src/tetrio/TetrioService.cpp` |
| Triangle room automation is wrong before bridge request | `src/tournament/tetrio/triangle/RoomAutomation.cpp` |
| Triangle room automation is wrong after bridge request | `tools/triangle-bridge/server.mjs` |
| Database schema or persistence is wrong | `src/core/sqlite.cpp` |
| Permission behavior is wrong | `src/core/security/PermissionManager.cpp` |
| Help text or language output is wrong | `src/core/Localization.cpp`, `resources/help/` |

## 15. Short Mental Model

If you remember only one thing, remember this:

```text
Discord command/button/modal
-> InteractionHandlers.cpp
-> CommandRegistry or custom_id router
-> src/commands/*.cpp
-> backend module
-> SQLite/API
-> Discord reply + terminal log
```

The command files should handle Discord-facing details. The backend modules should handle tournament rules and stored state.
