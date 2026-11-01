# Ironclad autoresearch — report

One autonomous session, karpathy/autoresearch-style: a frozen harness scores
every change; keep only paired-significant improvements; log everything.

## Headline (held-out `test` split, 500 seeds, A0, never optimised against)

| Agent | Wins | Win rate | 95% Wilson CI | Mean floor |
|---|---|---|---|---|
| **Final best (`hallpot0`)** — leak-free search + learned-by-loop policy | 169/500 | **33.8%** | [29.8%, 38.1%] | 39.3 |
| Mid-run checkpoint (`wev15`, test run 1/5) | 138/500 | 27.6% | [23.9%, 31.7%] | 37.8 |
| Start: leak-free search + lightspeed's legacy out-of-combat policy (`fair0`) | 2/500 | 0.4% | [0.1%, 1.4%] | 19.7 |
| Reference: lightspeed's **cheating** ScumSearchAgent2 @500 sims (`scum_cheat`) | 4/500 | 0.8% | [0.3%, 2.0%] | 22.1 |

"Win" = beating the Act 3 boss (lightspeed has no Act 4 without keys). Test
runs used: 4 of the 5 allowed. Val (300 seeds) for the final agent was 35.7%,
test 33.8% — the expected small winner's-curse shrink.

## Setup

- Simulator: `gamerpuppy/sts_lightspeed` (C++, Ironclad fully implemented),
  built with pybind11 2.13.6; patch + commit pinned in `research/patches/`,
  rebuild with `research/setup.sh`.
- Agent (`research/cpp/`): leak-free determinised MCTS for combat + a
  parameterised out-of-combat policy. Combat budget fixed at 500 simulations
  per decision for every comparison.
- Harness (`research/eval/`, frozen): fork-per-game (simulator crashes become
  logged rows, not dead evals), seed splits dev 100 / val 300 / test 500,
  score = floor/51 + win, paired bootstrap vs current best, keep rule
  Δscore > 0 and P(improve) ≥ 0.9 (`research/exp.sh` applies it automatically).
- Loop instructions, milestones, backlog: `research/program.md`.
  Full log of every run: `research/results.tsv` (68 rows: 11 keeps, 50 discards).

## Leak-free guarantees (and what they cost)

1. **Combat search re-randomises hidden state every simulation**: draw pile
   sorted to canonical order then shuffled with the searcher's own RNG; all six
   BattleContext RNG streams reseeded; `bc.seed` never read; tree nodes keyed
   by an observation hash. A **leak test** (`fair_eval --leaktest`) checks the
   chosen action is identical when hidden state + seed are perturbed with the
   search seed fixed — passed on every build (≈100–140 states each).
2. **Runic Dome is never taken** (boss relic or via Neow's random boss relic):
   the simulator still exposes intents to the search with Dome, so taking it is
   a leak. lightspeed's legacy ordering ranks Dome *first*. Enforcing this cost
   −0.034 score / −3 pts win on val — i.e. earlier numbers were inflated.
3. **Entropic Brew is never used** — it can create non-Ironclad potions
   (e.g. Ambrosia) whose effects lightspeed doesn't implement → crash.
4. Conservative: cards placed on top of the draw pile (Headbutt, Warcry) are
   *also* re-shuffled, so the agent knows slightly less than a human would.

lightspeed's built-in agent is a **cheating reference**, not a baseline: it
searches a copy of the true RNG and replays the found line (save-scumming).
Even so, our honest agent beats it by ~33 points on test, and the cheating
search paired with our final policy only reached 9.6% on val (diagnostic) —
our leak-free MCTS is the stronger combat player at equal budget.

## Keep chain (val, 300 seeds; each step paired vs the previous best)

| Step | Change | Val win | Δscore | P |
|---|---|---|---|---|
| 0 | leak-free MCTS + legacy policy | 0.0% (dev) | — | — |
| 1 | map DP + rest + KB-tier card picks + shop + removal policies | 3.0% | +0.17 (dev) | 1.00 |
| 2 | rollout END_TURN weight 1 → 0.2 | 10.0% | +0.187 | 1.00 |
| 3 | END_TURN weight 0.05 | 14.0% | +0.068 | 0.997 |
| — | Runic Dome guardrail (re-baseline, honesty fix) | 11.0% | −0.034 | — |
| 4 | event policy (rules written from lightspeed's event code) | 13.7% | +0.048 | 0.983 |
| 5 | boss relics ranked by the repo KB's tier list | 18.3% | +0.053 | 0.988 |
| 6 | +2 value for AoE cards when deck has none | 20.3% | +0.046 | 0.990 |
| 7 | +1.5 value for scaling cards when deck has <2 | 23.0% | +0.038 | 0.981 |
| 8 | map: prefer events (w_event 1.5) | 30.0% | +0.094 | 0.987 |
| 9 | combo: scaling 3/want 3, power upgrades, w_event 2, rich-shop 3, hallway fights 0.5 | 32.0% | +0.051 | 0.915 |
| 10 | no potions in hallway-fight search (unless HP < 25%) | 35.7% | +0.049 | 0.934 |

## What did not work (selected; all in results.tsv)

- **"Smarter" rollouts all lost to near-random ones**: expert play-order bias
  (−0.45), block-aware skills (−0.067), attack bias (−0.093), rollout cutoff +
  heuristic eval (−0.08). Exploration constant 0.3/0.5/1.5: noise or worse.
- Tree reuse across decisions (−0.009), compute reallocation to bosses (−0.042).
- Avoiding elites more (−0.045), fewer act-1 elites (−0.053), more hallway
  fights (−0.169), shop potion buying (−0.068), value HP ×2 (−0.064),
  win-bonus 100 (−0.117), block-card bonus (−0.039), Neow ranking (−0.009).
- Combining individually-positive near-misses (P 0.6–0.7) did **not** stack
  (combo4 −0.016) → those were noise, as the keep rule assumed.

## Where it still loses (final agent, test, 331 losses)

Act 1: 10 · Act 1 boss: 39 · Act 2: 62 · **Act 2 boss: 115** · Act 3: 36 · Act 3 boss: 69.

## What I'd do next (ranked)

1. **More combat compute is the clearest remaining lever**: 2× sims (1000)
   gave +0.081 score / +6.3 pts win on val (info-only run, not kept by rule).
   So: make each simulation smarter or cheaper — learned value function at
   rollout cutoff (trained on this harness's own battles), or C++ speedups +
   a wall-clock (not sims) budget.
2. Act 2 boss prep: boss-specific deck checks (Automaton/Champ/Collector).
3. Learned card/path policy from logged trajectories (the 68 runs × 300 seeds
   already form a dataset) instead of hand weights.
4. Ascension ladder per `program.md` milestones once A0 test ≥ 35% (not yet:
   33.8%, CI includes 35%).

## Caveats

- Simulator fidelity: lightspeed only, Ironclad only, A0, no Act 4/Heart.
  Not validated against the real game (CommunicationMod) — results are
  "in lightspeed", not "in Slay the Spire".
- ~50 hypothesis tests on val with a P ≥ 0.9 rule → some keeps are false
  positives; the test split (touched 4×, never tuned on) is the honest number.
- Several thresholds/weights are hand-set priors (KB tiers, event rules) —
  the loop tuned around them, it didn't learn them from scratch.
