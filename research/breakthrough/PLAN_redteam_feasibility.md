# PLAN red team — feasibility of stages 0 and 1 (2026-11-16)

Nothing built or run; no git. FA, FS, EM = `research/cpp/FairAgent.cpp`, `FairSearch.cpp`, `research/eval/eval_main.cpp`; SIM = `/home/user/sts_lightspeed`; pm = `research/program.md`. **[estimate]** and **[computed]** are mine.

## Verdict in five lines

1. Stage 0 does not fit in 5–7 days: (b)–(d) alone take about 7, and the 2×2 adds 6–12 [estimate].
2. The 2×2 is planned in the expensive direction, and its key cell cannot return an informative negative. No review examined it: `x2` is four minutes older than the plan (files: 11:21, 11:25).
3. A stand-in for the "our strategy" half of the 2×2 already exists as one parameter (`cheat_combat`, FA:146-151) and has never run at a meaningful budget.
4. Stage 1 (a) and (b) repeat, as tables, two lines this project measured as having no headroom with its own pilot. The plan cites neither.
5. Stage 0–1 compute is between correct and 2× low; money ignores worker utilisation; the stage-1 gate is undefined from 11% to 15%, where the likeliest outcome lands.

## Stage 0 problems and the corrected design

**(a) The 2×2: code facts.**
- Our observation is 412 values (SIM `bindings/slaythespire.h:17`, `bindings-util.cpp:36-63`). Their 6,820-value encoder is a patch; their 807-value descriptor and student features are Python (`x2` §2).
- Our Python module cannot step a game (no action or battle calls; SIM `bindings/slaythespire.cpp:26-146`).
- Enum sizes equal their one-hots (371 cards, 181 relics, 44 potions; SIM `include/constants/`) [computed]; order unverified.
- Their base commit is our legacy pin: `research/patches/sts_lightspeed.commit` is 7476a81, and `research/setup.sh` keeps a build path for the agent on it (`STS_SIM=legacy`).

| | Their strategy into our fork (`x2` §6b) | Our agent onto their engine build |
|---|---|---|
| Port | Encoder, maxima table, binding patches, Python option code, modified search, a binding for `playBattle` | `research/cpp` through the legacy build path, with only the inert parts of our patch (`lastMoveWasRolled`, escape bit); a Python-callable `playBattle` (`FairAgent.h:55`); their searcher in place of `legacy.playoutBattle` (FA:149) |
| Silent failures | Patch conflicts over about 290 commits; id order; Prismatic Shard in our shop pool (SIM `include/constants/RelicPools.h:32`, `include/game/GameContext.h:156`), absent from theirs; events our patch implemented and theirs may not (Colosseum, `research/patches/sts_lightspeed.patch:269-292`); map and burning-elite features; key options (recall = rest action 2, FA:1320; `RT::KEY`, FA:1370) | Crash classes only our fork fixed; whether today's `research/cpp` still compiles on the legacy base [unverified] |
| Verification | None. Their README says a fresh build does not reproduce their games (`x2` §7); our own agent lost half its A20 wins on changing simulator (6.35 to 3.0%, `h4` §1), hence the rule "never compare across simulators" (`h4` §5) | Their games replayed move for move on the merged build against their unmodified one, both built here; 300–500 local games for a reference rate; `{ours, ours}` anchored to legacy-era rows (`r1` §4) |
| Days [estimate] | 8–12 | 6–7 |

Defects either way:
- **The key cell is one-sided.** Their student seeks elites at 60% HP and takes keys (`x2` §1, §3). With our pilot an elite in place of a rest costs −0.083 (`pD_review`), and our one key-taking A20 measurement reached Act 4 in 2.5% of games, against 7.6% Act-3 wins without keys (`00_internal_map` §2, F47). "No better than ours" follows from co-adaptation, from the engine change and from a port bug alike. Only a win informs; I put it under 20% [estimate].
- **The metric is undefined.** "Clears Act 3 twice as often" compares a key-taking agent with a key-less one. Against 2.5%, a doubling is about 3 SE at 1,000 seeds [computed].
- **The harness kills winners.** `alarm(900)` (EM:194) against a 97 s median (`x2` §1); timed-out seeds leave the pairing.
- This VM has Python 3.13 (their bindings: 3.12) and 5.2 GB of free disk [checked].

**(b) Triage.**
- Clairvoyant switch: one condition at FS:470, 0.5 day. It is no ceiling: mean backup (FS:527-536), random playouts (FS:376-438), one action per search (FS:578-593). The plan's fourth table row still stops the programme on it, against `pD_review` F1.
- Boss re-fights: 1 day. Copy the run at `playBattle` entry; reseed the six fight streams (FA:428-429); no `exitBattle` on a copy (shared map pointer, FA:1427-1429); save and restore `rng` and `stats` (FA:1162-1179), or the main line changes. `bfFight` (FA:424-438) will not do: 100 simulations, no potions.
- Replicate seeds: 0.5 day (EM:51, EM:203); the harness is marked frozen (EM:1). The same seed feeds `legacy.rng` (FA:105), which picks Neow at random (`c2` §1), so replicate variance includes Neow.

**(c), (d).** `reseedAllRngs` (FA:1137-1150) leaves `monsterList`, `eliteMonsterList`, `secondBoss` and `bossRelics` untouched (SIM `include/game/GameContext.h:130, 190-194`): the leak test is new code, 1.5 days. The trace exists (EM:52-57); the audit names no data source.

**Corrected stage 0:** the cheaper experiment below, then (b)–(d); the import only if that calls for it.

## Stage 1 problems and the corrected design

**(a) Stage-value tables** are a third attempt at deck values from simulated fights.
- Before: online fights for card picks −0.267 and −0.224; `bf` models with paired correlation 0.10–0.14; twice the data −0.019; whole-run forks at card rewards −0.020 (A10) and −0.009 (A20) (`00_internal_map` §2, §4; `c2` §2).
- New here is averaging: about 900 paired fights per cell instead of one. With survival as outcome a cell's SE is about 1.9 points against effects of 1–3 [estimate]; `x2` §4 records that failure for its authors ("noise 2–6× the option differences"). HP kept resolves better.
- FA:461-497 already applies an option and re-fights on common seeds. Missing: entry-state harvest, a production-fidelity fight (refactor of FA:140-239), a table builder, decision hooks. 6–8 days, 130–330 core-hours [estimate].
- Likeliest null: the table repeats the knowledge-base ranking and flips few picks.
- **Free first step:** build a per-card table from the F28/F31 fork data on this disk (`research/vf/data/branch_f28_*`, `branch_f31_*`; 37.7k decisions, `00_internal_map` §4) and score it on held-out decisions. If it does not beat the agent's own picks, the fight version needs a new argument.

**(b) Whole-run forks.** `branchEval2` covers cards, boss relics, map and rest (FA:1473-1573). Split-half headroom: boss relic −0.007/+0.004, rest +0.004, map +0.014 with half already taken; Neow ranking closed at −0.017, +0.010, −0.0215 (`00_internal_map` §2, §4). New kinds for Neow and keys: 4–5 days, 100–250 core-hours [estimate]. Forks keep the true encounter lists, so they are valid offline only.

**(d) Run value at fight end.**
- `terminalValue` (FS:345-374) sees no run state. Inside a fight only HP, max HP, gold and potion count change among V's inputs (`research/vf/run_features.h:42-44`), so one table V(HP, potions) per fight suffices, at 3–6% of game time (`pD_review`).
- Scale: a loss is worth exactly floor/51. The loss shaping (FS:373) must be rescaled beneath the wins; it is the only signal where playouts cannot win (`c1` §6.2). V's HP slope comes from one label per run (`c2` §2) and is confounded with deck strength.
- No V exists between the two A20 act-3 bosses: the second fight starts with no screen between (SIM `src/game/GameContext.cpp:1139-1151`), and 49.4% of entrants die there (`00_internal_map` §5). That state needs stage 0's table of second-boss wins by entry HP.
- The known-answer test can fail for the wrong reason: HP in a won playout is also a low-variance proxy for win probability. The Heart-fight potion credit was already tested, no keep (`00_internal_map` §2). 3–4 days, 20–40 core-hours [estimate].

**(e) Potions.** The gate is FA:135, with 25% HP hard-coded. Hallway deaths with an unused potion are 7.4% of A20 games (`00_internal_map` §5); the ceiling is about +0.5 point [estimate]. 1–3 days, 10–20 core-hours.

**(c) Rules.** Effects under 1–2 points are below the power of 2,000+2,000 seeds (`h4` §5): test bundles. 5–8 days, 100–200 core-hours [estimate].

**Gate.** The clairvoyant agent's steps of the kinds stage 1 names multiply to at most ×2.3 [computed from `x2` §4; some bundle pilot changes]. Full transfer gives 17.5%; half, on a log scale, 11.5% [estimate]. The plan defines nothing between 11% and 15%.

## Corrected effort, compute and money

| | Plan | Mine [estimate] |
|---|---|---|
| Stage 0 days | 5–7 | 8–9 with the cheaper experiment; 13–19 with the import |
| Stage 0 core-hours | 100–150 | 45–95; with the import 90–120 at 1,000 seeds per cell, about 265 at the plan's 2,000-seed standard |
| Stage 1 days | 15 | 19–28 |
| Stage 1 core-hours | 400 | 360–840 |
| Money, whole plan | $2,000 | $2,300–4,100 |

- Two cells of 1,000 games at 97 s are 54 core-hours; ours cost 0.78–1.83 s per game (`c2` §5). The plan's own sums check (5,450 core-hours, 1,375 box-hours).
- The $1.5 rests on "10 worker sessions ≈ 2 h, ≈ $30" (`h1` §2); I could not find that figure in pm. At most half of that batch's billed core-hours were game time [computed: 97,500 games × 0.36–0.37 s], so dividing core-hours by four understates cost by up to 2×.
- This session plus workers can run both stages; tables, forks and re-fights fit the F28–F38 pattern. Frictions: the 2-hour limit (`research/workers/README.md:24`), idle containers reclaimed (pm 2026-11-08 13:42), longer games at half the estimated data rate (pm 2026-11-09 07:25), usage limits (pm 2026-11-16 10:11), workers declining tasks sent by message (pm "Parallel workers", 18:21). The import alone needs a new toolchain per worker.

## A cheaper first experiment, if one exists

Yes: the "our strategy" row, from our artefacts alone.
- `cheat_combat=1` plays fights with lightspeed's clairvoyant searcher (FA:146-151). Its design budget is 50,000 simulations, bosses ×3 (SIM `include/sim/search/ScumSearchAgent2.h:30-31`). The one diagnostic ran it at 500, never at budget: 9.6%, 16 crashes (`research/results.tsv` row 27).
- 1,000 common A20 seeds: fair 1× (exists), fair 16×, our search with the true future at 4×, and `cheat_combat=1 sims=40000`. Boss re-fights with both pilots.
- 1–2 days, 25–45 core-hours [estimate]. Lengthen the alarm; count crashes.
- Reading: our decks under a near-perfect pilot at 20% or less: strategy binds, stage 1 stands. At 40% or more: pilot and information bind, stage 3 moves ahead. Between: both. Our strategy is tuned to a weak pilot, so the figure is a lower bound.

## Changes the plan must make before the owner sees it as final

1. Replace stage 0 (a) with the experiment above; keep the import conditional, in the second direction, with metric, comparator and verification written down.
2. Remove "stop and report" from the clairvoyant reading.
3. Say that 1 (a) and 1 (b) revisit `bf` and F33/F34, and why tables should find what those did not; put the free table test first.
4. Specify 1 (d): table, loss scale, the two last-fight cases, the estimator caveat.
5. Define the outcome for 11–15%.
6. Restate stage 0 as 8–9 days and stage 1 as 19–28; state money with utilisation.
7. Add owner decisions: edits to the frozen harness, a longer hang guard, disk on this VM.
