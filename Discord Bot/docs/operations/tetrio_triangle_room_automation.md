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
  -> room link is posted in the match thread
```

If room creation fails, match thread creation still succeeds and staff can create the room manually.

## Toggle

Global/env toggle:

```text
BOT_ENABLE_TETRIO_ROOM_AUTOMATION=false
```

Bridge URL:

```text
TRIANGLE_BRIDGE_URL=http://127.0.0.1:8787
```

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
  "room_url": "https://tetr.io/#R:roomid"
}
```

## Third-Party Code

Triangle.js was downloaded into:

```text
third_party/triangle
```

It is MIT licensed. Keep the license notice when redistributing vendored copies.

## Operational Rules

- Keep this disabled unless the deployment is approved for TETR.IO room automation.
- Use a dedicated TETR.IO bot account.
- Do not use a personal player account.
- Keep TETR.IO credentials in ignored env files or process environment only.
- Treat bridge failure as non-fatal.
- Keep manual room creation as the fallback.
