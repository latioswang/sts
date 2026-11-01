# Ironclad autoresearch — report (updated 2026-11-01, month program in progress)

Autonomous research loop on the `sts_lightspeed` simulator: a frozen harness scores every
change on fixed seed splits; only paired-significant improvements are kept; every run is logged
in `research/results.tsv`. Phase 1 (hand-tuned agent) is summarised first, then phase 2
(simulator fixes) and the month program (learning infrastructure, keys/Act 4).

## Headline numbers

| Agent | Split | Win | 95% CI | Floor |
|---|---|---|---|---|
| **Current best `wea1p1`** (chests + root move prior + act-1 elite path weight) | val, 300 | **63.7%** | — | 45.1 |
| `priormlp_d0` (chests + learned move prior at the root) | val, 300 | 61.3% | — | 44.6 |
| `chest1` (fixed sim, chests opened) | val, 300 | 57.7% | — | 44.2 |
| **same, test confirmation** | **test, 500** | **54.6%** | **[50.2%, 58.9%]** | 43.7 |
| `rb3` — fixed simulator, before the chest fix | val, 300 | 38.0% | — | 40.5 |
| Phase-1 final `hallpot0` (old sim) | test, 500 | 33.8% | [29.8%, 38.1%] | 39.3 |
| Start: leak-free search + lightspeed legacy policy | test, 500 | 0.4% | [0.1%, 1.4%] | 19.7 |
| Reference: lightspeed's **cheating** ScumSearchAgent2 @500 sims | test, 500 | 0.8% | [0.3%, 2.0%] | 22.1 |

"Win" = beating the Act 3 boss at Ascension 0 (keys/Act 4 are a separate track). Compute per
comparison is fixed at the wall-clock of the original 500-simulation search.

## What the agent is

- **Combat:** online MCTS, uniformly random rollouts to the end of the fight, hand-written
  terminal value (win bonus + HP + potions). Not learned. Leak-free: hidden state is
  re-randomised every simulation (draw order, all RNG streams, intents under Runic Dome);
  a leak test (`fair_eval --leaktest`, `--leak_dome`) gates every build.
- **Out of combat:** parametric rules (map DP, card picks from the repo's KB tiers plus deck-need
  bonuses, shop, rest, events, boss relics, keys, chests). Not learned.
- **Loop:** hypothesis → `research/exp.sh` (val, paired bootstrap vs best, keep if P ≥ 0.9) →
  auto-promote → log. 85+ experiments so far; ~15 keeps.

## Phase 2: simulator holes filled (not routed around)

Root causes fixed in lightspeed (all `// FIX` in `research/patches/sts_lightspeed.patch`):
`BattleContext::player.cc` never initialised (Entropic Brew / Dead Branch drew from a garbage
class pool — the earlier "Ambrosia crash"); Blood Potion heal inverted; Iron Wave double
Dexterity; Dark Shackles *adding* Strength and Shackled inverted; Disarm+ −2; Impatience always
drawing; Smoke Bomb a no-op; Toy Ornithopter missing; Designer In-Spire switch fall-through and
free "Adjustments"; Mushrooms/Mysterious Sphere +99 gold; Mausoleum no relic; Woman in Blue
free; Scrap Ooze non-escalating; Colosseum stubbed and disabled → implemented. Agent-side: Runic
Dome handled honestly (intents hidden in the observation hash; pending move re-sampled only when
it came from an RNG roll — re-rolling scripted moves double-spawned the Automaton's orbs);
known top-of-pile cards (Headbutt/Warcry) kept in determinisation; Entropic Brew and Dome un-banned.
Re-baseline on the fixed sim: 38.0% val (from 35.7% on the buggy sim).

## The big one: treasure chests

lightspeed's treasure-room action is `0 = open, else = leave`; its legacy agent passed `true`
(=1) meaning "take", so it never opened a chest — and our agent inherited that handler. Every
run before `chest1` skipped all chest relics (and the sapphire key). Fixing it: **+19.7 points of
win rate (38.0% → 57.7%, P = 1.000)**, the largest single gain in the program. Found while
implementing the keys/Act 4 track, not by reasoning about win rate — a reminder that fidelity
audits of the *agent's* environment interface pay off as much as simulator audits.

## Learning infrastructure (Phase A) and first learned components (Phases B/C)

- Observable-only feature extractors: combat state (204 dims), run state (666 dims), action
  tokens (434) + target features; per-decision loggers (`vf_log`, `rv_log`, `pf_log`), torch
  trainers with grouped holdouts and non-finite/base-rate guards, weight export to `.inc`,
  C++ inference (MLP / bilinear), integration points: leaf value at rollout cutoff, PUCT prior,
  one-step-lookahead picks with V_run.
- Three data-integrity bugs were caught by the validation asserts before any model was trusted:
  child stderr reused as the log fd (cerr corrupted binary logs); the value loader mixing other
  loggers' files; `getStatus` throwing on a bit-only status (14.6% of logging games aborted).
  Datasets were regenerated each time (1,000 games, 0 crashes final).
- Results so far: outcome-regression leaf V — good predictor (RMSE 12 vs 28), **catastrophic as a
  leaf** (rows of a battle share one label → no within-turn signal → passivity). Bilinear move
  prior — neutral (−0.004 at equal compute; captures ~31% of the available information).
  V_run — val log-loss 0.561 vs 0.693 base after regularisation; pick experiments running.
  Bootstrapped V (targets = search root values): RMSE 9.7 vs 33.9 — sanity test running.

- **First learned component kept:** the MLP move prior (49% of the available information captured)
  used as a PUCT prior at the root only: **61.3% vs 57.7% val (+0.044 score, P = 0.90)** at ~zero
  extra cost. Using it deeper in the tree adds nothing at equal wall-clock; lower exploration
  constants look better (follow-up batch running). Expert-iteration step 2 (distilling from
  2,000-simulation self-play searches of the new best agent) is in progress.

## What did not work (selected)

Fixed per-decision *action* budgets (starve long fights, −0.065); every rollout cutoff with a
heuristic leaf (−0.14 to −0.37); "smart" rollout biases (−0.07 to −0.45); strong low-HP risk
aversion (−0.31); tree reuse, exploration constants, potion/HP value tweaks, Neow ranking,
elite avoidance, shop potions (all ≤ noise or negative). Fixed-size card piles for speed were
reverted after causing stack corruption in degenerate rollouts (3/300 crashes).

## Where it still loses (chest1, val)

127 losses (computed from the run file): Act 1 hallway 4 · Act 1 boss 20 · Act 2 hallway 13 ·
**Act 2 boss 35** · Act 3 hallway 20 · **Act 3 boss 35** — bosses account for 71% of losses, so
boss-specific preparation (deck checks keyed on the known boss) is the obvious next strategic lever.

## Keys and Act 4 (first results)

With `keys=1 chest_pol=1` (val, 300 seeds): 35.0% of runs collect all three keys and reach Act 4,
and **6.3% beat the Heart (19/300; 18% conversion from Act 4)**. (An earlier draft of this report
said 16.3% / 47% — that mistakenly counted key-less Act-3 clears as wins; the harness now defines a
Heart-track win as `act ≥ 4 && win`.) ~90% of Act-4 deaths are at the Heart itself, with winning runs
finishing on ~28 HP, so Act-4 conversion is a damage-race problem. Tracked separately from the A0
Act-3 metric; it makes the month's A20-Heart target measurable.

## Next (month program, `research/program.md`)

Expert-iteration step 2: a 2,000-simulation self-play dataset from the new best agent → sharper
move-prior targets → retrain → equal-compute test; PUCT exploration-constant sweep with the root
prior (batch 21); boss-aware deck preparation on the A0, Heart and A10 tracks (batch 20); A10 track
baseline (seeded from the A0 best) and then A20; Heart-track work shifts from "reach Act 4 more
often" (easy) to converting Act 4 (deck strength). Continuous fidelity fuzzing with the trace build.

## Caveats

Results are "in lightspeed" (Ironclad, A0, no Act 4 in the main metric), never validated against
the real game. ~90 val tests with a P ≥ 0.9 rule → some keeps are false positives; the test split
(touched 5 times, never tuned on) is the honest number. KB tiers and event rules are hand-written
priors; the loop tuned around them.
