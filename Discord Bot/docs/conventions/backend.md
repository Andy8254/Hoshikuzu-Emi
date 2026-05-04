# Backend Reading Map

This bot is plain C++ at the core, but three things make it look heavier than it is:

- DPP types wrap Discord events, users, channels, embeds, and replies.
- SQLite C APIs use prepared statements, manual binding, and row reads.
- Tournament formats have different match lifecycles.

Read backend code by following data ownership first, not command names.

## Main Flow

```text
Discord interaction
-> src/commands handler
-> tournament/core backend API
-> SQLite mutation/query
-> command response or Discord-side helper
```

The command layer should decide permissions, option parsing, and response style. Backend modules should decide tournament state and data rules.

## Core Files

- `include/core/sqlite.hpp`
  Shared SQLite wrapper, schema helpers, and transaction guard.

- `src/core/sqlite.cpp`
  DB connection setup, player links, guild/server settings, user language settings, and moderation cases.

- `src/tournament/manage.cpp`
  Tournament records: create, update, delete, status, registration/check-in flags.

- `src/tournament/registration.cpp`
  Tournament participant records: register, check in, seed, status.

- `src/tournament/ruleset.cpp`
  Match ruleset storage: primary/secondary first-to, deuce, win-by, score cap.

- `src/tournament/bracket/BracketGenerator.cpp`
  In-memory match generation for single elimination, double elimination, and round robin.

- `src/tournament/bracket/MatchStore.cpp`
  Persistent match storage and match lifecycle behavior.

## SQLite Pattern

Most write/read functions follow this shape:

```text
init()
prepare SQL
bind values
step statement
read result or check changes
finalize statement
return domain result
```

The project uses `DatabaseTransaction` for multi-step mutations. If a function returns before `commit()`, the transaction rolls back automatically.

## Match Types

Stored matches use the `bracket` string to identify format behavior:

- `winners`, `losers`, `grand_finals`
  Elimination brackets. Reports route winners and losers into destination matches.

- `round_robin`
  Standings format. Reports complete only that match; there is no downstream routing.

- `swiss`
  Standings format. Reports complete only that match. The next round is generated later from standings.

- `swiss_bye`
  Auto-completed bye record. It contributes to standings and is not reportable.

## Match Lifecycle

Persistent match states are:

- `pending`: waiting for one or both players.
- `ready`: both players are known and the match can be played/reported.
- `ongoing`: match interaction has started, commonly after check-in.
- `completed`: result is locked unless correction rules allow reset.

Reporting should only happen for `ready` or `ongoing` matches.

## Format Behavior

Important behavior split:

- Elimination supports routing, no-show auto-resolution, and downstream correction locks.
- Swiss supports standings and round appending, but correction locks after a later Swiss round exists.
- Round robin supports standings and local correction.
- Byes are completed records and cannot be manually reported.

## Deployment Notes

Default DB path is `db/master.db`.

For canary/stable separation, set:

```powershell
$env:BOT_DB_PATH = "db/hami-canary.db"
```

All shared `Database("db/master.db")` users honour that override.
