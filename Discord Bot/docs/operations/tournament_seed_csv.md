# Tournament Seed CSV Workflow

This document describes the staff workflow for exporting, editing, and importing tournament seed order.

## Commands

### `/tournament seed_export`

Exports the current generated seed order as a CSV file.

Options:

- `id`: Tournament ID.
- `mode`: Optional seeding mode. Supported values are `general`, `tetrio`, and `rating`.
- `bucket`: Optional manual rating bucket. Use this when `mode` is `rating`.

The export uses the same seeding logic as `/tournament seed`, but it does not apply seed changes by itself. Staff can reorder the CSV rows manually, then import the file.

Excluded players are not included in the exported CSV.

### `/tournament seed_import`

Imports a CSV attachment and applies seed positions from the row order.

Options:

- `id`: Tournament ID.
- `file`: CSV file attachment.

The import validates all usernames before applying changes. If the CSV does not match the checked-in tournament participants, the bot returns an error and does not apply the imported order.

## Accepted CSV Formats

### Full Export CSV

The CSV produced by `/tournament seed_export` can be imported after staff reorder the player rows.

Example:

```csv
seed,discord_id,display_name,tetrio_id,tetrio_rating,tetrio_current_rank,tetrio_top_rank,tetrio_world_rank,has_tetrio_data,tetrio_status
1,543676141177798676,andy8254,ajm8254,0,Z,Z,0,false,not_checked
2,123456789012345678,PlayerTwo,player_two,0,Z,Z,0,false,not_checked
```

Staff do not need to edit the `seed` column. The bot uses the order of the rows in the CSV as the new seed order.

### Username-Only CSV With Header

```csv
username
ajm8254
player_two
third_player
```

### Username-Only CSV Without Header

```csv
ajm8254
player_two
third_player
```

## Username Matching

The import accepts these header names for the username column:

- `username`
- `tetrio_id`
- `provided_username`
- `player`
- `name`

Matching is case-insensitive and ignores leading or trailing whitespace.

For each checked-in participant, the bot matches against:

1. TETR.IO ID, if the participant has one.
2. Provided username, if no TETR.IO ID is stored.

## Validation Rules

The import is rejected when:

- The CSV is empty.
- A username cell is empty.
- The CSV contains duplicate usernames.
- The CSV contains a username that is not a checked-in participant.
- A checked-in participant is missing from the CSV.
- Tournament participant username data contains duplicates.
- A checked-in participant has no usable username data.

No seed changes are applied unless username validation succeeds.

## Staff Workflow

1. Run `/tournament seed_export`.
2. Download and edit the CSV.
3. Reorder player rows according to staff discretion.
4. Keep usernames unchanged.
5. Upload the CSV with `/tournament seed_import`.
6. If the bot reports success, the row order has been applied as seed positions.
