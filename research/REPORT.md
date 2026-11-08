# Ironclad autoresearch — report (updated 2026-11-01, month program in progress)

Autonomous research loop on the `sts_lightspeed` simulator: a frozen harness scores every
change on fixed seed splits; only paired-significant improvements are kept; every run is logged
in `research/results.tsv`. Phase 1 (hand-tuned agent) is summarised first, then phase 2
(simulator fixes) and the month program (learning infrastructure, keys/Act 4).

## Headline numbers

| Agent | Split | Win | 95% CI | Floor |
|---|---|---|---|---|
| **Current A0 best `mapd085`** (bfbonus10_g1 + map-path discount 0.85; marginal keep, P=0.913; identical to its predecessor on 100 dev seeds — unreplicated) | val, 300 | **75.0%** | — | 47.4 |
| `bfbonus10_g1` (bfbonus10 with the graded-label model g1 in the bonus, P=0.947) | val, 300 | 73.3% | — | 47.3 |
| **same, test confirmation (M5)** | **test, 500** | **66.6%** | **[62.4%, 70.6%]** | 45.9 |
| learned bonus (g0→g1) vs `potsave2` on a second held-out slice: 72.0% vs 58.0% win | dev2, 100 | +0.171 score, P=0.996 | — | — |
| same on a third held-out slice (dev3): 62.0% vs 53.0% win | dev3, 100 | +0.147 score, P=0.988 | — | — |
| `bfbonus10` (potsave2 + learned interventional deck-value bonus on card picks, P=0.952) | val, 300 | 69.3% | — | 46.8 |
| `potsave2` (wea1p1 + potions saved for boss fights; kept on pooled val+dev 400 seeds, P=0.949) | val, 300 | 66.3% | — | 45.5 |
| `wea1p1` (chests + root move prior + act-1 elite path weight) | val, 300 | 63.7% | — | 45.1 |
| **A10 best `a10_bfbonus25_g1m`** (a10_eh075 + 25× deck-value bonus from the combined A0+A10 model, P=0.950; replicated on dev2: 26.0% vs 18.0%, P=0.991; dev3: 28.0% vs 17.0%, P=1.000) | val, 300, asc 10 | **29.3%** | — | 37.4 |
| `a10_eh075` (rest <60% + KB synergy bonuses + elites only above 75% HP) | val, 300, asc 10 | 27.0% | — | 35.4 |
| **A20 best `a20_restboss1`** (a20_bfb25 + always heal at the pre-boss rest; kept on the pooled val+dev rule, P=0.912, dev alone +0.012) | val, 300, asc 20 | **4.0%** (floor-driven keeps) | — | 28.4 |
| `a20_bfb25` (a20_potb2 + deck-value bonus 25×; P=0.991 on val, −0.027 on 100 dev seeds — both batch-41 A20 keeps are unreplicated) | val, 300, asc 20 | 3.7% | — | 27.8 |
| `a20_potb2` (a20_bfbonus10_g2m + potions spent freely in boss fights; P=0.998 on val, −0.002 on dev) | val, 300, asc 20 | 1.7% | — | 27.0 |
| `a20_bfbonus10_g2m` (a20_restlo + 10× deck-value bonus from the three-ascension model; pooled val+dev keep, P=0.935; second slice +0.007, P=0.608 — unreplicated) | val, 300, asc 20 | 1.0% | — | 26.6 |
| `a20_restlo` (elite gate 80% HP, act-1 elites avoided, rest valued higher) | val, 300, asc 20 | 1.3% | — | 25.6 |
| `base_a20` (A0 params at ascension 20, untuned) | val, 300, asc 20 | 2.3% | — | 21.8 |
| **Heart best `heart_bfrelic10`** (heart_ka3 + boss relics chosen by KB tier value + 10× simulated next-act fights; pooled-500 keep 12.0% vs 10.4% Heart wins, P=0.986; dev replication flat on wins) | val, 300, keys | **13.3%** | — | 46.8 |
| `heart_ka3` (heart_wba3 + the ruby key also taken at act-3 rest sites; extension keep on the pooled 600, P=0.949) | val, 300, keys | 11.3% | — | 46.5 |
| `heart_wba3` (heart_a4pot + the act-3 burning elite worth +3 on the map while the emerald key is missing; extension keep on the pooled 600, P=0.958) | val, 300, keys | 11.0% | — | 46.4 |
| `heart_a4pot` (heart_bfbonus25_g_heart + spare gold buys potions in the act-4 shop; pooled-500 keep P=0.996, dev replication +3 Heart wins P=0.953) | val, 300, keys | 10.7% | — | 46.3 |
| `heart_bfbonus25_g_heart` (keys + 25× bonus from the Heart-labelled model 6; win = Heart beaten) | val, 300, keys | 9.3% | — | 46.3 |
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

## Keys and Act 4

With `keys=1 chest_pol=1` on the chest-era agent (val, 300 seeds): 35.0% of runs collected all three
keys and reached Act 4, and **6.3% beat the Heart (19/300; 18% conversion from Act 4)**. The current
Heart best (`heart_bfbonus25_g_heart`: the A0 chain with keys plus a 25× bonus from a model whose act-3
labels are fights against the Heart itself) reaches Act 4 in 140/300 runs and **beats the Heart in 28/300
(9.3%, 20% conversion; +0.052 score vs the previous Heart best, P=0.989)**. On 100 independent dev seeds
the same agent won 4/100 vs 9/100 for its predecessor (score −0.034, P=0.20); on a second slice (dev2) it
scored +0.054 (P=0.93) with Heart wins 9/100 vs 8/100, and on a third (dev3) +0.047 (P=0.98) with Heart
wins equal. Floors replicate on two of three slices and Heart wins are flat on all three, so this keep is
reported as "floor-replicated, Heart wins unchanged"; Heart conversion
remains the weakest, noisiest track (a 2–3-point Heart-win difference is one or two runs). Because of
that noise, Heart (and A20) keep decisions moved on 2026-11-02 to a pre-registered pooled rule (val 300 +
dev2 100 + dev3 100, same paired bootstrap), with dev as the untouched replication slice. Its first keep is
`heart_a4pot`: spare gold buys potions in the act-4 shop (the last shop, so gold has no other use), which is
a pure conversion gain — floors unchanged, Heart wins +4/300 on val, +3/100 on dev2, +1/100 on dev3
(pooled 9.8% vs 8.2%, P=0.996) and +3/100 on the replication slice (P=0.953): +11 Heart wins on 600 seeds.
A replay of the val games also showed where the next Heart wins are: 37 of 300 runs (14% over 600 seeds)
beat the act-3 boss without all three keys — the emerald key (burning elite) missing in 22, the ruby key
(rest-site Recall) in 17 — and never reach the Heart. The first key-acquisition keep, `heart_wba3`, values
the act-3 burning elite +3 while the emerald key is missing: key-less clears 29 vs 37 on val, act-4 runs
147 vs 140, pooled-600 Heart wins 9.8% vs 9.3% (P=0.958, kept under the pre-registered near-miss extension,
so it has no untouched replication slice). Two more keeps followed: `heart_ka3` (the ruby key also taken at act-3
rest sites; extension keep, pooled 600 10.2% vs 9.8%, P=0.949; key-less clears 37 → 18 of 300 by then) and
`heart_bfrelic10`, the first pure conversion mechanism: each offered boss relic is scored by the knowledge-base tier
plus 10× the gain in two simulated next-act fights with the relic added (for the act-3 relic: Shield & Spear and the
Heart), pooled 500 Heart wins 12.0% vs 10.4% (P=0.986), act-4 reach unchanged; its dev replication is flat on wins
(+0.005, P=0.603), so it is reported as "pooled keep, dev flat". The same mechanism is flat at A0 and A10 and a
failed near-miss at A20: it helps only where the next fights are known. Pooled Heart wins over the night: 8.2% → 12.0%. (An earlier draft of this report
said 16.3% / 47% — that mistakenly counted key-less Act-3 clears as wins; the harness now defines a
Heart-track win as `act ≥ 4 && win`.) ~90% of Act-4 deaths are at the Heart itself, with winning runs
finishing on ~28 HP, so Act-4 conversion is a damage-race problem. Tracked separately from the A0
Act-3 metric; it makes the month's A20-Heart target measurable.

## Next (month program, `research/program.md`)

**What the learned strategic layer turned out to be.** Replacing the hand picker with the interventional
deck-value model lost 6–16 points on every ladder; the same model used as a small additive adjustment on
top of the KB card values is the day's best component: +3.0 points on A0 with the first model, +4.0 more
with graded labels (damage fraction against the act boss and a next-act boss), +2.3 on A10 once the model
had seen A10 fights, +1 floor on A20. Shop purchases, rest upgrades and removals stay with the hand rules
(the bonus is neutral or negative there). Closed with evidence: pooled-ascension models dilute (each ladder
wants a model that saw its own fights plus A0's), ladder-only models are too small, and training on the
*current* agent's games is worse than on the previous agent's (A0 −0.019, A10 −0.060) — expert iteration
of the strategic model does not work in this form.

**Running / queued:** labels averaged over three fights per option (label-noise reduction), then a
re-tune of the map weights on the current bests (last tuned on a much weaker agent). Open problems: A20
act-1/act-2 boss survival (deck strength), Heart conversion (floors move, Heart wins do not), and the
7-point val-to-test gap on A0.

## Caveats

Results are "in lightspeed" (Ironclad, A0, no Act 4 in the main metric), never validated against
the real game. ~190 val tests with a P ≥ 0.9 rule → some keeps are false positives; the test split
(touched 6 times, never tuned on) is the honest number: the current A0 best reads 73.3% on val and
66.6% on test, a 7-point winner's-curse gap accumulated over four val-selected keeps (the chest-fix agent
had a 3-point gap). Near-misses are now extended on fresh dev seeds under a pre-registered rule, and every
track best is re-checked on independent seeds before it is reported. KB tiers and event rules are hand-written
priors; the loop tuned around them.

## Phase S — scaling the learned combat value (2026-11-02, interim)

Question: does a learned leaf value, scaled in data and parameters, overtake random rollouts at equal wall-clock
on this 4-core CPU? Data: 2,500 games of the A0 best with the policy/root-value logger (8.1×10⁵ decisions, target =
the search's root mean value). Models: 204 → h → h → 1, h = 42 / 220 / 850 (10⁴ / 9.4×10⁴ / 9×10⁵ parameters), data
10⁴ / 10⁵ / 8.1×10⁵ as nested subsets. Fit (holdout RMSE, predict-mean 33.6): 16.6 / 13.8 / 13.3 → 11.8 / 10.7 / 9.9
→ 10.2 / 8.0 / 5.65 — monotone in both directions, steepest at the largest cell. Play (model replaces rollouts, equal
wall-clock, val 300 paired vs the A0 best; informational rows, nothing promoted):

| cell | decisions | params | sims at equal wall-clock | win | floor | dscore vs mapd085 |
|---|---|---|---|---|---|---|
| sv1 | 10⁴ | 1.0×10⁴ | 1250 | 6.7% | 24.86 | -1.1246 |
| sv2 | 10⁴ | 9.4×10⁴ | 156 | 26.0% | 35.76 | -0.7176 |
| sv3 | 10⁴ | 9.0×10⁵ | pending | | | |
| sv4 | 10⁵ | 1.0×10⁴ | 1036 | 34.3% | 38.28 | -0.5848 |
| sv5 | 10⁵ | 9.4×10⁴ | 150 | 26.7% | 36.56 | -0.6952 |
| sv6 | 10⁵ | 9.0×10⁵ | pending | | | |
| sv7 | 8.1×10⁵ | 1.0×10⁴ | 1132 | 36.0% | 38.42 | -0.5653 |
| sv8 | 8.1×10⁵ | 9.4×10⁴ | 108 | 29.7% | 38.13 | -0.6344 |
| sv9 | 8.1×10⁵ | 9.0×10⁵ | 50 (6× budget floor) | 27.0% | 37.90 | -0.6655 |
| control (heuristic leaf) | — | — | 3423 | 11.7% | 30.97 | -0.9548 |
| base `mapd085` (random rollouts) | — | — | 500 | 75.0% | 47.36 | 0 |

Interim conclusion: the learned value is real (≈ +0.38 score over a heuristic leaf at equal compute) and keeps
improving with scale in fit, but at equal wall-clock on a CPU every cell is far below random rollouts, because a
10⁵–10⁶-parameter leaf evaluation costs more than a rollout and the search loses simulations faster than the leaf
gains accuracy. Fixed-simulation reads settled it: with inference cost removed, the best-fitting net (9×10⁵ parameters, 8.1×10⁵
decisions) at 500 simulations scores −0.57 on the first 100 val seeds — the best leaf, but still far below random
rollouts (the agent wins 35.7% vs 77.0% on those seeds). Phase S is closed by its pre-registered rule; a faster
evaluator would recover only the inference penalty, not the gap. Phase S2 tests training on the states the search
actually evaluates.
