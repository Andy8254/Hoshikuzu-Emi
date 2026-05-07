# /tournament
Tournament and player-management tools for creation, registration, check-in, seeding, and participant views.
## Who Uses This
Regular users can use player-facing flows such as registration, check-in, and calling staff.
Staff can use operational flows for tournament management.
## Workflow
Create the tournament, open registration, open check-in, seed checked-in players according to the selected criteria, then generate the bracket.
## Commands
- `create`: Creates a new tournament.
- `edit`: Edits tournament metadata.
- `delete`: Deletes the selected tournament.
- `clear`: Deletes all tournament data stored for the server.
- `info`: Shows public tournament information.
- `staff_info`: Shows staff tournament information.
- `registration_open`: Opens registration and posts a registration panel.
- `registration_close`: Closes tournament registration.
- `checkin_open`: Opens check-in and posts a check-in panel.
- `checkin_close`: Closes tournament check-in.
- `register`: Registers or unregisters a player by slash command.
- `checkin`: Checks in or unchecks a player by slash command.
- `call_staff`: Calls staff for the current match.
- `participants`: Lists tournament participants.
- `seed`: Assigns seed numbers to checked-in players according to the selected criteria.
## Help
Use `/bot help category:tournament command:<command>` for a command-specific page.
