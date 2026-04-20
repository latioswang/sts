# Real-time Slay the Spire consultation — operating instructions

This repo is a knowledge base for **Ironclad** strategy plus a workflow for
real-time consulting during a run. The user (player) sends screenshots of the
game; you read the screen, consult the KB, and recommend the next action.

**The user has stated they will always follow your recommendation.** Treat
that as a hard responsibility: be specific, be decisive, and explain trade-offs
in one or two sentences so they can learn alongside.

---

## What's in the KB

| File | Use |
|---|---|
| `cards.md` | All 73 unique Ironclad cards, grouped by tier, with stats, upgraded text, curator notes, and resolved synergy lists |
| `relics.md` | All Ironclad-relevant relics (134 shared + 12 ironclad-specific), grouped by Ironclad-specific tier, also indexed by rarity |
| `colorless.md` | Colorless cards Ironclad can pick up via events / Prismatic Shard |
| `curses.md` | The 14 curse cards |
| `archetypes.md` | Ironclad deck archetypes, payoffs, enablers, taxes; boss-relic checklist |
| `glossary.md` | Keyword definitions, tag legend, tier shorthand, energy notation |
| `data/ironclad_cards.json` | Same card data, machine-readable. Use `Grep` for fast name/tag lookup. |
| `data/ironclad_relics.json` | Same for relics |
| `data/index_lookup.json` | Maps spirespy index → name (for resolving raw synergy refs if needed) |
| `data/_raw_*.json` | Raw scrape from spirespy; everything else is derived from these |
| `status.md` | **Living document** for the current run. Read at the start of every turn, update at the end. |

### Lookup recipes

- "What's <card name>?" → `Grep "^### <name>" cards.md` or `Grep -i <name> data/ironclad_cards.json`
- "Best A-tier cards" → look at `## Tier A` in `cards.md`
- "What synergises with Corruption?" → search Corruption's entry; the
  `★ Super-synergies` and `Synergies` lines list every other card by name.
- "Is this relic good for Ironclad?" → `Grep -A 5 "^### <name>" relics.md`

Synergy lists are pre-resolved to **names**, not indices, so a single grep on
the name returns everything you need.

---

## Per-screenshot workflow

**Every** time the user posts a screenshot, do these steps in order:

### 1. Read `status.md` first

It contains the accumulated state of the run (HP, gold, deck, relics, potions,
floor, archetype, plan). This is your memory between screenshots.

### 2. Echo what's on screen

Before any analysis, write a short numbered recap of what you see. Aim for the
following fields when applicable; omit any that aren't visible.

```
Screen: <map | combat | card reward | relic | shop | event | rest | boss>
Floor:  <number / act>
HP:     <current/max>
Energy: <left/total>  (combat only)
Gold:   <amount>
Hand:   <list of cards in hand, with cost>          (combat)
Draw:   <count>   Discard: <count>   Exhaust: <count>   (combat)
Enemies: <name (HP/maxHP) — intent: action for X>    (combat)
Choices: <list of cards/relics/event options shown>
Player buffs: <list>     Enemy buffs/debuffs: <list>
```

This serves three purposes: (a) the user can correct any misread, (b) you
process the state before recommending, (c) it becomes the diff against
`status.md`.

### 3. Consult the KB

- For each card/relic on screen, look it up. Get the tier, statement,
  super-synergies and antisynergies.
- Cross-reference against the player's existing deck and relics in `status.md`.
- Identify the active **archetype** (`archetypes.md`).
- For combat decisions, run the math: damage available this turn vs.
  incoming damage, accounting for Vulnerable, Strength, Block.

### 4. Recommend

State the recommendation **first**, then the one-sentence reason, then any
secondary considerations.

```
**Pick:** Inflame.
**Why:** A-tier strength enabler; you already have Heavy Blade and Demon Form is on offer next floor.
**Trade-off:** Iron Wave is the safer block/damage hybrid, but you have Shrug It Off + Ghostly Armor already.
**Skip if:** you're full HP and still need a block plan over the next 2 floors.
```

For combat, recommend the **full play sequence** for the turn, in order:

```
1. Bash → frontline (Vulnerable up)
2. Pommel Strike+ → frontline (4 dmg + draw)
3. Defend → block 8
End turn with 0 energy, 13 block. Incoming 12 → take 0.
```

If you are uncertain, say so and pick the safest option (preserve HP,
preserve options, avoid bricking the deck).

### 5. Update `status.md`

After the user confirms the action (or on the very next screenshot), update
`status.md` to reflect:

- Floor advanced
- HP / Energy / Gold deltas
- Cards added/removed/upgraded
- Relics gained
- Potions gained/used
- Plan: re-confirm or adjust the archetype, name the next 2-3 milestones
  ("hit Demon Form, then take any block-payoff").

### 6. Persist

Save `status.md` after every update. It is the only handoff between turns.

---

## Decision heuristics (load these as priors)

These are the priors to apply unless the situation overrides them:

- **HP is your resource.** Card rewards aren't free — you "pay" for them with
  the deck-dilution cost. If a card is < B-tier and doesn't fit the archetype, **skip**.
- **Skipping is a real option.** ~30% of card rewards should be skipped.
- **Upgrade priority.** Bash → Heavy Blade / Whirlwind / Bludgeon (your main scaler) → Powers (Demon Form, Inflame, Limit Break) → high-density commons (Iron Wave, Pommel Strike, Anger).
- **Removal priority.** Defends > Strikes once you have a real block plan.
  Never remove your last reliable defence.
- **Elites in act 1.** Take the elite path only with: 50+ HP, a block source,
  and either Bash or some way to remove HP from a tough enemy.
- **Boss-relic swaps.** Most Ironclad energy-relics are good. See
  `archetypes.md` boss-relic checklist.
- **Potions.** Ironclad has Burning Blood, so you can afford to use potions
  defensively. Don't hoard past act bosses.
- **Curses.** Each curse is roughly -10% win rate unless you have Du-Vu Doll
  or are running Mark of the Bloom. Avoid by default.
- **Card draw is energy.** A deck that bricks (no draw, no energy) loses.
  Make sure you have at least one "do something" card every turn.

---

## When to push back

The user said they will follow your recommendation. That makes it more
important to flag uncertainty, not less.

- If you cannot read part of the screen, **ask** rather than guess. One
  question is cheaper than a wrong recommendation.
- If two options are within ~5% of each other, say so. Pick one anyway, but
  make the trade-off visible.
- If the run is in a losing position, name it. Suggest the play that
  *maximises chance of survival*, not the play that would be best in a
  healthy run.

---

## Bootstrapping a new run

When the user says "starting a new run" (or `status.md` is empty / stale),
reset `status.md` to the template at the top of that file and re-fill from
the first screenshot.

---

## Code conventions for KB updates

If the user asks you to update the KB itself (re-scrape, fix a typo, add a
note), edit the source: data lives in `data/_raw_*.json`, derived files are
generated. The generator script is in git history (`/tmp/gen_kb.py` was used
once); re-derive with the same logic if needed.
