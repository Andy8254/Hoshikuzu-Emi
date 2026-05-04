# Beta1 Smoke Test Plan

Use Hami/canary for these tests. Keep Challonge or Battlefy as backup for any real event.

## Runtime Setup

Run from the inner project folder so relative resources and DB paths resolve:

```powershell
cd "C:\Users\User\source\repos\Discord Bot\Discord Bot"
$env:BOT_TOKEN="HAMI_CANARY_TOKEN"
& "..\x64\Debug\Discord Bot.exe"
```

Do not store real tokens in project files.

## 4-8 Player Dry Run

1. Create tournament
2. Set tournament channel
3. Set tournament log channel
4. Open registration
5. Register players
6. Open check-in
7. Check in players
8. Seed players
9. Generate single-elimination bracket
10. Create match threads
11. Use match check-in button
12. Report matches
13. Export SVG
14. Clear/reset test data

## Double-Elimination Dry Run

Repeat the 4-8 player flow with double elimination.

Extra checks:

- Winners bracket loser enters losers bracket
- Losers bracket loser is eliminated
- Grand finals can route into reset
- Correction is blocked after downstream completion

## No-Show Test

1. Create current match
2. Let one player check in
3. Wait or simulate grace expiry
4. Run `/tournament bracket resolve_no_shows`
5. Confirm checked-in player advances
6. Confirm tournament log output

## Both-Absent Test

1. Create match with seeded players
2. Neither player checks in
3. Run no-show resolver after grace
4. Confirm lower seed loses current match
5. Confirm upper seed is flagged for next-match auto-DQ

## Moderation Test

1. `/mod warn`
2. `/mod note`
3. `/mod history`
4. `/mod timeout`
5. `/mod clear_timeout`

Check:

- Replies are ephemeral where expected
- Cases are written to DB
- Modlog channel receives embeds
- No mass-mention abuse from reasons

## Help Test

Check:

- `/codex`
- `/codex category:tournament`
- `/codex category:brackets`
- `/codex category:moderation`
- `/codex category:settings`

## Pass Criteria

Beta-dev is not blocked if minor wording is awkward.

Beta-dev is blocked if:

- Staff cannot correct/report matches
- Double elimination routes incorrectly
- No-show resolver creates impossible bracket state
- Destructive commands bypass confirmation
- Moderation history leaks publicly
- Token or DB files are tracked

