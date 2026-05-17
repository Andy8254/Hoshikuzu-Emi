# Trusted Mahjong Module Scaffold

This module is a trusted experimental scaffold, not a complete Mahjong tournament implementation.

For the general trusted/isolated command model, see:

```text
docs/operations/misc_command_trust_model.md
```

## Status

Default:

```text
BOT_ENABLE_TRUSTED_MAHJONG=false
```

Current code locations:

```text
include/misc/trusted/mahjong/Module.hpp
src/misc/trusted/mahjong/Module.cpp
```

The module currently exposes domain contracts only. It does not register slash commands, create tables in the core DB, generate live brackets, or mutate tournament state.

## Tournament Model

The intended Mahjong structure is:

```text
League qualifiers
-> cumulative standings
-> four-player playoff tables
-> top two players advance from each table
-> repeat until final table
```

Core constants:

```text
TABLE_SIZE=4
PLAYOFF_ADVANCERS=2
```

This is not compatible with the existing 1v1 match primitive without additional core work.

## Safe Next Steps

Implement in this order:

1. Define the exact scoring policy.
2. Add an idempotent trusted schema or reuse core tournament tables only after a design pass.
3. Add table-result validation.
4. Add league standings calculation.
5. Add playoff table seeding.
6. Add staff-only commands after the backend is testable.

## Required Decisions

Before implementation, decide:

- hanchan or tonpuusen,
- uma and oka,
- starting score,
- return score,
- tiebreakers,
- number of qualifier rounds,
- whether playoff score resets,
- semifinal and final table seeding,
- whether substitutes or no-shows are supported.

## Trust Boundary

This is a trusted module because it is expected to touch tournament lifecycle rules later. It may eventually need core DB access, bracket generation, standings, and staff result reporting.

Do not treat it as an isolated user extension.
