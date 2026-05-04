# Beta1 Development Handoff

This folder exists to help the next development pass continue from `v0.1.0-alpha2` without rediscovering the project shape from scratch.

## Current Status

`v0.1.0-alpha2` is a feature-complete alpha checkpoint. The backend has enough tournament, moderation, settings, help, logging, bracket, and SVG surface area to begin beta-oriented cleanup.

The next milestone discussed is:

`v0.1.5-beta-dev1 - Hole Filling and Optimisation`

This is not a public stable release. It is a bridge from alpha2 toward beta1.

## Beta1 Pipeline

1. Consistency pass
2. Feedback and hole-filling
3. Policy alignment
4. Embed UI and UX implementation
5. Command chunking and optimisation
6. Data integrity and security
7. Canary deployment with Hoshikuzu Hami
8. Beta test events

## Development Posture

Alpha asked: can this exist?

Beta asks: can staff run this under pressure?

Stable asks: can this fail gracefully during a real event?

## Current Trust Level

The bot should be treated as internally testable, not stable. For any real event, run it in parallel with a conventional bracket platform until beta testing proves the workflows.

## Next Codex Starting Point

Read these files first:

- `docs/beta1/README.md`
- `docs/beta1/roadmap.md`
- `docs/beta1/policy_alignment.md`
- `docs/beta1/test_plan.md`
- `docs/beta1/dev1_backend_notes.md`
- `docs/conventions/backend.md`
- `docs/conventions/commands.md`
- `docs/conventions/messages.md`
- `docs/operations/security.md`
- `docs/operations/human_in_loop_scripting.md`
