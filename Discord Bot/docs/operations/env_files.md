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
BOT_ENABLE_TRUSTED_MAHJONG=false
BOT_ENABLE_YOUTUBE_RANDOMIZER=false
BOT_ENABLE_TETRIO_ROOM_AUTOMATION=false
TRIANGLE_BRIDGE_URL=http://127.0.0.1:8787
TRIANGLE_MATCH_START_GRACE_SECONDS=30
TRIANGLE_WARMUP_MATCHES=1
TRIANGLE_POST_WARMUP_START_DELAY_SECONDS=10
TRIANGLE_REPLAY_DIR=db/replays/tetrio
TRIANGLE_MAX_ACTIVE_ROOMS=1
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
BOT_ENABLE_TRUSTED_MAHJONG
BOT_ENABLE_YOUTUBE_RANDOMIZER
BOT_ENABLE_TETRIO_ROOM_AUTOMATION
TRIANGLE_BRIDGE_URL
TRIANGLE_MATCH_START_GRACE_SECONDS
TRIANGLE_WARMUP_MATCHES
TRIANGLE_POST_WARMUP_START_DELAY_SECONDS
TRIANGLE_REPLAY_DIR
TRIANGLE_MAX_ACTIVE_ROOMS
```

`BOT_TOKEN` is required for normal bot startup.

`BOT_DB_PATH` overrides the core database path.

`BOT_USER_DB_PATH` overrides the isolated misc/user extension database path.

`BOT_ENABLE_TRUSTED_MAHJONG` enables the trusted Mahjong scaffold. It is disabled by default and currently exposes no slash commands.

`BOT_ENABLE_YOUTUBE_RANDOMIZER` enables the isolated YouTube randomizer schema. It is disabled by default and currently exposes no slash commands.

`BOT_ENABLE_TETRIO_ROOM_AUTOMATION` enables the optional Triangle.js room automation integration when set to `true`, `1`, `yes`, or `on`. It is disabled by default.

`TRIANGLE_BRIDGE_URL` points the C++ bot to the local Triangle bridge service. The default is `http://127.0.0.1:8787`.

`TRIANGLE_MATCH_START_GRACE_SECONDS` controls the TETR.IO room auto-start delay after both invites are sent. The default is `30`.

`TRIANGLE_WARMUP_MATCHES` controls automatic FT1 warm-ups before the official match. The default is `1`; set it to `0` to disable automatic warm-up matches.

`TRIANGLE_POST_WARMUP_START_DELAY_SECONDS` controls the delay between the last warm-up result and the official match start. The default is `10`.

`TRIANGLE_REPLAY_DIR` controls where Triangle replay exports are stored. The default is `db/replays/tetrio`.

`TRIANGLE_MAX_ACTIVE_ROOMS` limits simultaneous Triangle-controlled rooms. The default is `1` because a single TETR.IO bot account and replay capture session should be treated as one active match at a time unless a deployment has been tested for higher concurrency.
