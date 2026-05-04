# Security and Operations Notes

## Token Handling

Never store bot tokens in:

- `.vcxproj.user`
- `Config.hpp`
- `.env` committed to Git
- README/docs
- screenshots
- Discord messages

Use runtime environment variables:

```powershell
cd "C:\Users\User\source\repos\Discord Bot\Discord Bot"
$env:BOT_TOKEN="HAMI_CANARY_TOKEN"
& "..\x64\Debug\Discord Bot.exe"
```

Rotate any token that has appeared in project files or screenshots.

## Stable vs Canary

Use separate:

- Bot token
- Discord application ID
- Database
- Guild command registration
- Test server scope

Suggested:

- Emi: stable/live bot
- Hami: canary/beta bot

## Runtime Files

Runtime DB files should not be tracked.

The root `.gitignore` should ignore:

- `**/db/*.db`
- `**/db/*.sqlite`
- `**/db/*.sqlite3`
- `**/db/*-wal`
- `**/db/*-shm`
- `*.env`

## Before Push Checklist

Run searches for:

- `BOT_TOKEN`
- Discord token-shaped strings
- `.vcxproj.user`
- `.env`
- `db/master.db`

## Current Known Security Posture

Already hardened in alpha2:

- Token removed from local project debug file
- Runtime DB files ignored
- Moderation warn/note/history replies made ephemeral
- Mass mentions neutralized in some logged user-supplied text
- Match check-in blocked after completion/no-show resolution

Still beta1 candidates:

- Central message sanitization helper
- Central audit log helper
- Role hierarchy checks for moderation live actions
- Separate DB path for canary/stable
- Data signature verification for external submissions

