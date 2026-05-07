# /tournament checkin_open
Opens check-in and posts a check-in panel.
## Usage
`/tournament checkin_open id:<id> closes_at:<unix> [grace_time]`
## Details
Players press Check in and the bot reuses their registered username automatically.
## Notes
`closes_at` is the Unix timestamp when normal check-in closes.
`grace_time` is extra late-check-in allowance after `closes_at`, in seconds. It is not the main check-in duration.
For a one-hour check-in window with no late check-in, set `closes_at` to one hour after opening and set `grace_time` to `0`.
For a one-hour check-in window plus ten minutes of late check-in, set `closes_at` to one hour after opening and set `grace_time` to `600`.
