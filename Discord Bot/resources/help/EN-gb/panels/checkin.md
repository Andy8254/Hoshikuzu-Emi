# Checkin Panel
Explains check-in and match action panels.

## Usage
`/tournament checkin_open id:<id> closes_at:<unix> [grace_time]`

## Details
The check-in panel confirms attendance with one button. Match threads expose Check in, Report Score, Forfeit, and Call Staff.

## Notes
Panels reduce typing; staff slash commands remain recovery/audit paths.

`closes_at` controls when normal check-in closes. `grace_time` is extra late-check-in allowance after that point, in seconds.

Example: if check-in should last one hour from opening, set `closes_at` to opening time plus 3600 seconds. Use `grace_time:0` for a hard close, or `grace_time:600` for ten minutes of late check-in.
