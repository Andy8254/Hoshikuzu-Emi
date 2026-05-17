# TETR.IO Room Automation

The bot can optionally ask a local Triangle.js bridge to create private TETR.IO rooms for match threads.

This feature is disabled by default.

## Status

Use only for trusted deployments with an authorized TETR.IO bot account.

Triangle.js uses the TETR.IO main game API. That API is unofficial and can break. Triangle.js documentation also warns that main game API use should only happen with an official bot account.

## Architecture

```text
Discord C++ bot
  -> /tournament bracket threads rooms:true
  -> local Triangle bridge HTTP request
  -> Triangle.js creates a private TETR.IO room
  -> Triangle.js invites both TETR.IO users
  -> Triangle.js starts the match after the room start grace time
  -> Triangle.js stores the replay locally after the match ends
  -> room link is posted in the match thread
```

If room creation fails, match thread creation still succeeds and staff can create the room manually.

The C++ bot sends each participant's linked TETR.IO ID when available. If the linked ID is missing, it falls back to the username provided during tournament registration.

## Toggle

Global/env toggle:

```text
BOT_ENABLE_TETRIO_ROOM_AUTOMATION=false
```

Bridge URL:

```text
TRIANGLE_BRIDGE_URL=http://127.0.0.1:8787
TRIANGLE_MATCH_START_GRACE_SECONDS=30
TRIANGLE_WARMUP_MATCHES=1
TRIANGLE_POST_WARMUP_START_DELAY_SECONDS=10
TRIANGLE_REPLAY_DIR=db/replays/tetrio
TRIANGLE_MAX_ACTIVE_ROOMS=1
```

`TRIANGLE_MATCH_START_GRACE_SECONDS` is the invite grace window before warm-up, not the tournament no-show grace timer. `TRIANGLE_WARMUP_MATCHES` controls automatic FT1 warm-up matches before the official room start; set it to `0` to disable automatic warm-ups. `TRIANGLE_POST_WARMUP_START_DELAY_SECONDS` controls the delay between the final warm-up result and official match start. Keep `TRIANGLE_MAX_ACTIVE_ROOMS=1` unless the deployment has been tested with multiple authorized TETR.IO bot sessions; replay capture is tied to the active Triangle client session.

Command toggle:

```text
/tournament bracket threads id:<id> buttons:true rooms:true
```

Both the env toggle and command toggle must be enabled for the bot to request rooms.

## Bridge Setup

The bridge lives in:

```text
tools/triangle-bridge
```

Install dependencies:

```powershell
cd tools\triangle-bridge
npm install
```

Run:

```powershell
$env:TETRIO_BOT_TOKEN="..."
npm start
```

Alternative login:

```powershell
$env:TETRIO_BOT_USERNAME="..."
$env:TETRIO_BOT_PASSWORD="..."
npm start
```

The bridge listens on:

```text
http://127.0.0.1:8787
```

Health check:

```text
GET /health
```

Room creation endpoint used by the C++ bot:

```text
POST /rooms
```

Expected response:

```json
{
  "ok": true,
  "room_id": "roomid",
  "room_url": "https://tetr.io/#R:roomid",
  "invite_grace_seconds": 30,
  "warmup_matches": 1,
  "official_first_to": 7,
  "start_in_seconds": 30,
  "replay_dir": "db/replays/tetrio"
}
```

## Room Flow

For each requested match room, the bridge:

1. Creates a private TETR.IO room.
2. Switches the bot account to spectator.
3. Applies the configured room preset. The default is `tetra league`.
4. Applies `match.ft=1` when warm-up is enabled, otherwise applies the tournament ruleset win score.
5. Resolves and invites both TETR.IO users.
6. Waits 30 seconds by default.
7. Starts one FT1 warm-up match by default.
8. After the warm-up ends, applies the official `match.ft` from the tournament ruleset.
9. Starts the official room match and spectates all players.
10. Exports the official replay as a local `.ttrm` JSON file after `client.game.end`.

Optional bridge env:

```text
TRIANGLE_ROOM_PRESET=tetra league
TRIANGLE_FIRST_TO=2
```

`TRIANGLE_FIRST_TO` is only a fallback. The C++ bot normally sends the tournament ruleset's `win_score`.

## Third-Party Code

The committed integration is the local bridge in `tools/triangle-bridge`. It depends on the `@haelp/teto` npm package from Triangle.js.

If you vendor or redistribute a local copy of Triangle.js source, preserve its MIT license notice.

## Operational Rules

- Keep this disabled unless the deployment is approved for TETR.IO room automation.
- Use a dedicated TETR.IO bot account.
- Do not use a personal player account.
- Keep TETR.IO credentials in ignored env files or process environment only.
- Treat bridge failure as non-fatal.
- Keep manual room creation as the fallback.
- Use `GET /health` to confirm the active-room count before a live run.
