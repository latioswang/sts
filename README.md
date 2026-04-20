# Ironclad strategy KB

A structured knowledge base for **Slay the Spire — Ironclad**, scraped from
[spirespy.maybelatergames.co.uk](https://maybelatergames.co.uk/spirespy/), and
arranged for **real-time consultation during a run**.

The user (player) sends screenshots; Claude reads the screen, looks things up
here, and recommends the next action. See [`CLAUDE.md`](CLAUDE.md) for the
exact workflow.

## Files

```
.
├── CLAUDE.md              ← consultation workflow (read this first if you're Claude)
├── status.md              ← living state of the current run
│
├── cards.md               ← 73 Ironclad cards by tier, with notes & synergies
├── relics.md              ← 146 Ironclad-relevant relics by tier
├── colorless.md           ← colorless cards (Prismatic Shard, events)
├── curses.md              ← 14 curse cards
├── archetypes.md          ← deck themes, payoffs/enablers, boss-relic checklist
├── glossary.md            ← keywords, tag legend, tier shorthand
│
└── data/
    ├── ironclad_cards.json    ← clean structured card data
    ├── ironclad_relics.json   ← clean structured relic data
    ├── colorless_cards.json
    ├── curses.json
    ├── index_lookup.json      ← spirespy index → name resolver
    └── _raw_*.json            ← raw scrape (source of truth)
```

## Why this shape

Three principles, applied across the KB:

1. **Single source of truth, multiple views.** Raw JSON in `data/_raw_*.json`,
   cleaned JSON for programmatic queries, Markdown for human reading and
   in-context consultation. All MD is derived from the JSON.
2. **Pre-resolve cross-references.** Synergies in the source data are
   integer indices. They're resolved to names in the cleaned JSON and the
   Markdown, so a single `Grep` on a card name returns everything you need.
3. **Strategy lives next to the data.** The curator's `statement` field
   becomes the **Notes** paragraph on every entry. Tier ratings are
   first-class. `archetypes.md` ties cards into named deck shapes.

## How a consultation looks

1. User posts a screenshot.
2. Claude reads it, **echoes** what's on screen (so misreads can be caught).
3. Claude consults `cards.md` / `relics.md` / `archetypes.md` and the running
   `status.md`.
4. Claude **recommends one action**, with one-sentence justification and the
   top alternative.
5. After the user acts, `status.md` is updated.

## Data freshness

Scraped on 2026-04-20 from spirespy. The Nuxt JS bundle ships card/relic data
inline as `JSON.parse(\`[...]\`)` in `_nuxt/Dy5sGjr5.js`; that's the source of
truth for both arrays. To re-scrape, fetch the same chunk and re-run the
generator (see commit history).
