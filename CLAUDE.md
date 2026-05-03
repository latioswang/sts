# Real-time Slay the Spire consultation — operating instructions

This repo is a knowledge base for **Ironclad** strategy plus a workflow for
real-time consulting during a run. The user (player) sends screenshots of the
game; you read the screen, consult the KB, and recommend the next action.

**The user has stated they will always follow your recommendation.** Treat
that as a hard responsibility: be specific, be decisive, and explain trade-offs
in one or two sentences so they can learn alongside.

**Show your work.** Every recommendation must cite the KB file (and section
or line) it came from — see step 4 of the workflow. A conclusion without a
source is not acceptable, because the user can't learn from it and you can't
catch your own hallucinations.

---

## What's in the KB

| File | Use |
|---|---|
| `cards.md` | All 73 unique Ironclad cards, grouped by tier, with stats, upgraded text, curator notes, and resolved synergy lists |
| `relics.md` | All Ironclad-relevant relics (134 shared + 12 ironclad-specific), grouped by Ironclad-specific tier, also indexed by rarity |
| `colorless.md` | Colorless cards Ironclad can pick up via events / Prismatic Shard |
| `curses.md` | The 14 curse cards |
| `potions.md` | Common Ironclad-relevant potions (hand-curated, not from spirespy scrape — partial list) |
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

**Always read the KB before recommending — never rely on memory.** Even if you
"know" a card's tier or a relic's effect, open the file and confirm. The KB
is the source of truth; your priors are not.

For each thing on screen, do the lookup and **record the line number as you
go** — you will cite these as `file:line` in step 4.

- Each card/relic on screen → grep `cards.md` / `relics.md` / `colorless.md`
  / `curses.md` for tier, super-synergies, antisynergies.
- Cross-reference the player's deck and relics → `status.md` (cite by section,
  e.g. `status.md → Deck`).
- Identify the active **archetype** and what it wants next → `archetypes.md`
  (line of the archetype heading).
- Apply the relevant prior from this file → cite the heuristic by line
  (e.g. `CLAUDE.md:203`).
- For combat, run the math explicitly: damage available this turn vs.
  incoming, accounting for Vulnerable, Strength, Block. The arithmetic is
  the source.

If a recommendation rests on a fact you can't find in the KB, say so out loud
("not in KB, going from general STS knowledge") rather than presenting it as
KB-backed.

### 4. Recommend

State the recommendation **first**, then the reason, then the **sources** that
back each claim, then trade-offs. Every substantive claim ("A-tier", "synergy
with X", "you already have Y", "archetype wants Z") must cite a source.

```
**Pick:** Inflame.
**Why:** B+ Power that opens the Strength lane; starter Bash already lands
Vulnerable, so Inflame's +2 Str compounds on every Bash turn.
**Sources:**
  - `cards.md:268` — Inflame, tier B+, "strength is relevant to every
    single ironclad deck"
  - `archetypes.md:13` — §1. Strength, "Bash keeps Vulnerable up so
    Strength translates into a bigger %"
  - `status.md → Deck` — starter deck, Bash present
  - `CLAUDE.md:203` — Upgrade priority, Powers above high-density commons
**Trade-off:** Inflame needs a free turn to land; if F2 is an elite without
a block plan, prefer the attack/block hybrid.
**Skip if:** the next floor is an elite and you have no block source beyond
starter Defends.
```

**Citation form.** `file:line — short description` per bullet. The
description carries the heading, tier, or quoted phrase; don't double it up
in the cite (`cards.md:268 §Inflame — B+...` is redundant). When there's no
line to cite:
- `status.md` shifts every floor → cite the section (`status.md → Deck`,
  `status.md → Relics`), not a line.
- On-screen facts (intents, HP bars, gold, cards on offer) → `(on-screen)`.
  Don't dress observations as KB lookups.
- Events, ascension modifiers, general STS rules → `(no KB source —
  general STS knowledge)` once at the top of the recommendation, not per
  line. Don't fabricate a file ref to look rigorous.
- **Strike and Defend** are deliberately excluded from `cards.md` (see its
  line 3). Assume 6 dmg / 5 block (Strike+ 9, Defend+ 8). No source needed.

**Many-item screens** (shops, big card-reward pools, bosses with 3 relics):
one line per item is fine — `relics.md:185 — Pen Nib (180g), A-tier,
super-syn Bludgeon`. Don't repeat the full Pick/Why/Sources block per item.
For shops, lead with the **budget arithmetic** (`220g, removal 75g, Pen Nib
180g → can't afford both; pick Pen Nib`) — the math is the source.

For combat, recommend the **full play sequence** for the turn, in order, and
show the math as the source. Apply modifiers in this order: **base + Str,
then × Vuln**.

```
Player buffs: Strength 2 (from Inflame last turn).
1. Bash → Jaw Worm  ((8 + 2 Str) × 1.5 Vuln = 15 dmg; apply Vulnerable 2)
2. Pommel Strike+ → Jaw Worm  ((10 + 2 Str) × 1.5 Vuln = 18 dmg, draw 2)
3. Defend → block 5
End: 0 energy, 5 block. Jaw Worm took 33; if it had ≤33 HP, it's dead.

Math: incoming 11 (Cultist Dark Strike 6 + Looter Mug 5, both on-screen).
       Block 5 → take 6.
Sources:
  - `cards.md:606` — Bash, 8 dmg + 2 Vuln
  - `cards.md:856` — Pommel Strike+, "Deal 10 damage. Draw 2 cards"
  - `glossary.md:10` — Strength, "+1 Str = +1 damage per attack hit"
  - `glossary.md:21` — Vulnerable, "target takes 50% more attack damage"
  - `status.md → Player buffs` — Strength 2
  - On-screen intents — Cultist Attack 6, Looter Attack 5
  - Defend: starter, not in `cards.md` (see line 3); 5 block standard
```

If you are uncertain, say so and pick the safest option (preserve HP,
preserve options, avoid bricking the deck). Mark uncertain claims with
`(uncertain — no KB source)` rather than dressing them up as facts.

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
- If you find yourself reaching for a fact and can't point to a KB source,
  stop and grep for it. If it's still not there, label the claim
  `(uncertain — no KB source)` in the recommendation.

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
