# SQL Reliability Check

Run the bot executable in local SQL check mode to validate the SQLite layer without starting Discord.

## Command

```powershell
& '.\x64\Debug\Discord Bot.exe' --sql-reliability-check db\sql_reliability_check.db
```

To verify encrypted moderation storage too:

```powershell
$env:BOT_DATA_KEY="<at least 32 random characters>"
& '.\x64\Debug\Discord Bot.exe' --sql-reliability-check db\sql_reliability_check_encrypted.db
```

## What It Checks

- Empty database schema creation
- Repeated schema initialization idempotency
- Required tables and columns
- Required operational indexes
- Server, user, guild, and moderation writes
- Encrypted moderation storage when `BOT_DATA_KEY` is set
- Tournament creation, registration, check-in, seeding, and single-elimination bracket writes
- Score report, score correction, stream flag, Discord thread IDs, and forfeit writes
- Round-robin and Swiss generation plus standings after one reported match
- Transaction rollback behavior
- `PRAGMA quick_check`
- `PRAGMA foreign_key_check`
- SQLite backup API and backup integrity

## Safety

The check mode sets `BOT_DB_PATH` before any database manager is initialized, so it does not need a bot token and does not connect to Discord.

The checker refuses to reset a database unless the filename contains `sql_reliability_check`, which keeps accidental runs away from `master.db` and canary databases.
