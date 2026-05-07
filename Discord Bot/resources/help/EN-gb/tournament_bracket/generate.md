# /tournament bracket generate
Generates a bracket from checked-in players.
## Usage
`/tournament bracket generate id:<id> [type]`
## Details
Creates a bracket for single elimination, double elimination, round robin, or Swiss.
After successful generation, the bot automatically queues Discord threads for current playable matches in the configured tournament channel.
## Notes
Run after registration, check-in, and seeding are complete.
Use `/tournament bracket threads` only as a manual recovery command, or when you need to create threads for a specific round later.
