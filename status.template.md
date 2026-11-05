# Run status

> Living document — updated after each user screenshot. See `CLAUDE.md` for
> workflow.
>
> **This file is `.gitignore`'d.** It holds transient state for the current
> run only. To start a fresh run: `cp status.template.md status.md` and
> fill in the Run header below from the first screenshot.

## Run header

- **Character:** <ironclad | silent | defect | watcher>
- **Ascension:** <0–20 — ask user; AscendersBane in deck implies ≥10>
- **Run started:** <YYYY-MM-DD>
- **Last update:** <YYYY-MM-DD, F? — short note>
- **Floor:** <number / act>
- **Boss seen (act 1):** —
- **Boss seen (act 2):** —
- **Boss seen (act 3):** —

## Resources

- **HP:** <current> / <max>
- **Gold:** <amount>
- **Potion slots:** <filled> / <total> — list potions held

## Deck (<N> cards)

```
<count> × <card name>     (<cost>: <effect summary>)
...
```

**Card count:** <N>
**Upgrade count:** <U>
**Notes:** <archetype signal — what the deck is leaning toward>

## Relics

- **<Starter relic>** (starter): <effect>.
- *(other relics — add as found)*

## Active plan / archetype

- **Archetype:** <committed | leaning toward X | undecided>.
  Reference `<char>_archetypes.md` for the full lane definition once
  committed (≥3 archetype cards in deck).
- **Win condition:** <how this deck plans to kill the act-3 boss>.
- **Block plan:** <which cards / relics carry block density>.
- **Next 2-3 milestones:**
  1. <e.g. pick up first archetype-defining rare>
  2. <e.g. find card removal>
  3. <e.g. survive act-1 elite at HP X>

## Draft priorities

> Start from `<char>_cards.md` tier rankings, then bias toward the
> committed archetype. Update this table as the deck commits.

| Tier | Pick |
|---|---|
| Auto-pick | <archetype payoffs and S-tier cards> |
| Strong | <A-tier and archetype enablers> |
| Situational | <B-tier with conditions> |
| Skip | <C-tier and antisynergies> |

## Recent decisions

- [F?] <decision and one-line reason>

## Open questions / risks

- <unanswered ascension / unread relic / route uncertainty>
- <HP risk before next elite/boss>
- <archetype commit deadline>
