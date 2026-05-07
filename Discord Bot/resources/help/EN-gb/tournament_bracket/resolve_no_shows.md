# /tournament bracket resolve_no_shows
Resolves matches whose no-show timer has expired.
## Usage
`/tournament bracket resolve_no_shows id:<id>`
## Details
Checks matches whose grace windows expired and applies no-show handling where safe.
## Notes
Use this command carefully.
Review whether the automated result matches the intended tournament operations decision.
Under StAr policy, match no-show grace is 600 seconds, or 10 minutes, from match thread creation.
