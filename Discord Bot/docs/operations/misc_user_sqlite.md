# Misc User SQLite Database

The misc module has an isolated SQLite database for user-customizable extensions.

For the general trusted/isolated command model, see:

```text
docs/operations/misc_command_trust_model.md
```

## Purpose

Use this database when an extension needs storage but should not touch core bot tables, tournament tables, moderation tables, or profile tables.

Default path:

```text
db/user.db
```

Runtime override:

```text
BOT_USER_DB_PATH
```

The database is intentionally blank by default. The wrapper opens the file and applies SQLite pragmas, but it does not create app tables, schema metadata, or extension tables.

## Files

```text
include/misc/sqlite-user.hpp
src/misc/sqlite-user.cpp
```

## Usage

```cpp
#include "misc/sqlite-user.hpp"

bool init_my_extension() {
	auto& db = misc_user_sqlite::user_db();
	if (!db.ok()) {
		return false;
	}

	return db.execute(
		"CREATE TABLE IF NOT EXISTS my_extension_state ("
		"id INTEGER PRIMARY KEY,"
		"value TEXT NOT NULL"
		");"
	);
}
```

Use `misc_user_sqlite::UserDatabaseTransaction` for multi-step writes.

## Isolation Rules

- Do not store core bot data here.
- Do not add foreign keys to core bot tables.
- Do not assume tournament IDs, Discord IDs, or profile rows exist unless the extension validates them itself.
- Prefix extension tables clearly, such as `my_feature_state` or `custom_mode_results`.
- Keep migrations idempotent with `CREATE TABLE IF NOT EXISTS`, `add_column_if_missing`, and `create_index_if_missing`.

## Reliability Coverage

The SQL reliability check verifies that the isolated user DB:

- opens successfully,
- starts with no tables,
- allows extension-owned tables and indexes,
- rolls back uncommitted transactions,
- passes `PRAGMA quick_check`.
