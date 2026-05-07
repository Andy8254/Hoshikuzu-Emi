# /tournament bracket
Tools for managing brackets, matches, stream assignments, no-shows, score reports, standings, and visual exports.
## Who Uses This
Tournament staff use these commands directly.
Regular users can use the buttons provided in bot-created match threads.
## Workflow
Generate the bracket from checked-in players, inspect current matches, create threads, then report or correct results.
## Commands
- `generate`: Generates bracket matches from checked-in players.
- `current`: Shows current playable matches.
- `round`: Shows matches in the selected round.
- `match`: Shows information for the selected match.
- `report`: Reports the final score for the selected match.
- `correct_report`: Corrects a completed match report.
- `forfeit`: Records a player forfeit or DQ.
- `resolve_no_shows`: Resolves matches whose no-show timer has expired.
- `threads`: Creates Discord match threads.
- `stream_assign`: Marks a match as assigned to livestream.
- `stream_clear`: Clears a livestream assignment.
- `stream_list`: Shows matches assigned to livestream.
- `standings`: Shows Swiss or round-robin standings.
- `svg`: Exports the full bracket as SVG.
- `match_svg`: Exports information for the selected match as SVG.
## Help
Use `/bot help category:tournament_bracket command:<command>` for a command-specific page.
