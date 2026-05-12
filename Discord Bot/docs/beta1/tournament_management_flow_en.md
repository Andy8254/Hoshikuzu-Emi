# Beta1 Tournament Management Flow

This guide is for staff running a real beta1 tournament with the bot. Keep a conventional bracket platform or manual spreadsheet ready as the fallback until the beta has survived live events.

## Staff Roles

- Tournament admin: creates tournaments, sets roles/channels, configures rules, corrects reports, and approves emergency actions.
- Tournament staff: handles registration, check-in, seeding review, match threads, reports, forfeits, no-shows, and player support.
- Stream staff: uses stream assignment commands and watches current match state.
- Backup recorder: keeps an external copy of seeds, bracket state, and completed match results.

## Pre-Event Setup

### 1. Confirm Runtime

Before opening registration, confirm the intended bot build is online.

- Stable and canary should be separated by environment/token.
- For beta testing, prefer canary unless the event explicitly uses the stable bot.
- Confirm the bot can reply to `/codex ping` or `/codex info`.

If commands are missing in Discord, restart the bot and wait for guild command registration to finish.

### 2. Configure Staff Permissions

Run these once per server or before the event if roles changed:

```text
/tournament config set_staff_role role:<staff role>
/tournament config set_admin_role role:<admin role>
/tournament config roles
```

Expected result:

- Staff role can operate tournament workflows.
- Admin role can perform higher-risk actions such as report correction.
- `/tournament config roles` shows the expected roles.

### 3. Configure Channels

Set the tournament operation channel and audit log channel:

```text
/tournament config set_channel channel:<registration/check-in channel>
/tournament config log_channel_assign channel:<private staff log channel>
```

Use the log channel to verify registrations, check-ins, reports, forfeits, no-show resolution, and staff calls.

### 4. Create Tournament

Create the tournament with the intended game and format:

```text
/tournament create name:<event name> game:<game> format:<format>
```

Then inspect it:

```text
/tournament info id:<id>
/tournament staff_info id:<id>
```

Record the tournament ID in a staff note. Most commands require it.

### 5. Configure Game-Specific Restrictions or Ratings

For TETR.IO events, set restrictions if needed:

```text
/tournament config tetrio_restrictions id:<id> current_rank_min:<rank> current_rank_max:<rank> tr_min:<value> tr_max:<value> allow_unranked:<true|false>
```

For TE:C or PPT2 manual rating events, enter rating points before seeding:

```text
/tournament config rating_set id:<id> user:<player> bucket:<bucket> points:<points> note:<optional note>
/tournament config rating_list id:<id> bucket:<bucket>
```

Default manual rating buckets:

- TE:C: overall, connected VS, zone battle, score attack, classical score attack.
- PPT2: puzzle, puyo puyo, tetris.
- General: single rank point.

### 6. Configure Match Rules

Check current rules:

```text
/tournament config ruleset_show id:<id>
```

Set primary rules:

```text
/tournament config ruleset_set_primary id:<id> first_to:<score> deuce:<mode> win_by:<diff> score_cap:<cap> allow_draw:<true|false>
```

Set secondary rules for top 8 or grand finals if needed:

```text
/tournament config ruleset_set_secondary id:<id> trigger:<top8|grand_finals> first_to:<score> deuce:<mode> win_by:<diff> score_cap:<cap> allow_draw:<true|false>
```

Use `score_cap:0` for no cap.

## Registration Flow

### 1. Open Registration

```text
/tournament registration_open id:<id>
```

Players register themselves:

```text
/tournament register id:<id> username:<username>
```

Staff can register a player manually:

```text
/tournament register id:<id> user:<player> username:<username>
```

Players or staff can unregister with `abort:true`.

### 2. Monitor Entrants

Use:

```text
/tournament participants id:<id>
/tournament staff_info id:<id>
```

Check for duplicate usernames, missing usernames, wrong game accounts, and obvious rating mistakes.

### 3. Close Registration

```text
/tournament registration_close id:<id>
```

After closing, do not reopen casually. If late registration is allowed, record the staff decision in the log.

## Check-In Flow

### 1. Open Check-In

Use a Unix timestamp for the close time:

```text
/tournament checkin_open id:<id> closes_at:<unix timestamp> grace_time:<seconds>
```

Players check in:

```text
/tournament checkin id:<id> username:<username>
```

Staff can check a player in manually:

```text
/tournament checkin id:<id> user:<player> username:<username>
```

Players or staff can undo check-in with `abort:true`.

### 2. Close Check-In

```text
/tournament checkin_close id:<id>
```

Then inspect:

```text
/tournament participants id:<id>
/tournament staff_info id:<id>
```

Only checked-in players are seeded.

## Seeding Flow

### Fast Path

For ordinary events:

```text
/tournament seed id:<id> mode:<general|tetrio|rating> bucket:<bucket if rating>
```

This applies seed order immediately.

### Human-Reviewed CSV Path

Recommended for real beta1 tournaments:

```text
/tournament seed_export id:<id> mode:<general|tetrio|rating> bucket:<bucket if rating>
```

Staff workflow:

1. Download the CSV.
2. Reorder player rows.
3. Keep usernames unchanged.
4. Upload the reordered CSV:

```text
/tournament seed_import id:<id> file:<csv attachment>
```

Import rules:

- Row order becomes seed order.
- Full export CSVs are accepted.
- Username-only CSVs are accepted.
- The username set must exactly match checked-in participants.
- If validation fails, no seed changes are applied.

After import:

```text
/tournament participants id:<id>
```

## Bracket Flow

### 1. Generate Bracket

```text
/tournament bracket generate id:<id> type:<optional format>
```

Generate only after seeding is final.

### 2. Inspect Current Matches

```text
/tournament bracket current id:<id>
/tournament bracket round id:<id> round:<round number>
/tournament bracket match id:<id> match_id:<match id>
```

### 3. Create Match Threads

```text
/tournament bracket threads id:<id> buttons:<true|false>
```

For a specific round:

```text
/tournament bracket threads id:<id> round:<round number> buttons:<true|false>
```

Use buttons when players should report or check in through match thread UI.

### 4. Report Matches

```text
/tournament bracket report id:<id> match_id:<match id> score_a:<score> score_b:<score>
```

If a completed result is wrong and downstream state allows correction:

```text
/tournament bracket correct_report id:<id> match_id:<match id> score_a:<score> score_b:<score> confirm:CORRECT
```

Use correction only with admin approval.

### 5. Handle Forfeits and No-Shows

For a direct forfeit or DQ:

```text
/tournament bracket forfeit id:<id> match_id:<match id> player:<player> reason:<reason>
```

For due no-show states:

```text
/tournament bracket resolve_no_shows id:<id>
```

Check the log channel after each no-show resolution.

### 6. Stream Assignments

```text
/tournament bracket stream_assign id:<id> match_id:<match id>
/tournament bracket stream_list id:<id>
/tournament bracket stream_clear id:<id> match_id:<match id>
```

### 7. Public Outputs

```text
/tournament bracket standings id:<id>
/tournament bracket svg id:<id>
/tournament bracket match_svg id:<id> match_id:<match id>
```

Use SVG exports for staff review, announcements, or stream assets.

## Live Event Checklist

Before bracket start:

- Bot build and token confirmed.
- Staff/admin roles confirmed.
- Tournament and log channels configured.
- Tournament ID pinned in staff channel.
- Registration closed.
- Check-in closed.
- Manual ratings complete, if used.
- Seeds exported and backed up.
- Human-reviewed seed CSV imported, if used.
- Bracket generated after final seeds.
- Current matches and match threads verified.
- External fallback bracket or spreadsheet ready.

During bracket:

- Report only verified scores.
- Use `current`, `round`, and `match` before resolving disputes.
- Record manual decisions in staff chat.
- Use `call_staff` when players need help from a match.
- Use corrections sparingly and immediately verify bracket state afterward.
- Export SVG snapshots at major milestones.

After event:

- Export final bracket SVG.
- Save final standings or result message.
- Record unresolved issues for beta feedback.
- Do not run `/tournament clear` unless the event data is no longer needed.

## Troubleshooting

### Slash Commands Are Missing

Likely causes:

- Bot was not restarted after command changes.
- Guild command registration has not completed.
- Wrong bot build or token is online.

Actions:

1. Confirm stable/canary identity.
2. Restart the intended bot.
3. Check `/codex ping`.
4. Wait briefly for Discord command refresh.

### Staff Cannot Use Tournament Commands

Likely causes:

- Tournament staff role is not configured.
- User does not have the staff/admin role.
- Discord role hierarchy or permissions changed.

Actions:

```text
/tournament config roles
/tournament config set_staff_role role:<staff role>
/tournament config set_admin_role role:<admin role>
```

Then ask the user to retry the command.

### Tournament Not Found

Likely causes:

- Wrong tournament ID.
- Tournament was deleted.
- Data was cleared.
- Staff is using an ID from another test run.

Actions:

1. Check the pinned tournament ID.
2. Use `/tournament staff_info id:<id>`.
3. If not found, create a new tournament or switch to backup records.

### Player Cannot Register

Likely causes:

- Registration is not open.
- Player is already registered.
- Username is missing or invalid for the workflow.
- TETR.IO restrictions reject the player.

Actions:

1. Check `/tournament info id:<id>`.
2. Check `/tournament participants id:<id>`.
3. If staff approves, register manually with `/tournament register id:<id> user:<player> username:<username>`.

### Player Cannot Check In

Likely causes:

- Check-in is not open.
- Player is not registered.
- Check-in window closed.
- Username differs from registration.

Actions:

1. Check `/tournament participants id:<id>`.
2. If staff approves, check in manually with `/tournament checkin id:<id> user:<player> username:<username>`.
3. If the player should not participate, leave them unchecked.

### TETR.IO Profile or Seeding Data Looks Wrong

Likely causes:

- TETR.IO API returned incomplete data.
- Player is unranked.
- Username was mistyped.
- Restrictions exclude the player.

Actions:

1. Verify with `/profile tetrio username:<name>`.
2. Recheck restrictions with `/tournament config tetrio_restrictions`.
3. Use CSV seeding path if staff wants final discretion.
4. For non-TETR.IO events, use manual rating buckets instead.

### Manual Rating Seeding Excludes Players

Likely causes:

- Missing rating points for the selected bucket.
- Wrong bucket for the game type.
- Player was not checked in.

Actions:

```text
/tournament config rating_list id:<id> bucket:<bucket>
/tournament config rating_set id:<id> user:<player> bucket:<bucket> points:<points>
/tournament participants id:<id>
```

Then run `seed_export` or `seed` again.

### Seed Import Fails

Likely causes:

- CSV has a username that is not checked in.
- CSV is missing a checked-in player.
- CSV has duplicate usernames.
- Staff edited usernames instead of only moving rows.
- Wrong tournament ID.

Actions:

1. Export a fresh CSV with `/tournament seed_export`.
2. Reorder rows only.
3. Do not rename usernames.
4. Import again.

Username-only CSVs are accepted, but the username list must exactly match checked-in participants.

### Bracket Generation Fails or Looks Wrong

Likely causes:

- No seeds have been applied.
- Too few checked-in players.
- Wrong format selected.
- Staff generated bracket before final seed import.

Actions:

1. Check participants and seeds.
2. Re-run seed export/import if needed.
3. Generate bracket only after seed order is final.
4. If already generated incorrectly during a real event, switch to the backup bracket unless staff confirms it is safe to recreate.

### Match Report Fails

Likely causes:

- Match is not current or reportable.
- Match ID is wrong.
- Score violates ruleset.
- Match already completed.

Actions:

```text
/tournament bracket current id:<id>
/tournament bracket match id:<id> match_id:<match id>
/tournament config ruleset_show id:<id>
```

If the score is valid and the bot still rejects it, record the result externally and continue with staff escalation.

### Wrong Score Was Reported

Actions:

1. Stop downstream reporting for affected matches.
2. Confirm the correct score with both players or staff evidence.
3. Use:

```text
/tournament bracket correct_report id:<id> match_id:<match id> score_a:<score> score_b:<score> confirm:CORRECT
```

4. Verify with `/tournament bracket current` and `/tournament bracket match`.

If correction fails because downstream matches already depend on the result, preserve the bot state and move to backup operations.

### No-Show Resolution Seems Wrong

Actions:

1. Check match thread messages and button state.
2. Check grace time.
3. Run `/tournament bracket match id:<id> match_id:<match id>`.
4. Use `/tournament bracket resolve_no_shows id:<id>` only when the state is due.
5. If the result is disputed, pause the match group and use staff judgment.

### Match Threads Are Missing

Likely causes:

- Bracket is not generated.
- There are no current matches.
- Bot lacks thread permissions.
- Threads were already created elsewhere.

Actions:

```text
/tournament bracket current id:<id>
/tournament bracket threads id:<id> buttons:true
```

Check Discord permissions if no threads appear.

### SVG Export Fails

Likely causes:

- Bracket does not exist.
- Match ID is wrong.
- SVG generator hit an unexpected bracket state.

Actions:

1. Check `/tournament bracket current`.
2. Try `/tournament bracket svg id:<id>`.
3. For one match, verify the match ID before `/tournament bracket match_svg`.
4. Use text standings or backup bracket if SVG remains unavailable.

### Bot Is Slow or Unresponsive

Actions:

1. Avoid repeating the same command rapidly.
2. Check whether Discord itself is delayed.
3. Confirm the bot process is still running.
4. Keep recording results externally while waiting.
5. If the bot recovers, reconcile one match at a time.

### Emergency Fallback

Switch to backup operation when:

- Commands disappear during bracket play.
- Bracket state becomes impossible.
- Corrections cannot safely repair a wrong result.
- The bot cannot report multiple completed matches.
- Staff cannot validate seed or participant state.

Fallback steps:

1. Announce staff-controlled fallback in the staff channel.
2. Export or screenshot current bot state if possible.
3. Continue event in the backup bracket or spreadsheet.
4. Stop using bot commands that mutate state.
5. Keep logs for post-event beta debugging.

## Post-Event Report Template

Use this structure after the event:

```text
Event:
Date:
Bot build:
Stable or canary:
Tournament ID:
Format:
Entrants:
Checked-in players:
Seeding mode:
Bracket generated at:
Issues:
Manual interventions:
Fallback used:
Commands that failed:
Expected behavior:
Actual behavior:
Screenshots/log links:
```
