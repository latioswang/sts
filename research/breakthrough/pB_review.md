# pB review — "combat engine first"

Nothing was built or run. [estimate] = my number, [computed] = my arithmetic on cited numbers, [inferred] = my reading. `00` = `00_internal_map.md`; `FS.cpp` = research/cpp/FairSearch.cpp; `BC.cpp` = sts_lightspeed/src/combat/BattleContext.cpp.

## Verdict in five lines

1. Do not approve as the main line. Approve its diagnostics and the no-learning skeleton (P0 reduced, P1) as components of another plan.
2. The thesis is contradicted by the best fair outside agent (21.9% at A20 with no combat search at a similar budget, `s6` §4) and by the project's own Heart data.
3. Play-time cost is understated about twofold [estimate]: at honest cost the new engine runs about 400 simulations, not 700.
4. The plan's own "go" level of pilot headroom is worth 13–15% at A20 [computed], half of T1; the rest is strategy work it postpones to day 41.
5. Two gates cannot fail as written (P0 go, P2 kill), and one go rule would have passed the F52 mix the owner closed.

## Fatal or near-fatal flaws

**F1. The order of work contradicts where the measured gap is.**
- spire-engine reaches 93.1 / 75.2 / 21.9% with one greedy network pass and no search; "the 3 ms limit does not explain the gap" (`s6` §4; self-reported, own engine).
- Spire Mentor's chance-node search at 16–64 times this project's budget (`w1` §1.1) scored 16.8% at StS2 A10 in an independent host, against 15.6% for a policy with no search (`s6` §1b).
- Internally, Heart conversion rose from about 26% to about 50% when the Heart-labelled V arrived (`00` §7.8); the Heart-fight loss rate stayed at 47–49% under 2× and 4× search (`d1` §3).
- The plan takes 0.13 of its 0.28 for T1 from "the adopted strategy training of P4": the rival mechanism, started after 40 days and given 15.

**F2. Passing P0 at its threshold does not reach T1.** With the stage rates of `d1` §2, boss deaths cut 25% give 12.7% at A20; adding 15% fewer non-boss deaths gives 14.8% [computed]. That assumes a 64–1,000× pilot is fully amortised into 1×. T1 needs 40% at every stage (plan §5; checked, 24.6%).

**F3. The evaluator is the object that failed in closed loop elsewhere, placed where it is most exposed.**
- P(win) plus an HP distribution is a fight-outcome surrogate. Mentor's had good per-fight error and predicted survival 0.64–0.68 against a true 0.29, "because death is a tail event" (`s4` §1). Mentor also has learned leaf nets and does not deploy them (same table).
- Here, cut-offs lost more the shorter they were: −0.17 / −0.28 / −0.37 at 6 / 3 / 2 turns (`00` §2). The plan cuts at zero turns.
- The F52 nets explained 87% of target variance (`h1` §2) [computed] and still lost 0.30: the within-decision signal sits in the residual.
- Hallway fights end in death 0.5–3.5% of the time (`d1` §4), so the label is nearly constant exactly where attrition is claimed.

**F4. The "two-turn expectimax" is statistically empty.** 700 simulations × 30% = 210 over 3–8 afterstates; widening leaves about 7 draw children with about 7 visits each over 8–10 legal actions: under one visit per second-turn action [computed]. `w2` A2: widening over effectively unique outcomes is "suboptimal regardless of the computation time"; the one measured gain used bucketing, at 5 s per move. The plan has none.

## Corrections (novelty, code, arithmetic)

**Novelty.**
- Confirmed never tried (`00` §3 a, c, f, g, o, q): fight end in run-value units, entity inputs, draw nodes, paired root with sequential halving, privileged critic, SPSA.
- The plan's table mislabels Phase S as an 8-rollout target. Phase B and Phase S regressed the search root mean on up to 10⁶ decisions (−0.565, `00` §2); the plan's TD(λ) blend "with that agent's later-turn search values" reuses part of that closed target, and in round 0 those values are random-rollout means.
- Prior below the root lost at equal wall-clock (−0.010) and at +40% compute (−0.009) (`00` §3 d). "Different this time" rests on a 0.5 µs expansion that does not hold (below).
- Sequential halving replaces the visit-lead early stop (`FS.cpp:562-576`), a confirmed +0.016 to +0.032 per track (`00` §4). Not mentioned.

**Code.**
- `BC.cpp:3684-3774`: `afterMonsterTurns()` is not a pre-draw stop. It queues start-of-turn relics and powers and only queues the draw (`:3754`), with post-draw triggers behind it (`:3766-3767`). The draw runs later in `executeActions` (`:961-965`) and may queue a reshuffle and a second draw (`:4023-4031`). A stop needs a sentinel in the queue; revisiting a draw child needs re-sampling that covers reshuffles and known top cards.
- Canonical sorted-hand keys are not a key change. Edges store a hand index (`FS.cpp:204`), and duplicates merge only when adjacent (`FS.cpp:188-194`), so the edge list depends on hand order. Merging needs identity-keyed actions, per-simulation remapping and a new enumeration: a rewrite of the 595-line searcher.
- No state serialiser exists (the action queue holds function pointers, `ActionQueue.h:27-52`). In-process forks exist (`FairAgent.cpp:422-446`), so a bank of (seed, battle index) is cheap, but it is "frozen" only while agent and simulator stay bit-identical, which P1 ends.
- Confirmed: node-mean quirk (`FS.cpp:521`, `:535`), agent seed tied to run seed (`eval_main.cpp:51`), unconditional `determinise` (`FS.cpp:470`).
- Effort [estimate]: P1 and P2 are about twofold low; P4 in 15 days is not credible (three models regenerated plus search-in-the-loop training, which took Mentor 36–62 hours per leg on 24 cores, `w1` §1.1).

**Arithmetic.**
- Evaluation cost. The 1.5 µs of `s5` §2 is an incrementally updated accumulator in a micro-benchmark, without feature emission. Here the simulator emits no deltas, so each evaluation is emission plus a full refresh (the plan's own "30 KB touched"). Today's 204-feature extraction costs about 1.3 µs (2.7 µs per `sv` call, `c1` §4, minus 1.4 µs for the MLP, `s5` §2) [inferred]. So evaluation ≈ 3 µs, and priors at an expanded node ≈ 2 µs, since they need that node's own tail [estimate]. One-turn simulation ≈ 7 µs, two-turn ≈ 13, mix ≈ 8.8: about 400 simulations in 3.5 ms, against the claimed 700 and today's 400–520.
- E1 cost. The nested pilot is about 330 times a boss decision at boss playout lengths [estimate]: 16k boss fights ≈ 290 CPU-h, Heart bank ≈ 165, the 1–64× curve (127× in total) ≈ 190. About 650 CPU-h, not 350; still under one fleet-day (960).
- Ranking loss. The 4,000-decision test set costs 70 CPU-h; 100k training decisions would cost 1,750, above all of P2 (1,200). The loss that answers "passivity" is unfunded.
- U in [0, 1]. A20 run-win probability is about 0.08, so sibling differences are 0.001–0.01 [inferred]; without normalisation PUCT becomes prior-driven.

## Evidence against the thesis

- The same estimator does not flatten at A10 (48 / 53 / 58%) and does at A20 (6.0 / 8.5 / 9.3%) (`d1` §3). "Bias, not variance" predicts both flatten; "deck-limited" predicts what is seen [inferred].
- 25% of runs entering the act-2 boss at 85% HP or more die there (18 of 71); winners hold 10.2 against 8.4 relics and 3.7 against 2.35 removals (`d1` §4).
- Outside, 58–72% of final-boss losses could not be won by replay (`w1` §1.2). Learned leaves, guided PUCT and distillation failed here, six ways in sts-rl-agent, and in two more projects (`w1` dead ends).
- Evidence 4 dates from the 3–14% A0 era (`00` §7.5). Evidence 6 is analogy: 2048 and chess have no deck variety; the spire-engine look-ahead (+5.6) is unreleased and not re-measured (`s6` §1a).
- Decision headroom was found only at paths (`00` §2), so P4's "regenerate V, path and deck models" has little to find.

## Rule problems

- Flagged correctly: the reopening, Direction A, the 3M-weight table.
- Not flagged: the root-mean target reuse above; value-shape constants (closed: `hp_w=2` −0.064, low-HP penalty −0.306, "risk aversion → passivity"). §6.3 says the §2 table states the difference; it has no such row.
- "Equal CPU time ±1%" is not attainable as written: calibration was off by ±14% and a 1.43% overspend voided a result (`h1` §5). A deterministic cost unit is needed.
- P0 go: best of nine or more pilot arms on either of two metrics, the 15–25% band undefined. A 64× random-rollout arm meeting it would refute "bias, not variance" and still count as go.
- P2 kill: "bank boss deaths not below baseline" over four arms is avoided by chance in up to 94% of null cases [computed, independent arms].
- E2 test 2 go, "not above baseline at equal simulations": the F52 mix was level at equal simulations (−0.0215 against −0.0183) and would pass.
- E2 test 1 compares with 8 rollouts; the incumbent's leading candidates receive hundreds. Offline fit failed as a proxy six times (`00` §2).
- P2 run gate: +2.0 points on 2,000 pairs has SE ≈ 0.65 [computed from win correlation 0.43, `d1` §3]: 50% power at the effect it names.
- The bank is reused for SPSA and round gates, so those are not fresh seeds.
- Leak detail: the emitter must hide intents under Runic Dome (`FS.cpp:85`).

## A faster kill test

Whole runs, no bank, no simulator change, two days:
- 1,000 fresh paired A20 seeds at 1×, 16× and 64× (the `sims` parameter only): about 23 CPU-h.
- A nested-pilot arm on the same seeds (each rollout step chosen by a 32-simulation inner search; about 30 lines in `FairSearcher::rollout`): about 55 CPU-h [estimate].
- The clairvoyant arm of `s3` E0: under 2 CPU-h.

Pre-registered reading, in the owner's unit, HP compounding included:
- Best arm below 15%: piloting at 64–300× compute cannot supply half of T1. Plan B becomes a component.
- Nested at 20% or more while 64× stays below 12%: the bias thesis has real support, and the reopening request deserves a hearing.

## What survives

- Fight-level paired harness, independent agent seeds, per-fight logs, the deck-or-pilot measurement.
- Node-mean fix; paired root comparison, with the early stop kept.
- Fight end valued in run-value units (never tried), tested alone, with a stated normalisation.
- The pre-draw stop; the entity emitter, which strategy models can reuse; the leak test extended to it.
- Win rate as primary metric at A20 and Heart; SPSA for constants.

## Revised probabilities for the three tiers

All [estimate].

- **T1 (A20 Act-3 ≥ 25%): 10%** (plan: 28%). Headroom at the P0 level ≈ 0.6; an evaluator worth at least 4× search at honest cost ≈ 0.3 (`s1`'s 35–45% is for any keep); strategy adding ten points in 15 days ≈ 0.35: 0.06. Plus about 0.04 for a moderate engine completed by P4.
- **T2 (A20 Heart ≥ 20%): 2%** (plan: 7%). The Heart fight has not responded to search, conversion moved only with strategy, and the plan reaches Heart work in week 12.
- **T3 (A20 Heart ≥ 45%): below 0.5%** (plan: 1.5%). No mechanism in the plan builds expert decks.
