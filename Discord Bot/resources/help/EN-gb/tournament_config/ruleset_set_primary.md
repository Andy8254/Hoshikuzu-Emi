# /tournament config ruleset_set_primary
Sets the tournament's default ruleset.
## Usage
`/tournament config ruleset_set_primary id:<id> first_to:<score> [deuce] [win_by] [score_cap] [allow_draw]`
## Details
Defines the default match win condition.
## Notes
Use `score_cap:0` for no cap.
When deuce is enabled, the win condition can use either win-by difference or golden point.
