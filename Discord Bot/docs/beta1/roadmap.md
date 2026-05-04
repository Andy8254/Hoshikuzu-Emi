# Beta1 Roadmap

## 1. Consistency Pass

Unify names, message style, permission failures, log formatting, command grouping, and module boundaries.

Priority targets:

- Slash command names and option names
- Embed title/color/footer conventions
- Button custom ID format
- Public vs ephemeral response rules
- Tournament log and moderation log style
- Help file coverage and wording
- Command aliases or legacy command cleanup

## 1.5. Feedback and Hole-Filling

Run internal smoke tests and Hami canary sessions. Classify findings as:

- Bug
- Missing policy behavior
- UX friction
- Unsafe workflow
- Command clutter
- Performance or database issue
- Documentation gap

Do not jump directly into embed UI until this list exists.

## 2. Policy Alignment

Bring the implementation in line with Stacking Arena policy:

- Moderation levels 1-5
- Soft warning vs formal warning
- Time-out range conventions
- Appealable vs permanent ban
- Tournament absence procedure
- Staff-confirmed no-show timing
- Upper-seed extra 5-minute response window
- Resignation flow for double elimination
- Objections and appeals

## 3. Embed UI and UX

Create guided panels for repeated workflows:

- Tournament dashboard
- Staff tournament panel
- Match panel
- Registration/check-in panel
- Moderation case panel
- Settings panel

Keep direct slash commands for staff/debug use.

## 4. Command Chunking and Optimisation

Reduce the visible command surface by moving repeated actions into panels, buttons, and select menus.

Target state:

- Fewer top-level commands
- More guided interactions
- Less manual ID copying for players
- Staff commands remain explicit and auditable

## 5. Data Integrity and Security

Bundle together:

- External import/export verification
- SQL/data signatures for external submissions
- Migration checks
- Audit trails
- DB backups
- Runtime token hygiene
- Separate stable/canary DB paths

## 6. Canary Deployment

Use Hoshikuzu Hami as the beta/canary bot.

Rules:

- Separate token
- Separate application ID
- Separate database
- Guild commands only
- Test community only
- Clearly marked beta status

## 7. Beta Test Events

Suggested ramp:

- 4-8 player dry run
- 8-16 player first beta test
- 16-32 player confidence test
- 32-64 player stress test
- 64-100 player late beta or release-candidate test with backup bracket tooling

