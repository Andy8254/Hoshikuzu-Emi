# Check-in Panel
Explains check-in and match action panels.
## Usage
`/tournament checkin_open id:<id> closes_at:<unix> [grace_time]`
## Details
Players confirm tournament attendance with one button on the check-in panel. Match threads can also show Check in, Report Score, Forfeit, and Call Staff buttons.
## Notes
This workflow is intended to happen through buttons. If something goes wrong, staff can still use slash commands for recovery.
`closes_at` controls when normal check-in closes. `grace_time` is extra late-check-in allowance after that point, in seconds.
Example: if check-in should last one hour from opening, set `closes_at` to opening time plus 3600 seconds. Use `grace_time:0` for a hard close, or `grace_time:600` for ten minutes of late check-in.
