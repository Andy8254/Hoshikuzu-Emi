# YouTube Randomizer Module Scaffold

This module is an isolated misc extension scaffold for storing and randomly selecting YouTube links.

For the general trusted/isolated command model, see:

```text
docs/operations/misc_command_trust_model.md
```

## Status

Default:

```text
BOT_ENABLE_YOUTUBE_RANDOMIZER=false
```

Current code locations:

```text
include/misc/isolated/youtube_randomizer.hpp
src/misc/isolated/youtube_randomizer.cpp
```

The module currently provides backend functions only. It does not register slash commands or send embeds.

## Database

The module uses the isolated misc user SQLite database:

```text
BOT_USER_DB_PATH=db/user.db
```

Table:

```text
misc_youtube_randomizer_links
```

Stored fields:

- guild ID,
- display title,
- YouTube URL,
- user who added the link,
- added timestamp,
- enabled flag.

The table has a unique constraint on `(guild_id, youtube_url)`.

## Intended Commands

Future staff-only or trusted commands may include:

```text
/misc music add title:<title> url:<youtube_url>
/misc music random
/misc music list
/misc music disable id:<id>
/misc music enable id:<id>
```

## Safety Notes

- Keep this isolated from core tournament tables.
- Validate URLs before exposing public commands.
- Prefer YouTube watch, Shorts, or playlist URLs only if staff explicitly allows them.
- Do not auto-play audio. Return a link or embed preview only.
- Keep guild data scoped by guild ID.
