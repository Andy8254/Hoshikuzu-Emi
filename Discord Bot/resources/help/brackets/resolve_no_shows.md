# /tournament bracket resolve_no_shows
Resolves due match no-shows and pending auto-DQs.
## Usage
`/tournament bracket resolve_no_shows id:<tournament_id>`
## Rules
If one player checked in, they advance. If neither checked in, lower seed loses now and upper seed is auto-DQ flagged for the next match.
## Notes
Run this after match grace time has expired.
