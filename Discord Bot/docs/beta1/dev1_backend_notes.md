# Dev1 Backend Optimisation Notes

Dev1 moved the backend from alpha-style "it works" toward beta-style "it is understandable and recoverable".

## Completed

- Added `BOT_DB_PATH` support for canary/stable DB separation.
- Added SQLite pragmas for low-maintenance hosting:
  - `foreign_keys = ON`
  - `journal_mode = WAL`
  - `synchronous = NORMAL`
  - `temp_store = MEMORY`
- Added schema helpers:
  - `table_has_column`
  - `add_column_if_missing`
  - `create_index_if_missing`
  - `schema_meta` version storage
- Added hot-query indexes for tournaments, participants, rulesets, matches, moderation cases, and language settings.
- Replaced private tournament SQLite wrapper classes with the shared `Database` wrapper.
- Added `DatabaseTransaction` so multi-step mutations roll back automatically unless committed.
- Scoped match behavior by format:
  - elimination routes winners/losers
  - Swiss and round robin update standings only
  - Swiss byes are not reportable
  - due no-show auto-resolution is elimination-only for now

## How To Read MatchStore

Read `MatchStore.cpp` in this order:

1. SQLite helpers and row mapping.
2. Destination helpers for placing winners/losers.
3. Match generation persistence.
4. Format/lifecycle helper functions.
5. Public list/query functions.
6. Check-in, forfeit, no-show, report, correction.
7. String formatting helpers.

The central idea is:

```text
generation creates matches
report completes matches
elimination report also routes players
Swiss/RR report only affects standings
correction resets the local match and then re-reports
```

## Current Limits

- `MatchStore.cpp` is still large. It is safer now, but later it should split into persistence, generation, reporting, no-show, and standings files.
- Schema versioning exists but is still basic. Future migrations should use explicit numbered migration steps.
- There is no standalone backend smoke-test harness yet.
- Swiss round count, top-cut handoff, and tiebreakers remain later Beta1 work.

## Suggested Next Read

- `docs/conventions/backend.md`
- `src/tournament/bracket/MatchStore.cpp`
- `src/tournament/bracket/BracketGenerator.cpp`
- `src/tournament/manage.cpp`
- `src/tournament/registration.cpp`
