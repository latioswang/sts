# h4 history digest — program.md lines 533–687

Scope: 2026-11-06 01:15 → 2026-11-07 09:01 UTC: phases F1–F12, the F13 pre-registration and two simulator switches (legacy → fork dfda16c → fork master 1b05bdc). The whole range is about the run value V (vr_*): retraining, weighting, and handing it more out-of-combat decisions. It contains no combat-search, rollout or leaf-value experiment. dscore / dwin / P = paired score difference, win-rate difference, P(improve). Slices: 500 seeds (A0, A10), 1,000 (A20, Heart).

## 1. Experiments and idea lines

| ID | Area | What was changed | Result | Verdict | Track |
|---|---|---|---|---|---|
| Sim switch 1 re-baseline (dfda16c) | simulator fidelity | Legacy bests re-run on the fidelity fork, 1,000 fresh seeds each | A0 0.796 (85.1 → 79.6, −5.5); A10 0.361 (37.2 → 36.1, −1.1); A20 0.030 (6.35 → 3.0, −3.35); Heart 0.118 (12.7 → 11.8, −0.9), act4 0.580 | info | all |
| F1 | run value V | V retrained on 120,000 fork games (4 × 30,000, explore_eps=0.2); vr_171 = fork + legacy data, h64 | Holdout mean RMSE 0.4254 vs vr_151 0.4502, vr_121 0.5042, vr_81 0.7392. Play at old weights: A0 dscore −0.0005 (P 0.491); A10 pooled +0.0113 (P 0.711); A20 pooled +0.0036 (P 0.707); Heart pooled +0.0246, dwin +0.013, P 0.997 | keep Heart only (f1heart) | all |
| F2 | run value V | V weights ×1.5 (vr_bonus, vr_relic_w); V-only card and boss-relic picks (vr_pure) | f2a0w +0.0537 / dwin +0.036 / P 0.982; f2a10w +0.0733 / +0.052 / 0.996; f2a20w pooled +0.0100 / −0.002 / 0.950; f2heartp (V-only) +0.0323 / +0.006 / 0.981, act-4 reach +9.0 pts; f2a0p (V-only) −0.0277 / −0.016 / 0.140; f2heartw +0.0114, P 0.880, extension withdrawn | keep a0w, a10w, a20w, heartp; discard a0p | all |
| F3 | run value V | Weights ×2.25; V-only on A10 / A20 | f3a0w pooled +0.0142 / +0.014 / P 0.862; f3a10w +0.0227, P 0.855, withdrawn; f3a20w pooled +0.0125 / +0.009 / P 0.995 (wins 3.2 → 4.1%); f3a10p +0.0702 / +0.052 / P 0.987; f3a20p pooled +0.0032 / −0.002 / P 0.668 | keep f3a20w, f3a10p | A0/A10/A20 |
| Sim switch 2 (1b05bdc) | simulator fidelity | 28 more game fixes + speed work | User CPU 17.7 s vs 29.7 s (−40%), wall 5.2 s vs 8.3 s (10 games). Re-baseline: A0 0.831, A10 0.434, A20 0.040, Heart 0.152 (act4 0.656) vs dfda16c ~82–84 / 44.0 / 4.1 / 13.8 | info | all |
| F4 | run value V | Expert-iteration step: 120,000 games from the V-driven bests → vr_191 (= vr_182, 627,300 runs) | Mean holdout RMSE 0.4280 vs vr_171 0.4379 (−0.0099; F1's step was −0.025). f4a0 +0.0115 (P 0.684); f4a10 −0.0275 (0.192); f4a20 +0.0035 (0.646); f4heart +0.0076 (0.678) | discard all | all |
| F5 | shop-rest-event-relic policy | V on five more decisions at once: shop cards, shop relics, upgrades, rest-vs-smith, removal | f5a0 −0.0140 / −0.014 / P 0.265; f5a10 +0.0522 / +0.036 / P 0.946; f5a20 +0.0160 / −0.001 / P 0.928 (+0.86 floors, wins flat); f5heart −0.0160 / −0.004 / P 0.134 | keep A10, A20 | all |
| F6 | shop-rest-event-relic policy | Per-hook leave-one-out (A10, A20) and single adds (A0, Heart) | Sign table (A0/A10/A20/Heart): shop cards + + + +; shop relics 0 + + −; upgrades + + + +; rest − − − 0; removal − 0 + −. Keeps: f6a10_norest +0.0778 / +0.052 / P 0.999; f6a20_norest pooled +0.0091 / +0.006 / P 0.905; f6a0_shop +0.0254 / +0.018 / 0.968; f6a0_upg +0.0268 / +0.018 / 0.949; f6heart_shop +0.0189 / +0.018 / 0.997; f6heart_upg pooled +0.0111 / +0.009 / 0.971 | six keeps | all |
| F7 | shop-rest-event-relic policy | vr_rest=2: V(heal) vs V of the one upgrade the agent would actually pick (removes max-over-cards bias) | f7a10 +0.0094 (P 0.653); f7a20 +0.0163 / +0.011 / P 0.942; f7a0 −0.0161 (0.201); f7heart −0.0217 / −0.020 / P 0.040 | keep A20 (f7a20) | all |
| F8 | shop-rest-event-relic policy | V-only upgrade choice (vr_upg_w=1000000) | A10 +0.0109 (P 0.649); A20 pooled +0.0078 / +0.005 / P 0.871; A0 −0.0194 / −0.012 / P 0.164; Heart +0.0001 (0.500) | discard all | all |
| F9 pmlp | move prior | Root MLP move prior ported to Heart | Slices +0.0093, +0.0078; pooled +0.0086 / +0.003 / P 0.794 | discard | Heart |
| F9 mapd / ehp | path/map | map_discount=0.85 on A10, A20, Heart; elite_hp=0.75 on A0 | a10 +0.0053 (0.648); a20 −0.0068 (0.115); heart +0.0027 (0.679); a0_ehp −0.0147 (0.198) | discard all | all |
| F9 pot | shop-rest-event-relic policy [closest area, inferred] | A0 potion values (potion_v=8, potion_v_boss=4) ported | heart −0.0214 (P 0.056); a10 +0.0120 (0.683); a20 +0.0024 (0.591) | discard all | A10/A20/Heart |
| F10 | shop-rest-event-relic policy | V on event choices: K=4 RNG-re-seeded copies per option, roll forward with own policy, mean V; hand table if a copy reaches BATTLE | A10 +0.0296 → pooled +0.0094 (P 0.649); A20 +0.0096 → pooled +0.0020 (0.596); A0 −0.0035 (0.444); Heart −0.0458 / −0.029 / P 0.001 | discard all | all |
| F11 | run value V | Heart-labelled V (15,000 Heart games; label counts a win only if act ≥ 4) driving V-only picks | Holdout rmse 0.3722 / corr 0.354 vs vr_171 0.7854 / 0.273 (predict-the-mean 0.3980). Play: win 0.114 vs 0.179, floor 34.14 vs 49.16, act4 0.241 vs 0.668; dscore −0.3263, P 0.000 | discard (collapse) | Heart |
| F12 | run value V | Same, 32 rows per run instead of 4 | corr 0.332 (h64), 0.275 (h256) vs 0.354; no arm run | closed | Heart |
| F13 | statistics/method | Paired re-measure, current best vs pre-F5 best, 2,000 never-used seeds per track | Pre-registered only; outcome is outside lines 533–687 | info | all |
| Event leak test | statistics/method | Recompute each event decision after re-seeding the real RNG streams | First build: mismatches 7 / 9 / 7 / 13 — relic pools were shuffled from their real hidden order [cause inferred in source]; fix e57f1a9 → 0 mismatches | keep (gate) | all |
| 1b05bdc crashes | simulator fidelity | SIGSEGV in `CardManager::moveToDiscardPile` ← `onAfterUseCard` ← `FairSearcher::rollout` | Seven seeds (1237071, 1240783, 1255890, 1264825, 1267057, 1276108, 5240281), all on the Heart track; "first crash in ~9,000 games" | info, unresolved | Heart |

Bests at the end of the range: A0 f6a0_upg, A10 f6a10_norest, A20 f7a20, Heart f6heart_upg.

## 2. Sensitivities

- **Simulator fidelity.** Legacy → fork: A0 −5.5, A10 −1.1, A20 −3.35 (6.35 → 3.0, about half), Heart −0.9. Fork → 1b05bdc (28 more fixes): all within noise (83.1 vs ~82–84; 43.4 vs 44.0; 4.0 vs 4.1; 15.2 vs 13.8). X1's legacy gain showed "no sign" of transferring [INFERRED, MED; unpaired, as written].
- **V accuracy → play.** F1: mean RMSE −0.025 → tie on A0, +0.4 pts A10, +0.2 pts A20 at unchanged weights; +1.2 pts Heart wins only where the old V was two generations old (vr_41). F4: RMSE −0.0099 with 120,000 more on-policy games → nothing measurable. h256 vs h64: no material difference (F1 "gave nothing"; F4 mean 0.4282 vs 0.4280; F11 rmse 0.3722 vs 0.3738; F12 worse).
- **V weight.** ×1.5: A0 +3.6 pts, A10 +5.2 pts, A20 score +0.0100 with wins −0.2, Heart +0.8 (near miss). ×2.25 on top: "+1.4 / n.a. / +0.9 points" — "the weights look near their optimum".
- **V-only card and boss-relic picks.** A0 −1.6 pts; A10 +5.2 pts; A20 pooled +0.0032 (P 0.668); Heart act-4 reach +9.0 pts but Heart kills +0.6 ("noise").
- **V on more decisions (F5/F6).** A10: removing the rest hook +5.2 pts, removing upgrades −3.4, shop −2.2, shop relics −1.2, removal 0. A20: every hook ≤ 1 pt. A0: shop +1.8, upgrades +1.8, rest −1.0, removal −3.2. Heart: shop +1.8, upgrades +0.9 pooled, shop relics −1.2, removal −3.4, rest +1.6 wins but −0.76 floors (score +0.0001).
- **Unbiased rest rule (F7).** A20 +1.1 pts wins; A10 neutral; A0 −1.0; Heart −2.0.
- **V on events (F10).** ≤ +0.2 pts on A0/A10/A20; Heart −2.9 pts.
- **Heart V.** In-distribution labels raised corr 0.273 → 0.354 yet play fell 6.5 win pts and 15 floors. 8× rows from the same 12,597 runs: corr 0.354 → 0.332.
- **Cumulative over the range [inferred from the quoted levels; spans dfda16c and 1b05bdc, which the source never compares directly, though the re-baseline put them within noise]:** A10 0.361 → 0.458–0.478; A0 0.796 → 0.830–0.854; Heart 0.118 → 0.165–0.181; A20 0.030 → 0.040–0.056. The gains came from how vr_171 is used (weight, V-only picks, more hooks, removing the rest hook); the second accuracy step (vr_191) added nothing.
- **Selection shrink.** A10 best 0.478 → 0.452–0.466 on re-measure; A0 0.854 → 0.830–0.836. Near miss f10a10 +0.0296 → pooled +0.0094.
- **Cost.** 1,000 games on 1b05bdc: 442 s (A0), 325 s (A10), 178 s (A20), 498 s (Heart).

## 3. Diagnoses

- **A20 gains are floors, not wins** (f2a20w: wins 3.3 vs 3.5% pooled; f5a20: +0.86 floors, 4.0 vs 4.1%). Mean A20 floor stays 27–29 across every arm — the typical A20 death is in act 2 [inferred from the floor numbers]. Strong, repeated evidence that the strategic-V line barely moves A20 wins.
- **Heart: reaching act 4 is not the problem; killing the Heart is.** act4 0.64–0.70 with win 0.15–0.18, so roughly a quarter of act-4 entrants win [inferred ratio]. V-only picks added +9.0 pts of act-4 reach and +0.6 Heart kills.
- **V is blind to the Heart objective.** Every V before F11 was trained on keyless runs with win = act-3 boss, so Heart choices "optimise for the act-3 boss, not for keys and act 4" [inferred in source]. Structural: the V features (24 scalars + boss + relics + cards) contain no key flags, and the logged rows lack keys [known; effect size inferred]. Supporting: F10 Heart −2.9 pts, F8/F9 flat, shop-relic and removal hooks negative on Heart.
- **Level accuracy ≠ ranking accuracy.** F4 "accuracy of the value level is not accuracy of the ranking between options" [INFERRED, MED]; F11 "a holdout win in RMSE/correlation said nothing about whether the model's within-decision rankings are good enough to drive picks". F4 conclusion: "V accuracy is no longer the bottleneck of the current decisions" [INFERRED, MED].
- **Weak V skips cards.** F11: mean final deck 17.6 vs 26.3 cards; thin decks died at floor 34 vs 49.
- **Rest sites.** vr_rest=1 compared one heal estimate against the max of 10–20 noisy upgrade estimates, biased toward smithing [inferred in source]; cost 5.2 pts on A10. After the fix V's rest-vs-smith signal still loses on A0 and Heart — "itself wrong" [inferred in source].
- **A0.** The KB card score "still carries upgrade information V lacks" (V-only upgrades −1.2 pts); V on removal −3.2 pts; "more V-made decisions help from A10 up, cost a little at A0" [INFERRED, MED].
- **Distribution shift.** vr_171 scored worse on states of the new V-driven policies (A0 0.4738 vs 0.4626, A10 0.5668 vs 0.5367); policy and simulator changes "not separable". A0 holdout correlation is low (0.297–0.39) against A10/A20 0.53–0.60.
- **Legacy-simulator overfit.** The tracks whose V carried "the most legacy-specific signal" fell most on the fork (A0, A20).
- **Heart data limit.** "What limits a Heart V is runs, not rows" [inferred in source]; both Heart nets early-stopped at epoch 1.

## 4. Mentioned but never tested

"Never tested" means within lines 533–687; later phases may have picked some up. Stated in the text:
1. **V features v2 with key flags + ≥ 50k Heart runs** — "parked as a larger project", needs workers.
2. **Heal/upgrade data in V's training set** as the fix for a wrong rest-vs-smith signal — named as "the next step" under a condition that was only partly met; never done.
3. **"1.5x weights on top of V-only" on Heart** — "a new question for the next phase"; F3 had no Heart arm.
4. **V trained on BATTLE states** — "V was never trained on BATTLE states", so any event option leading into combat falls back to the hand table. MATCH_AND_KEEP, DEAD_ADVENTURER and NEOW are excluded from V entirely.
5. **Simulated heal hypothetical** — heal stays a hand formula because executing REST on a copy would reveal Dream Catcher's reward; the upgrade side keeps "some bias".
6. **One V for Heart and non-Heart runs** — rejected by design ("would mix two meanings of win"); not tried with a track or key input.
7. **Small positives left below power, no third slice allowed:** V-only upgrades on A20 (pooled +0.0078, P 0.871, both slices positive); 2.25× on A0 (pooled +0.0142, P 0.862); Heart root move prior (pooled +0.0086, P 0.794); A10 potion values (+0.0120, P 0.683). Withdrawn before resolution: f3a10w (P 0.855), f2heartw (P 0.880).
8. **Two-candidate combination step** (F6) — defined, never triggered.
9. **f6heart_rest: +1.6 pts Heart wins, −0.76 floors** — discarded on score; "trades depth for Heart conversions"; no follow-up.
10. **vr_event settings** (K=4, cap 20 steps) — never varied.
11. **Early stop at epoch 1** on fork-only and Heart sets — noted as a caveat, never investigated.
12. **Crash root cause** — seven seeds handed to the fork owner; unresolved in this range.
13. **The 28 fixes of 1b05bdc** — relayed, "not yet checked by me beyond the build".
14. **Leak test for out-of-combat decisions** — only combat search and events are covered; upgrade and rest hooks are argued "leak-free by construction".
15. **Store clean-up** — owner's open decision to delete per-file data sets that have _z twins.
16. **F13** — the result of the honest re-measure is not in this range.

Gaps implied by the text but stated nowhere in it [inferred]:
- A V selection metric or training target that scores within-decision ranking instead of RMSE.
- Any experiment aimed specifically at the act-4 fights.
- A re-test of the hook signs after the simulator or V changes (each hook was judged once, on one slice).

## 5. Method lessons

- Single-slice keeps carry selection bias; re-measures shrink by about 2 pts. Near misses usually shrink on extension.
- Rule in force: fresh slice per decision; near-miss band 0.7 ≤ P < 0.9; one extension only; keep iff pooled P ≥ 0.9 and second-slice dscore > 0. The source calls +0.5 pt on A20 something "2,000 seeds cannot confirm"; effects below about 1–2 pts are below the rule's power [inferred].
- Holdout RMSE / correlation is not a proxy for play (F1, F4, F11). A new model can look null merely because it is under-weighted (F1 → F2).
- Bundled arms hide opposing effects: F5's A0 and Heart discards "hid two helpful hooks behind the rest hook".
- Labels must match the track objective (keyless act-3 wins scored 2.0 on the Heart track before the fix).
- A max over many noisy value estimates against one estimate is biased.
- The leak test must be able to fail: the event test caught a real leak (relic-pool order) before any arm ran.
- Never compare across simulators (results are tagged); repro gates must be bit-identical after each rebuild.
- Withdraw extensions whose base is superseded; fix the base when two candidates share one.
- Mean RMSE over tracks weights A0/A10 more than A20 (RMSE scales with the outcome level).
- Operations: hourly container restarts kill the runner; `| tail -1` hides exit codes; never edit a script a queue line is executing; training was killed at 13.9 GB RSS (≈ 3.9M rows); a crashed game leaves an empty data file; workers stop at a 2-hour limit unless detached.
- Pre-registered predictions missed in both directions (four F2 "keep LOW" arms were kept; F11 lost far more than predicted). Track parameters proved "mostly track-specific, not missed transfers" (F9).

## 6. Hints of a big untested lever

1. **Act-4 conversion is the Heart bottleneck** [inferred]. act4 reach 0.65–0.70 against Heart wins 0.15–0.18 (lines 573, 587, 625); every experiment in this range acts on pre-act-4 strategy and none on the Heart fights.
2. **A Heart-aware value needs new inputs and far more runs** [inferred]. No key flags in the features (line 534); 15,000 runs gave corr 0.354 and a collapse when trusted alone (line 536). The only Heart-specific attempt was starved, not refuted.
3. **Train and select V on decision ranking, not outcome level** [inferred]. Lines 608, 611 and 536 all show RMSE gains with zero or negative play effect, while changing how V is used was worth +3.6 to +5.2 pts (lines 644–645).
4. **A20 is not limited by the strategic layer** [inferred]. Twelve phases of V work moved A20 wins by 1–2 pts (0.030 → 0.040–0.056), with floors stuck at 27–29 (lines 606, 638, 576); the step change must come from combat strength or act-2 survival.
5. **Look-ahead that plays through combats** [inferred]. The event planner already copies the game, re-seeds hidden randomness and rolls forward, but stops at BATTLE because V has no battle states (line 552); a value or simulation that crosses fights would extend planning to paths, elites and risky events.
