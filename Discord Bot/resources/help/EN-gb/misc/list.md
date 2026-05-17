# Misc Modules
`/misc` is reserved for optional and customisable modules.
## Purpose
Misc modules are not part of the core tournament flow by default. They are used for server-local utilities, experimental trusted modules, and isolated examples.
## Module Types
- Isolated modules use the separate user extension database and do not touch tournament records.
- Trusted modules may integrate with core tournament behavior, but stay disabled until reviewed and explicitly enabled.
## Current Scaffolds
- YouTube randomiser: isolated backend scaffold, no public command yet.
- Mahjong: trusted tournament-mode scaffold, no public command yet.
## Availability
If a misc feature is disabled, hidden, or not registered yet, it will not appear as a usable slash command.
Staff should check the server's deployment notes before relying on misc features during an event.
