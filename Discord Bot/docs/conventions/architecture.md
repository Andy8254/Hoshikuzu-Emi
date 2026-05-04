# Architecture Notes

## Main Principle

Do not let Discord slash-command shape become the architecture.

Slash commands are one interface. Later interfaces may include:

- Embed dashboards
- Web manager
- CSV import/export
- Canary bot
- External bracket sync

All interfaces should call shared backend/domain services.

## Current Module Shape

Existing module areas:

- `src/commands`
- `src/core`
- `src/tournament`
- `src/tournament/bracket`
- `src/tournament/discord`
- `src/tournament/utility`

This is a good alpha2 shape, but beta1 should move more orchestration out of command files.

## Candidate Beta1 Services

- `ModerationPolicy`
- `TournamentPolicy`
- `MatchLifecycleService`
- `NoShowResolver`
- `AuditLogService`
- `PermissionService`
- `LocalizationService`
- `FlavourTextService`

## Workflow Reading Pattern

Trace workflows as:

```text
Discord command/button
-> handler
-> service/domain function
-> SQLite/store mutation
-> Discord response/log
```

Good first workflows to understand:

- Player registration
- Tournament check-in
- Bracket generation
- Match reporting
- No-show resolution
- Moderation warning/history

