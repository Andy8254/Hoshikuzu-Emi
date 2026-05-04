# Policy Alignment Notes

These notes compare the current alpha2 bot behavior with the Stacking Arena moderation and tournament policy draft.

## Moderation Levels

Policy defines:

- Level 1: Soft warning, not recorded in infraction log
- Level 2: Formal warning, recorded
- Level 3: Time-out, 1 hour to 7 days depending on severity
- Level 4: Indefinite ban, appealable
- Level 5: Permanent ban, non-appealable

Current implementation:

- `/mod warn`
- `/mod note`
- `/mod history`
- `/mod timeout`
- `/mod clear_timeout`
- `/mod kick`
- `/mod ban`
- `/mod unban`

Gaps:

- No moderation level field
- No soft-warning path that avoids infraction logging
- No appealability flag
- No appeal status
- No distinction between indefinite and permanent ban

Suggested beta1 changes:

- Add `level` to moderation cases
- Add `appealable` and `appeal_status`
- Add `/mod soft_warn` or `/mod warn level:1|2`
- Ensure Level 1 does not create a formal infraction record

## Tournament Absence and Intentional Delay

Policy requires:

- Staff confirmation of no response
- 10-minute grace period after staff confirmation
- If one player is absent, soft warning + instant DQ
- If both players intentionally delay, lower seed gets soft warning + DQ
- Upper seed receives an additional 5-minute staff-message window
- Upper seed receives formal warning + DQ if still absent

Current implementation:

- Match thread opening stores `match_opened_at`
- Match check-in button stores per-match check-in flags
- `/tournament bracket resolve_no_shows` resolves due states
- If one player checked in, absent player loses
- If neither checked in, lower seed loses now and upper seed is flagged for auto-DQ in their next match

Gaps:

- Grace time starts from match thread open, not staff-confirmed absence
- No explicit staff confirmation timestamp
- No extra 5-minute upper-seed timer
- No soft/formal moderation case creation tied to tournament DQ
- No objection workflow

Suggested beta1 changes:

- Add absence confirmation command
- Store `absence_confirmed_at`
- Store `upper_seed_notice_at`
- Resolve lower-seed and upper-seed sanctions according to policy windows
- Create linked moderation cases when required

## Resignation

Policy requires players to state whether they continue in losers bracket for double-elimination events.

Current implementation:

- `/tournament bracket forfeit` makes the player lose a match through normal bracket routing

Gaps:

- No tournament resignation command
- No drop-from-event state that differs from match forfeit
- No `continue_losers` choice

Suggested beta1 command:

`/tournament resign id:<id> player:<user> continue_losers:<true_or_false> reason:<text>`

## Objections and Appeals

Policy recognizes objection conditions:

- Policy exploitation
- Arrival in time
- Excuses informed

Current implementation:

- `/tournament call_staff`
- Match correction
- Manual moderation tools

Gaps:

- No objection record
- No objection category
- No objection resolution state

Suggested beta1 command:

`/tournament objection id:<id> match_id:<match_id> type:<type> details:<text>`

