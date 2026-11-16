# pC review — "unified expert iteration"

Nothing was built or run. [computed] = my arithmetic on cited numbers; [estimate] and [inferred] are mine. `00` = `00_internal_map.md`; `FA`, `FS` = research/cpp/FairAgent.cpp, FairSearch.cpp; `pm` = research/program.md. H = honest headroom: pick by one half of the playouts, score on the other.

## Verdict in five lines

1. Do not approve as written. The plan is three component projects; the loop it is named after is first gated in P4.
2. Its lead strategic mechanism is the project's own F26–F38 line, closed at card rewards with H below zero. Policy gradient on the run outcome, which the outside results used, is a one-campaign fallback.
3. The plan's own contribution ranges top out at 22.6% at A20 [computed], below T1's 25%, to which it gives 30%.
4. The play-time arithmetic holds only for the value-only search. The stage specified first runs about 390 simulations, and the run network's cost assumes a batched kernel that does not exist.
5. E1 and E3 can pass without supporting the thesis. Three tests of a few days each decide more.

## Fatal or near-fatal flaws

**F1. The first two weeks do not test the thesis.** E1, E2 and E3 test a terminal value, an offline ranking and an oracle, each alone. A closed loop over whole runs is first gated in P4 ("A20 ≥ 15% by generation 5"), after 39 engineering days and 2,800 box-hours, $4,200 [computed].

**F2. The strategic operator is a closed line, and the order of mechanisms is inverted.**
- Labels from continuations on common seeds, centred within the decision and distilled into a model, are F26–F32 (cards) and F35–F38 (paths). Cards: 19.5k decisions gave +0.0019 per decision and 37.7k gave +0.0028 (`h3` §1). F33 then measured H = −0.020 and −0.009; the pre-registered verdict was "card-decision work stops" (`pm`:366, :374).
- "Round 2 above round 1" is the signature of the lineage (`s1` §5). On the one surface where round 1 worked, round 2 was flat: pm_4 +0.0071 and pm_5 −0.0031, neither armed (`pm`:266, :242).
- The loop's label is 64 continuations in total: with four options (`pm`:374), 8 per option in the first halving round [computed], at a quarter of the search budget. At 8 full-cost playouts the halves agreed on the best card 35% of the time, against 25% by chance (`h2` §1).
- Mentor built a labelled option ranker first and replaced it with PPO (`s4` §2, item 7). The spire-engine and Mentor numbers the plan cites are PPO results.

**F3. T1 does not follow from the plan's numbers.** Today's 7.6% plus the top of every range (10, 3 and 2 points) is 22.6% [computed]; the stated median is 15–18%.

**F4. The U-table brings back a shape that failed, at an unstated scale.**
- Random playouts end with less HP than real play [inferred], so a concave U is read at its steep end. A concave HP utility is `lowhp_pen` (`FS`:349): −0.306, "strong risk aversion → passivity" (`pm`:1511).
- A20 run-win probability is about 0.08 and the loss term must stay below the smallest win value. Losses span about 12% of the normalised range today and a few percent under U [estimated from `FS`:345-373]. That band steers boss fights, where 48.7% of A20 games end (`d1` §2).

## Corrections (novelty, code, arithmetic)

**Novelty.**
- Confirmed never tried (`00` §3 a, g, n, o, r): fight end in run-value units, paired root with sequential halving, reinforcement learning, privileged critics, an A20 Heart objective.
- Prior below the root lost at equal wall-clock (−0.010) and with 40% more compute (−0.009), and better priors gave nothing at 500 simulations (`00` §2). Entity inputs do not answer that.

**Code.**
- `branchEval2` covers card reward, boss relic, map and rest (`FA`:1480-1491); `shop()` buys one item per call in fixed order (`FA`:941); `playout` runs to the end of the run (`FA`:1578). E3 needs a new shop decision type and truncation.
- The 519k logged runs hold 666 or 670 floats per state plus win, floor, act and seed (`FA`:1241-1248, 1587-1594): no options, chosen action, relic counters, potion identities or map. Pre-training R's inputs or cloning decisions needs a new logger at every hook, legacy handler included (`FA`:1391-1392).
- No state serialiser: `GameContext` holds vectors and a shared map pointer (sts_lightspeed/include/game/GameContext.h:178-186, :220); `SaveFile` only reads the real game's files (SaveFile.h:152-157). A bank is replay keys, valid only while agent and simulator stay bit-identical.
- `vr::predict` is a scalar batch-1 loop (research/vf/vr_mlp.h:49-57). The plan's 19 µs is a batched figure; batch 1 costs 196–222 µs at 1.05M parameters (`s5` §2).
- Sequential halving replaces the visit-lead early stop (`FS`:562-576), a confirmed +0.016 to +0.032 on every track (`00` §2); the plan is silent.
- `tools/calibrate.sh` times 8 games on the A0 parameters only (lines 8, 15); calibration has been off by ±14% (`h1` §5).

**Arithmetic.** Totals, labels per day and fights per day check out.
- Stage 2 as specified keeps the rollout: 9.0 µs per simulation, 390 simulations, 0.78 of today's [computed from the plan's rows]; the table omits it.
- C's 3 µs doubles a micro-benchmark without feature extraction, which costs about 1.3 µs for today's 204 aggregates [inferred from `c1` §4, `s5` §2]. At 4.5 µs the three forms give 510, 305 and 235 simulations [estimate].
- U-table: 7.3 ms in each of 15.9 fights is 15% of an A20 game in the worst case [computed]. E1's one-pass form is nearly free and should be the design.
- E3 uses 64 continuations per option, four times the loop's label [computed]; a pass does not validate the loop.

## Evidence against the thesis

- A20 wins go 6.0, 8.5, 9.3% over two doublings (`d1` §3). The combat half's +1.5 to +3 points needs a network worth two to four times the search at equal time; `s1` §5 puts any keep at 35–45%.
- The Heart-fight loss rate moves two points at 2× and at 4× search, within noise (`d1` §3). P2 does nothing for T2.
- Headroom exists only at paths (`00` §2), and the plan's operator is the same one-step test under the agent's own continuation, not the "joint policy change" of `00` §7.3.
- C's heads (death probability, HP-loss histogram) are a fight-outcome surrogate used as the leaf value. Mentor's predicted survival 0.64–0.68 against a true 0.29, and its learned leaf nets are not deployed (`s4` §1). `w1` lists learned leaves and search distillation as dead ends in three outside projects.
- Outside numbers are self-reported on other engines. The fidelity fork halved this project's A20 rate (`h4` §2); Mentor's 30.0% re-ran at 16.8% (`s6` §5).
- The kept A20 gains were floors (`h4` §3). A win-only criterion at a 7.6% base discards that signal; at A20 Heart the base is 0.3%.
- Models replacing rules lost −0.07 to −0.62 and ports across tracks failed 9 of 9 (`00` §2); the cloning loss of one network for all tracks is not budgeted.

## Rule problems

- Closed or kept items touched without a row in §6: card-decision work (`pm`:366, :374); path second rounds; terminal and loss shape (`00` §2); potions back in hallway search, which reverts a keep of +0.0492 (`r1` §1). The evaluator reopening is flagged correctly.
- The non-combat leak test is promised in §2 and absent from every phase; today's test checks combat only (research/eval/eval_main.cpp:80-88).
- E1 go: any of three arms on either of two tracks, then one confirmation above zero: false go about 15–20% [estimate]. Go is in win rate, stop in score, with an undefined band between.
- At A20 the SE of dwin on 2,000 pairs is about 0.63 points [computed; win correlation 0.43, `d1` §3], so the gate needs +0.8: the currency's +0.5 to +2 sits at that limit, and a true +1-point generation is promoted 58% of the time [computed].
- E2's go is offline, where fit failed as a proxy for play six times (`00` §2), and its comparator is 8 rollouts while a root candidate gets about 100 [inferred]; it must be the 64-rollout mean.
- E3: H's unit is not stated; scored with the same V bootstrap, it measures agreement with V; map (+0.014 already known) plus a rule-driven shop can pass alone.
- The fight suite is reused for E2, P2 and round comparisons, so its seeds are not fresh. P0's diagnostics use 500 seeds and no decision rule.

## A faster kill test

Three tests on existing code, under two weeks, about 100 box-hours ($150).

1. **Known-answer currency (2 days, under 5 box-hours).** In the last fight of a run the true U is known: HP and kept potions are worth nothing. Add an `hp_w` override beside the per-fight potion ones (`FA`:119-138), set both to zero there, and run 2,000 paired Heart seeds and 6,000 A20 seeds. If the last-fight death rate (49.9% and 49.4%, `d1` §2) does not fall by 2 SE (about 3.5 points [computed]), drop P1. The potion half alone was a null on 500 seeds (`pm`:2613-2620).
2. **Label scaling (5 days, about 60 box-hours [computed]).** 200k card-reward decisions, ten times F28, at the loop's own label budget, through `branchEval2` and `train_dec.py`; H scored on full playouts; 4,000 play seeds. Kill the operator if the gain stays below +0.006 per decision or dwin ≤ 0. The plan learns this only at P3's kill point.
3. **Closed loop on one surface (4 days, about 35 box-hours).** The experiment of `s4` §6: PPO on card rewards at A10, 123k runs, real search in every fight. Below +1.5 points on 2,000 pairs, the +4 to +10 points that carry the plan are in doubt.

## What survives

- Fight end in run-value units, tested alone, one-pass form, stated normalisation: the owner-approved, never-started Phase R stage 3 (`pm`:2026).
- One option-scoring network over every non-combat surface, trained on run outcomes with the real search in the loop, PPO first.
- Win rate as the gate at A20 and Heart, with seed counts sized to it; an A20 Heart objective.
- Per-fight logs, a replay-key fight bank, the entity export, the extended leak test, privileged critics, common seeds in training.
- Paired root comparison with the early stop kept.
- E2's offline gate, comparator corrected, before any combat-network work.

## Revised probabilities for the three tiers

All [estimate].

- **T1 (A20 Act-3 ≥ 25%): 9%** (plan: 30%). It needs more than the plan's top end: strategy adding 8 points or more (0.25, mostly through the PPO fallback), 12 or more given that (0.3), plus a little for all three components landing high.
- **T2 (A20 Heart ≥ 20%): 1%** (plan: 8%). The plan's arithmetic needs 40% keyed Act-3 clears, well beyond T1, and 50% conversion against 14% today (6 of 42, `d1` §2).
- **T3 (A20 Heart ≥ 45%): below 0.3%** (plan: 2%). No gate tests expert-level deck building.
