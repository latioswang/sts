# Ironclad autoresearch — report (updated 2026-11-01, month program in progress)

Autonomous research loop on the `sts_lightspeed` simulator: a frozen harness scores every
change on fixed seed splits; only paired-significant improvements are kept; every run is logged
in `research/results.tsv`. Phase 1 (hand-tuned agent) is summarised first, then phase 2
(simulator fixes) and the month program (learning infrastructure, keys/Act 4).

## Headline numbers

| Agent | Split | Win | 95% CI | Floor |
|---|---|---|---|---|
| **Current A0 best `bfbonus10_g1`** (bfbonus10 with the graded-label model g1 in the bonus, P=0.947) | val, 300 | **73.3%** | — | 47.3 |
| **same, test confirmation (M5)** | **test, 500** | **66.6%** | **[62.4%, 70.6%]** | 45.9 |
| `bfbonus10` (potsave2 + learned interventional deck-value bonus on card picks, P=0.952) | val, 300 | 69.3% | — | 46.8 |
| `potsave2` (wea1p1 + potions saved for boss fights; kept on pooled val+dev 400 seeds, P=0.949) | val, 300 | 66.3% | — | 45.5 |
| `wea1p1` (chests + root move prior + act-1 elite path weight) | val, 300 | 63.7% | — | 45.1 |
| **A10 best `a10_bfbonus25_g1m`** (a10_eh075 + 25× deck-value bonus from the combined A0+A10 model, P=0.950) | val, 300, asc 10 | **29.3%** | — | 37.4 |
| `a10_eh075` (rest <60% + KB synergy bonuses + elites only above 75% HP) | val, 300, asc 10 | 27.0% | — | 35.4 |
| **A20 best `a20_bfbonus10_g2m`** (a20_restlo + 10× deck-value bonus from the three-ascension model; pooled val+dev keep, P=0.935) | val, 300, asc 20 | **1.0%** (floor-driven keeps) | — | 26.6 |
| `a20_restlo` (elite gate 80% HP, act-1 elites avoided, rest valued higher) | val, 300, asc 20 | 1.3% | — | 25.6 |
| `base_a20` (A0 params at ascension 20, untuned) | val, 300, asc 20 | 2.3% | — | 21.8 |
| **Heart best `heart_bfbonus25_g_heart`** (keys + 25× bonus from the Heart-labelled model 6; win = Heart beaten) | val, 300, keys | **9.3%** | — | 46.3 |
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
  auto-promote → log. 140 logged runs so far; 31 keeps/track bests.

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

## Where it still loses (computed from the run files)

A0 `wea1p1`: 109 losses — Act 1 hallway 4 · Act 1 boss 13 · Act 2 hallway 18 ·
**Act 2 boss 25** · Act 3 hallway 25 · **Act 3 boss 24** — bosses are 57% of losses
(71% before the move prior; the prior helped most in boss fights). A10 `a10_kbsyn`: 53 of 227 losses
(18% of all runs) end at the Act 2 boss. A20 `base_a20`: 160 of 300 runs
(53%) die in Act 1, 89 of them in hallway/elite fights before the boss — the
A0-tuned map policy walks a starter deck into A20 elites. The 4×-compute probe (+4.5 pts) says combat
search is not the main bottleneck at A0; the deck is, which is what Phase C v3 attacks.

## Keys and Act 4 (first results)

With `keys=1 chest_pol=1` (val, 300 seeds): 35.0% of runs collect all three keys and reach Act 4,
and **6.3% beat the Heart (19/300; 18% conversion from Act 4)**. (An earlier draft of this report
said 16.3% / 47% — that mistakenly counted key-less Act-3 clears as wins; the harness now defines a
Heart-track win as `act ≥ 4 && win`.) ~90% of Act-4 deaths are at the Heart itself, with winning runs
finishing on ~28 HP, so Act-4 conversion is a damage-race problem. Tracked separately from the A0
Act-3 metric; it makes the month's A20-Heart target measurable.

## Next (month program, `research/program.md`)

**Phase C v3 — interventional deck value (in the queue).** Observational run values (V_run) and
online counterfactual fights (cf_pick) both failed; v3 plays the fights offline: at every card
reward / upgrade / removal of the best agent's own games, the simulator plays the act boss and a
sampled hallway fight from the current state and from each hypothetical option (same fight seeds
per decision), a 666→128→4 MLP is trained on absolute and paired labels, and card picks, upgrades
and removals become argmax of the predicted fight value. Also queued: prior-guided rollouts at
calibrated equal wall-clock, A20 act-1 survival (elite avoidance), A10 near-miss re-tests, KB
synergy transfer to A20. Tooling: a sequential experiment queue (`research/tools/runq.sh`) and a
mechanised bit-identity check after every rebuild (`research/tools/repro_check.sh`).

## Caveats

Results are "in lightspeed" (Ironclad, A0, no Act 4 in the main metric), never validated against
the real game. ~190 val tests with a P ≥ 0.9 rule → some keeps are false positives; the test split
(touched 6 times, never tuned on) is the honest number: the current A0 best reads 73.3% on val and
66.6% on test, a 7-point winner's-curse gap accumulated over four val-selected keeps (the chest-fix agent
had a 3-point gap). Near-misses are now extended on fresh dev seeds under a pre-registered rule, and every
track best is re-checked on independent seeds before it is reported. KB tiers and event rules are hand-written
priors; the loop tuned around them.
