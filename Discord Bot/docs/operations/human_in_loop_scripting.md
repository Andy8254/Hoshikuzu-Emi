# Human-in-the-Loop Scripting Guidelines

Use these guidelines when investigating backend anomalies with helper scripts, SQL snippets, or one-off repair tools.

The goal is not full automation. The goal is controlled inspection, clear checkpoints, and small reversible actions.

## When To Use This

Use human-in-the-loop scripting when you see:

- impossible match state
- wrong Swiss/RR standings
- player routed to the wrong downstream match
- report/correction failing unexpectedly
- missing or duplicated tournament rows
- language/config values not resolving as expected
- startup/deployment DB path confusion

Do not use scripts as the first response to normal command mistakes. Prefer bot commands when they are sufficient.

## Operating Principle

Every backend script should follow this shape:

```text
inspect
explain
checkpoint
change
verify
record
```

Never jump directly from anomaly to mutation.

## Before Running A Script

Confirm these facts:

- Which DB file is active.
- Whether `BOT_DB_PATH` is set.
- Which tournament ID is affected.
- Whether the bot is currently running.
- Whether the event is live or a test/canary.
- What exact symptom was observed.

Recommended PowerShell checks:

```powershell
Get-Location
$env:BOT_DB_PATH
Get-ChildItem db
```

If the bot is running during a live event, prefer read-only inspection first.

## Read-Only First

Start with SELECT queries or scripts that only print state.

Useful tables:

- `tournaments`
- `tournament_participants`
- `tournament_matches`
- `tournament_rulesets`
- `guild_config`
- `server_settings`
- `user_settings`
- `moderation_cases`

Useful match fields:

- `id`
- `bracket_match_index`
- `round`
- `position`
- `bracket`
- `player_a_id`
- `player_b_id`
- `winner_id`
- `state`
- `next_winner_match`
- `next_winner_slot`
- `next_loser_match`
- `next_loser_slot`
- `pending_auto_dq_player_id`

## Checkpoint Before Mutating

Before UPDATE, DELETE, INSERT, or schema changes, create a DB copy.

For PowerShell:

```powershell
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
Copy-Item "db\master.db" "db\master.before-fix-$stamp.db"
```

If using `BOT_DB_PATH`, copy that file instead of `db/master.db`.

Do not mutate a live event DB without a backup.

## Mutation Rules

Prefer the narrowest possible change:

- One tournament at a time.
- One match at a time.
- One participant at a time.
- Explicit `WHERE tournament_id = ?`.
- Explicit `WHERE id = ?` for match-level fixes.

Avoid broad statements:

```sql
UPDATE tournament_matches SET state = 'ready';
DELETE FROM tournament_matches;
```

Use a transaction for every repair:

```sql
BEGIN TRANSACTION;
-- narrow changes here
COMMIT;
```

Rollback if verification fails.

## What Scripts Should Not Do

Scripts should not:

- invent missing players
- guess a Discord ID
- silently delete tournament data
- bypass confirmation keywords for destructive actions
- change bot tokens
- rewrite schema without a backup
- repair Swiss pairings after later rounds exist without explicit human approval
- auto-resolve no-shows for Swiss/RR unless that policy is implemented later

## Match Format Rules

Elimination:

- `winners`, `losers`, `grand_finals`
- reports route players through destination fields
- downstream completed matches lock correction
- due no-show resolver applies here

Round robin:

- `round_robin`
- reports affect only local match result and standings
- no downstream routing

Swiss:

- `swiss`
- reports affect standings
- next round is generated later
- earlier rounds should not be corrected after later Swiss rounds exist

Swiss bye:

- `swiss_bye`
- auto-completed
- contributes to standings
- not manually reportable

## Anomaly Triage Checklist

For match report issues:

1. Check the match exists.
2. Check `state` is `ready` or `ongoing`.
3. Check both player IDs are present.
4. Check bracket type.
5. For elimination, check destination fields.
6. For Swiss, check whether a later Swiss round exists.

For standings issues:

1. List completed `swiss`, `swiss_bye`, and `round_robin` matches.
2. Check `winner_id`.
3. Check byes are recorded as completed.
4. Check the participant is still checked in.

For no-show issues:

1. Confirm bracket is elimination.
2. Check `match_opened_at`.
3. Check `grace_time`.
4. Check player check-in flags.
5. Check `pending_auto_dq_player_id`.

## Script Output

Every script should print:

- DB path used
- tournament ID
- read-only or mutating mode
- rows inspected
- rows changed
- follow-up verification query result

If a script changes data, save the output or paste it into the dev notes.

## Good Repair Script Shape

```text
parse args
open db
print db path
SELECT current state
ask human to confirm exact change
backup db
BEGIN TRANSACTION
UPDATE narrow target
SELECT verify target
COMMIT or ROLLBACK
print summary
```

## Current Safe Default

When unsure, stop after inspection and ask for review.

Backend scripting should make anomalies easier to understand, not easier to hide.
