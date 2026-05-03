# Slay the Spire strategy KB

A structured knowledge base for **all four Slay the Spire characters**
(Ironclad, Silent, Defect, Watcher), scraped from
[spirespy.maybelatergames.co.uk](https://maybelatergames.co.uk/spirespy/),
and arranged for **real-time consultation during a run**.

The user (player) sends screenshots; Claude reads the screen, looks things up
here, and recommends the next action. See [`CLAUDE.md`](CLAUDE.md) for the
exact workflow.

## Files

```
.
├── CLAUDE.md              ← consultation workflow (read this first if you're Claude)
├── status.md              ← living state of the current run
│
├── ironclad_cards.md      ← 73 Ironclad cards by tier, with notes & synergies
├── ironclad_cards_index.md      ← lean one-line-per-card scan
├── ironclad_cards_by_tag.md     ← reverse-tag index ("all draw cards", etc.)
├── ironclad_relics.md     ← 146 Ironclad-relevant relics by tier
├── ironclad_relics_index.md     ← lean one-line-per-relic scan
├── ironclad_relics_by_tag.md    ← reverse-tag index
├── ironclad_archetypes.md       ← deck themes, payoffs/enablers, boss-relic checklist
│
├── silent_cards.md        ← 73 Silent cards by tier
├── silent_cards_index.md
├── silent_cards_by_tag.md
├── silent_relics.md       ← 145 Silent-relevant relics
├── silent_relics_index.md
├── silent_relics_by_tag.md
├── silent_archetypes.md   ← Silent archetypes (Poison, Shiv, Discard, Grand Finale, ...)
│
├── defect_cards.md        ← Defect cards (auto-generated; no curated archetypes yet)
├── defect_cards_index.md
├── defect_cards_by_tag.md
├── defect_relics.md
├── defect_relics_index.md
├── defect_relics_by_tag.md
│
├── watcher_cards.md       ← Watcher cards (auto-generated; no curated archetypes yet)
├── watcher_cards_index.md
├── watcher_cards_by_tag.md
├── watcher_relics.md
├── watcher_relics_index.md
├── watcher_relics_by_tag.md
│
├── colorless.md           ← colorless cards (Prismatic Shard, events) — every class
├── curses.md              ← 14 curse cards
├── glossary.md            ← keywords, tag legend, tier shorthand
│
└── data/
    ├── ironclad_cards.json    ← clean per-character card data
    ├── ironclad_relics.json
    ├── silent_cards.json
    ├── silent_relics.json
    ├── defect_cards.json
    ├── defect_relics.json
    ├── watcher_cards.json
    ├── watcher_relics.json
    ├── colorless_cards.json
    ├── curses.json
    ├── index_lookup.json      ← spirespy index → name + character resolver
    ├── _raw_cards_all.json    ← raw scrape (source of truth)
    ├── _raw_relics_all.json   ← raw scrape (source of truth)
    └── _generate.py           ← regenerator
```

## Why this shape

Three principles, applied across the KB:

1. **Single source of truth, multiple views.** Raw JSON in `data/_raw_*.json`,
   cleaned per-character JSON for programmatic queries, Markdown for human
   reading and in-context consultation. All MD is derived from the JSON.
2. **Pre-resolve cross-references.** Synergies in the source data are
   integer indices. They're resolved to names at generation time and filtered
   to the character's pickable scope (no cross-class noise), so a single
   `Grep` on a card name returns everything you need.
3. **Strategy lives next to the data.** The curator's `statement` field
   becomes the **Notes** paragraph on every entry. Tier ratings are
   first-class. `<char>_archetypes.md` ties cards into named deck shapes.

## Lean baseline + drill-down

For every character we generate three "depths" of card and relic views:

- `<char>_cards_index.md` — one line per card. ~5k tokens. Load this whole
  file as your baseline scan.
- `<char>_cards_by_tag.md` — reverse-tag index. "What's every poison card?"
  is one section.
- `<char>_cards.md` — full entry per card with Notes, super-synergies,
  upgrade text. Drill into this only when you need depth.

This matches the Karpathy-style "lean context, tools-on-demand" pattern:
the agent doesn't need the whole 73k-token file in context for every
decision.

## How a consultation looks

1. User posts a screenshot.
2. Claude reads it, **echoes** what's on screen (so misreads can be caught).
3. Claude consults the matching `<char>_*.md` files and the running `status.md`.
4. Claude **recommends one action**, with one-sentence justification and the
   top alternative.
5. After the user acts, `status.md` is updated.

## Regenerating

```
python3 data/_generate.py            # all characters + shared
python3 data/_generate.py silent     # just Silent
python3 data/_generate.py shared     # just colorless / curses / index_lookup
```

The generator reads `data/_raw_*.json` and writes everything else. To pull
fresh data from spirespy, refetch the Nuxt JS bundle (the card/relic arrays
ship inline as `JSON.parse(\`[...]\`)`) and replace the raw files.

## Coverage

- **Card / relic data**: complete for all 4 characters (last scraped 2026-04-20).
- **Archetype guides + boss-relic checklist**: Ironclad and Silent only.
  Defect / Watcher have data but no curated strategy notes — flag that
  in any consultation for those classes.
