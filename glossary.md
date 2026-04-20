# Glossary — keywords, tags, and shorthand

Quick reference for terms that appear across `cards.md`, `relics.md`, and the
JSON data. If you see an unfamiliar word in a card text, look it up here first.

## Damage / defence

- **Block** — absorbs damage this turn. Lost at the start of your next turn
  (unless you have Barricade / Calipers / Blue Candle exceptions).
- **Strength** — flat bonus damage per attack hit. Stacks; +1 Str = +1 damage
  to every hit, every multi-hit.
- **Dexterity** — flat bonus block per Block-granting card.
- **Thorns** — when an enemy attacks you, they take this much damage.
- **Plated Armor** — like Block but persistent; loses 1 stack each time you
  take unblocked attack damage.
- **Intangible** — incoming damage of any kind is reduced to 1 this turn.
- **Metallicize** — gain N Block at end of every turn.

## Debuffs (apply to enemies, or rarely yourself)

- **Vulnerable** — target takes 50% more attack damage. Default 1 turn / stack.
- **Weak** — target deals 25% less attack damage. 1 turn / stack.
- **Frail** — target gains 25% less Block. 1 turn / stack.
- **Poison** — at the start of each enemy turn, lose this much HP, then -1.
- **Strength-Down** — at end of turn, lose this much Strength.

## Card-flow keywords

- **Exhaust** — card is removed from this combat (returns next combat).
- **Ethereal** — if in hand at end of turn, exhausts itself.
- **Innate** — starts in your opening hand.
- **Retain** — does not discard at end of turn.
- **Unplayable** — cannot be played from hand (e.g. Wounds, Burns).

## Tag legend (used in `_tags_` line of each card/relic)

Tags are spirespy's machine-readable categorisation. They drive the synergy graph.

| Tag | Meaning |
|---|---|
| `aoe` | hits all enemies |
| `block` | grants Block |
| `dblock` | grants Block conditionally / scaling |
| `draw` | draws cards |
| `drawblock` | draw + something restrictive (e.g. Battle Trance) |
| `discard` | discards from hand |
| `exhaust` | exhausts itself when played |
| `exhaustother` | exhausts other cards |
| `sacrifice` | exhausts a card from hand for an effect (e.g. Feed) |
| `multihit` | hits multiple times |
| `vulnerable` | applies Vulnerable |
| `weak` | applies Weak |
| `strength` | grants Strength |
| `strengthdown` | grants Strength-Down (or removes Str) |
| `dexterity` | grants Dexterity |
| `heal` | heals HP |
| `hpgain` | raises max HP |
| `managain` | grants Energy |
| `create` | creates / adds cards (e.g. Anger, Havoc) |
| `pollute` | clutters deck (e.g. Wild Strike adds Wound) |
| `ethereal` | self-exhausts at end of turn |
| `innate` | starts in opening hand |
| `retain` | survives discard |
| `shuffle` | triggers a shuffle |
| `upgrade` | upgrades cards (Armaments, Apotheosis) |
| `thorns` | grants Thorns |
| `intangible` | grants Intangible |
| `platedarmor` | grants Plated Armor |
| `artifact` | grants Artifact charges (negates next debuff) |
| `buffer` | prevents next HP loss (Fossilized Helix) |
| `slotgain` | adds potion slots |
| `gold` | grants/affects gold |
| `endturn` | triggers on end of turn |

Most other tags are character-specific (Silent: poison/discard, Defect:
channel/lightning/frost/dark/plasma/focus, Watcher: stance/wrath/calm/scry/
mantra/divinity).

## Tier shorthand

`S+ S S- A+ A A- B+ B B- C+ C C- D+ D D- E+` — descending. Anything ≥ B is
generally a confident pick, A/S is auto-pick, D/E is usually skip unless it
fits a very specific build.

## Energy notation

`cost 2` means 2 Energy. `cost 1→0` means 1 base, 0 after upgrade. `X` means
spends all Energy.

## "Statement" field

The `**Notes.**` paragraph on each entry is the spirespy curator's strategic
commentary. It is opinionated but well-informed — treat it as a strong prior,
not gospel.
