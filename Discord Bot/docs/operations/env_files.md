# Environment Files

The bot can load environment variables from a local env file before startup.

## Security

Never commit real bot tokens.

These files are ignored by git:

```text
.env
*.env
```

Commit only templates such as:

```text
.env.example
stable.env.example
canary.env.example
```

If a token is posted in chat, logs, screenshots, or commits, rotate it in the Discord Developer Portal before using it again.

## Default Local Run

Copy `.env.example` to `.env` and fill in local values:

```text
BOT_ENV=canary
BOT_TOKEN=
BOT_DB_PATH=db/master.db
BOT_USER_DB_PATH=db/user.db
BOT_ENABLE_MISC_EXAMPLES=false
```

Then run the bot normally. The bot attempts to load `.env` before reading `BOT_TOKEN`.

## Stable and Canary

Use separate env files when running separate bot identities.

Stable:

```powershell
$env:BOT_ENV_FILE="stable.env"
& ".\x64\Debug\Discord Bot.exe"
```

Canary:

```powershell
$env:BOT_ENV_FILE="canary.env"
& ".\x64\Debug\Discord Bot.exe"
```

`BOT_ENV_FILE` takes priority over `.env`.

## Supported Variables

```text
BOT_ENV
BOT_TOKEN
BOT_DB_PATH
BOT_USER_DB_PATH
BOT_ENABLE_MISC_EXAMPLES
```

`BOT_TOKEN` is required for normal bot startup.

`BOT_DB_PATH` overrides the core database path.

`BOT_USER_DB_PATH` overrides the isolated misc/user extension database path.
