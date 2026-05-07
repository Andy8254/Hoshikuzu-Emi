# Game Module Rating Guidelines

This document defines how tournament game modules should collect and use manual rating points when a platform does not have an automated rating API integration.

## Goals

- Keep registration simple: players provide their tournament username or linked account.
- Store rating points per tournament participant, not as a global player truth.
- Let each game module define the rating buckets that matter for seeding.
- Keep TETR.IO API-based seeding separate from manual rating-point seeding.
- Fall back to a single general rating point when no game-specific module exists.

## Core Terms

- **Game module**: A supported tournament platform or ruleset family, such as TETR.IO, TE:C, or PPT2.
- **Rating bucket**: A named rating category inside a game module. Examples include TE:C Connected VS or PPT2 Tetris.
- **Manual rating point**: A staff-entered or player-entered numeric value used for seeding.
- **Seed score**: The numeric value selected by the tournament staff for the active seeding mode.

## Recommended Storage Model

Manual ratings should be stored against the tournament participant record.

Required identity fields:

- `tournament_id`
- `discord_id`
- `game_type`
- `rating_bucket`
- `rating_points`

Recommended metadata:

- `source`: `player`, `staff`, `import`, or `api`
- `updated_by`
- `updated_at`
- `note`

The same player may have different values in different tournaments. This avoids treating a temporary tournament estimate as a permanent profile rating.

## Game-Specific Buckets

### TETR.IO

TETR.IO should use API-based league data where available.

Manual rating points should only be used as a fallback or staff override.

Primary automated fields:

- Tetra Rating
- Current rank
- Top rank
- Global rank

### Tetris Effect: Connected

TE:C should support these manual rating buckets:

- `tec_overall`: Overall rating across all four modes
- `tec_connected_vs`: Connected VS
- `tec_zone_battle`: Zone Battle
- `tec_score_attack`: Score Attack
- `tec_classic_score_attack`: Classic Score Attack

Default recommendation: use `tec_overall` unless the tournament ruleset is tied to one specific TE:C mode.

### Puyo Puyo Tetris 2

PPT2 should support these manual rating buckets:

- `ppt2_puzzle`: Puzzle League or mixed puzzle performance
- `ppt2_puyo_puyo`: Puyo Puyo
- `ppt2_tetris`: Tetris

Default recommendation: tournament staff should choose the bucket that matches the tournament ruleset. Do not mix Puyo and Tetris ratings unless the event format explicitly does so.

### General

General modules should support one manual bucket:

- `general_single_rank_point`: Single Rank Point

This is the fallback for games or formats without a dedicated module.

## Other Modes

Other game modes should follow `Creation Manual.md` once that document exists.

Until then, unsupported modes should use `general_single_rank_point`.

## Command Flow

Recommended staff command shape:

```text
/tournament rating set id:<tournament_id> user:<player> bucket:<rating_bucket> points:<number>
```

Recommended player command shape, if player entry is allowed:

```text
/tournament rating submit id:<tournament_id> bucket:<rating_bucket> points:<number>
```

Recommended seeding command shape:

```text
/tournament seed id:<tournament_id> mode:rating bucket:<rating_bucket>
```

## Validation Rules

- Rating points must be numeric.
- Negative values should be rejected unless a game module explicitly allows them.
- Staff-entered values should override player-submitted values.
- Player-submitted values may require staff confirmation for official events.
- Missing rating points should not silently become `0` unless staff explicitly chooses that policy.

## Seeding Rules

When using manual rating points:

1. Include checked-in and late checked-in players only.
2. Prefer players with a valid rating in the selected bucket.
3. Sort by rating points descending.
4. Break ties by existing manual seed, if present.
5. Break remaining ties by registration or check-in order.
6. Export the selected bucket and rating point in the seed CSV.

Players without the selected rating bucket should be either:

- excluded with a clear reason, or
- placed below rated players, if staff chooses a permissive policy.

The default should be exclusion for strict competitive events and permissive placement for casual events.

## Documentation Requirements For New Modules

Each game module should document:

- Supported rating buckets
- Default bucket
- Whether values are API-based, staff-entered, player-entered, or imported
- Valid rating range
- Tie-break policy
- Whether unrated players are allowed
- CSV export fields

## Current Planned Buckets

```text
tec_overall
tec_connected_vs
tec_zone_battle
tec_score_attack
tec_classic_score_attack
ppt2_puzzle
ppt2_puyo_puyo
ppt2_tetris
general_single_rank_point
```
