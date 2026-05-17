# Log Capture

The bot writes structured operational logs to the terminal by default.

This is intentional. Persistent log files are disabled by design for normal beta and local tournament runs because they add disk cleanup, rotation, privacy, and storage-management work that the bot does not need yet.

## Where Logs Appear

Core bot logs appear in the terminal that runs:

```powershell
& ".\x64\Debug\Discord Bot.exe"
```

Triangle bridge logs appear in the terminal that runs the bridge service.

## Normal Staff Workflow

For most issues, copy the relevant terminal output directly from Windows Terminal or PowerShell.

Useful details to include when reporting an issue:

- command or button used
- tournament ID
- match ID, if relevant
- Discord user ID, if relevant
- the first warning or error line
- the lines immediately before and after the warning or error

## One-Off File Capture

If a tournament night needs persistent capture, redirect the process output from outside the bot:

```powershell
& ".\x64\Debug\Discord Bot.exe" *> bot-session.log
```

For separate stable and canary runs:

```powershell
$env:BOT_ENV_FILE="stable.env"
& ".\x64\Debug\Discord Bot.exe" *> stable-session.log
```

```powershell
$env:BOT_ENV_FILE="canary.env"
& ".\x64\Debug\Discord Bot.exe" *> canary-session.log
```

Do not commit captured log files. They may include Discord IDs, tournament data, player identifiers, local paths, or API request URLs.

## Current Policy

The bot should not write log files by itself during beta testing.

If persistent logs become necessary later, add them behind an explicit opt-in setting such as:

```text
BOT_LOG_FILE=false
BOT_LOG_LEVEL=info
BOT_LOG_DIR=logs
BOT_LOG_ROTATE_MB=5
```

The default should remain terminal-only.
