# Message and Localization Conventions

## Message Layers

The planned beta1 language system should separate:

- Official language strings
- User language preference
- Server primary language
- Server secondary language
- Flavour/personality text
- User-specific easter eggs

## Recommended Resource Layout

```text
resources/lang/EN-gb.json
resources/lang/KO-kr.json
resources/flavour/emi.json
resources/flavour/hami.json
resources/easter_eggs/users.json
```

## Lookup Order

For normal user-facing messages:

1. User language
2. Server primary language
3. `EN-gb` fallback

For bilingual logs:

1. Server primary language
2. Server secondary language, if configured
3. `EN-gb` fallback

For flavour-safe messages:

1. Allowed easter egg override
2. User interaction flavour traits, if applicable
3. Bot flavour file
4. Normal language lookup

## Critical Message Rule

Do not use flavour text or easter eggs for:

- Moderation rulings
- Tournament disqualifications
- Appeals
- Privacy/legal notices
- Ban messages
- Audit logs that must be cited later

Allowed flavour keys:

- `hello.*`
- `fun.*`
- `beta.notice`
- `error.soft`
- `profile.flavour`
- `interaction.safe.*`

## User Interaction Flavour Traits

User interaction messages may be influenced by flavour traits when the message is non-critical.

Examples of safe trait influence:

- Greeting warmth
- Beta/canary caution wording
- Soft error phrasing
- Harmless profile flavour
- Friendly dashboard microcopy

Trait influence should be data-driven. Avoid hardcoding personality branches into command handlers.

Suggested shape:

```json
{
  "traits": {
    "careful": {
      "interaction.safe.retry": [
        "Please try again carefully.",
        "That did not settle properly. Try once more."
      ]
    },
    "playful": {
      "interaction.safe.retry": [
        "That button wandered off course. Try again?"
      ]
    }
  }
}
```

Resolution may choose from trait lines based on the active bot profile, user-specific flavour flags, or future staff-configured personality settings.

Do not let trait influence change meaning. It may change tone only.

## Placeholder Style

Use named placeholders:

```json
"tournament.register.ok": "You are registered for {tournament}."
```

Avoid positional placeholders for translated text.
