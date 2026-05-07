# /tournament bracket threads
Creates Discord match threads.
## Usage
`/tournament bracket threads id:<id> [round] [buttons]`
## Details
Creates threads for current matches or a round. Button threads expose Check in, Report Score, Forfeit, and Call Staff.
## Notes
Bracket generation automatically queues threads for current playable matches.
Use this command as a manual recovery path, or when you need to create threads for a specific round later.
When a match thread is created, the match is marked as opened and the StAr match no-show grace timer starts.
Match no-show grace is fixed at 600 seconds, or 10 minutes.
