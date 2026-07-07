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

The bot also rejects empty tokens and tokens that look too short. This is only a sanity check; it does not prove the token is valid.

## Sensitive Text Encryption

Set `BOT_DATA_KEY` to enable AES-GCM encryption for newly stored moderation reasons and staff notes:

```powershell
$env:BOT_DATA_KEY="<at least 32 random characters>"
```

When `BOT_DATA_KEY` is present, `moderation_cases.reason` is stored as `enc:v1:<nonce>:<ciphertext>:<tag>` and decrypted when staff reads moderation history. Existing plaintext rows still display normally.

Keep this key outside git. If the key is lost, encrypted moderation reasons cannot be recovered from the DB.

Generate a local test key with PowerShell:

```powershell
$bytes = New-Object byte[] 32
[System.Security.Cryptography.RandomNumberGenerator]::Fill($bytes)
$env:BOT_DATA_KEY = [Convert]::ToBase64String($bytes)
```

For a real test event, store the generated value somewhere private before starting the bot.

## Moderation Honeypot

The bot can arm one automatic-ban honeypot channel per server:

```text
/settings honeypot_set channel:#channel
/settings honeypot_show
/settings honeypot_clear
```

When a non-exempt user sends any message in the configured channel, the bot:

- deletes the triggering message,
- bans the user,
- creates a moderation case with action `auto_ban`,
- posts the case to the configured moderation log channel, if one exists.

Exempt users:

- bots,
- the server owner,
- the bot developer,
- configured admin, moderator, and staff roles.

Operational guidance:

- Keep the honeypot channel private or clearly outside normal user workflows.
- Use `/settings honeypot_show` after permission changes.
- Configure `/settings modlog_set` before arming the honeypot.
- Make sure the bot role can delete messages and ban users below its role.
- Use `/mod unban` for false positives.

Current failure behavior is intentionally minimal: failed bans are not yet mirrored to a fallback audit channel, and failed message deletion or modlog posting may be silent. Treat this as a beta hardening candidate before broad deployment.

## Stable vs Canary

Use separate:

- Bot token
- Discord application ID
- Database
- Data encryption key, if `BOT_DATA_KEY` is used
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
- Optional AES-GCM encryption for newly stored moderation reasons/notes through `BOT_DATA_KEY`
- Moderation warn/note/history replies made ephemeral
- Mass mentions neutralized in some logged user-supplied text
- Match check-in blocked after completion/no-show resolution

Still beta1 candidates:

- Central message sanitization helper
- Central audit log helper
- Role hierarchy checks for moderation live actions
- Rich error logging for automated honeypot delete/ban/modlog failures
- Separate DB path for canary/stable
- Data signature verification for external submissions
