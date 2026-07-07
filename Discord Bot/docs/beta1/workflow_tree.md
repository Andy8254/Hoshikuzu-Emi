# Beta1 Tournament Workflow Tree

This document is the quick navigation tree for running a beta1 tournament with the bot.

Use the detailed English and Korean management guides for full command examples and troubleshooting notes.

## High-Level Tree

```text
Tournament Operation
|
+-- 0. Pre-Run Decision
|   |
|   +-- Choose bot identity
|   |   +-- Stable
|   |   +-- Canary
|   |
|   +-- Confirm runtime
|   |   +-- Bot process starts
|   |   +-- Commands register
|   |   +-- /codex ping or /codex info replies
|   |
|   +-- Prepare fallback
|       +-- External bracket platform
|       +-- Staff spreadsheet
|       +-- Manual score log
|
+-- 1. Server Setup
|   |
|   +-- Configure staff/admin roles
|   +-- Configure tournament channel
|   +-- Configure private log channel
|   +-- Confirm permissions
|
+-- 2. Tournament Creation
|   |
|   +-- Create tournament
|   +-- Record tournament ID
|   +-- Confirm game type
|   +-- Confirm bracket format
|   +-- Inspect public/staff info
|
+-- 3. Rules and Restrictions
|   |
|   +-- TETR.IO event
|   |   +-- Configure rank/TR restrictions if needed
|   |   +-- Confirm profile lookup works
|   |
|   +-- TE:C or PPT2 event
|   |   +-- Choose rating bucket
|   |   +-- Prepare manual rating points
|   |
|   +-- General event
|   |   +-- Choose single rank-point source
|   |
|   +-- Configure match ruleset
|       +-- Primary rules
|       +-- Secondary rules, if needed
|
+-- 4. Registration
|   |
|   +-- Open registration
|   +-- Players register
|   +-- Staff handles manual registrations
|   +-- Monitor participants
|   +-- Close registration
|
+-- 5. Check-In
|   |
|   +-- Open check-in
|   +-- Players check in
|   +-- Staff handles manual check-ins
|   +-- Close check-in
|   +-- Confirm checked-in participant set
|
+-- 6. Seeding
|   |
|   +-- Fast path
|   |   +-- Run seed command
|   |   +-- Bot applies order immediately
|   |
|   +-- Human-reviewed CSV path
|       +-- Export seed CSV
|       +-- Staff reorders rows
|       +-- Import seed CSV
|       +-- Validate username set
|       +-- Apply seed order
|
+-- 7. Bracket Generation
|   |
|   +-- Confirm final seeds
|   +-- Generate bracket
|   +-- Inspect current matches
|   +-- Create match threads
|   +-- Confirm player-facing UI/buttons if used
|
+-- 8. Live Bracket Operation
|   |
|   +-- Match starts
|   |   |
|   |   +-- Manual room flow
|   |   |   +-- Staff or players create room
|   |   |   +-- Players play match
|   |   |   +-- Result is reported
|   |   |
|   |   +-- TETR.IO Triangle automation, if enabled
|   |       +-- Create room
|   |       +-- Invite both players
|   |       +-- Wait grace time
|   |       +-- Run FT1 warm-up, if enabled
|   |       +-- Start official match
|   |       +-- Save replay locally
|   |
|   +-- Match result
|   |   +-- Player report
|   |   +-- Staff report
|   |   +-- Staff correction, if approved
|   |
|   +-- Match exception
|       +-- Call staff
|       +-- Forfeit
|       +-- No-show resolution
|       +-- Manual decision recorded in staff channel
|
+-- 9. Public Outputs
|   |
|   +-- Current matches
|   +-- Round view
|   +-- Standings
|   +-- Bracket SVG
|   +-- Match SVG
|   +-- Stream assignment list
|
+-- 10. Post-Event
|   |
|   +-- Export final bracket/standings
|   +-- Save staff notes
|   +-- Copy terminal logs if needed
|   +-- Record issues for beta feedback
|   +-- Keep database unless cleanup is explicitly approved
|
+-- 11. Emergency Fallback
    |
    +-- Trigger conditions
    |   +-- Commands disappear
    |   +-- Bot becomes unresponsive
    |   +-- Bracket state cannot be trusted
    |   +-- Correction cannot safely repair a wrong result
    |   +-- Staff cannot validate participant or seed state
    |
    +-- Fallback action
        +-- Announce staff-controlled fallback
        +-- Screenshot or copy current bot state
        +-- Stop mutating bot tournament state
        +-- Continue in external bracket or spreadsheet
        +-- Preserve logs for debugging
```

## Critical Gates

Do not move to the next phase until the gate is true.

| Phase | Gate |
| --- | --- |
| Runtime | The intended stable/canary bot replies to a known command. |
| Setup | Staff/admin roles and log channel are confirmed. |
| Creation | Tournament ID, game type, and format are recorded. |
| Registration | Registration is closed and participant list is reviewed. |
| Check-in | Check-in is closed and checked-in set is final. |
| Seeding | Seed order is confirmed by staff. |
| Bracket | Bracket was generated after final seed order. |
| Live matches | Current match state is visible to staff. |
| Corrections | Admin approval exists and downstream state is checked. |
| Fallback | Staff has stopped mutating bot state before continuing externally. |

## Quick Branch Rules

- Use the fast seeding path for small tests and low-risk events.
- Use the CSV seeding path for real beta tournaments.
- Use manual rating buckets for TE:C, PPT2, and other non-TETR.IO modes.
- Use Triangle automation only when the TETR.IO bot account, bridge, and replay directory are ready.
- Treat report correction as an admin action, not a normal score-entry workflow.
- Preserve terminal logs when any issue affects live bracket state.
