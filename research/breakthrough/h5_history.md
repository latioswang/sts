# h5 history digest — program.md lines 687–1376

Scope: the oldest part of the state log, about 2026-11-01 → 2026-11-06 00:31 UTC, all on the **legacy simulator** except Phase P1 and the re-baseline (fidelity fork dfda16c). Covers batches 26–47, Phase C v3 (deck value bf_*), Phases S/S2 (learned leaf), R–R10 (run value V), B20, the selection-bias discovery, X1, P1 and the first simulator switch. dscore / dwin / P = paired score difference, win difference, P(improve). Line 687 is the tail of an entry whose header is above my range; lines 1366 and 1374–1375 are orphaned fragments, quoted as such. Knob names such as `ca1b2`, `ct4`, `wev`, `wsr`, `rh06b`, `scal4` are not defined in my range.

## 1. Experiments and idea lines

| ID | Area | What was changed | Result | Verdict | Track |
|---|---|---|---|---|---|
| Prior ceiling note | move prior | v2/v3, temperature, c sweeps on 500-sim targets | "all neutral or worse" | closed | A0 |
| Batch 26 | path/map; combat search | `wea1p1` act-1 elite path weight; `selq03/05` robust-child selection; `sbd0` removal order; `ca1b2` | wea1p1 keep (61.3 → 63.7%); selq +0.003 / +0.010; sbd0 exactly +0.000; ca1b2 −0.007 | keep wea1p1 | A0 |
| Batch 28 `rollprior_cal` | rollout/leaf evaluation | prior-guided rollouts at equal wall-clock (2.43× per-sim cost → 210 sims) | 59.3% win, −0.062, P=0.033 | closed | A0 |
| Batch 29 | path/map | `wea1p2`, `weh25`, `eh05`; `a10_wea1p1` | −0.008, +0.001, −0.014; A10 −0.022 | discard ("saturated at A0") | A0/A10 |
| Batches 31/34/35 | shop-rest-event-relic policy (potions) | potion HP value; save potions for bosses (`potsave2`) | potv8 +0.022 P=0.77; potsave2 keep 66.3% val (65.5% pooled); potsave3/4 flat; `a10_potsave2` 20.3%, −0.091, P=0.000 | keep A0 only | A0/A10 |
| Batches 32/33 | deck value; path/map | KB synergy bonuses (`a10_kbsyn`), `a10_eh075` elites only above 75% HP | kbsyn +5.7 pts val, ~0 on dev (15.0% vs 15.0%); kbsyn2 −0.069; eh075 27.0%, +0.065, P=0.981, dev +0.075 P=0.928 | keep eh075 | A10 |
| A20 ladder | path/map; rest | `a20_eh08` (80% HP elite gate), `a20_noel1`, `a20_restlo` | floor 21.8 → 24.2 → 25.3 → 25.6; dev `a20_restlo_dev` +0.084, P=0.999; `a20_el04` −0.016 P=0.04 | keep (floor-driven; wins 7→4→3→4 of 300) | A20 |
| C v3 g0 as replacement | deck value | interventional fight model picks cards / upgrades / removals | `bfpick_g0` 56.0%, −0.107; `_m02` 50.3%, −0.195; `bfupg_g0` +0.004; `bfrem_g0` −0.140; A10 ports −0.075 to −0.207 | closed | A0/A10 |
| `bfbonus10` (g0) | deck value | KB tier value + 10 × model's fight-value gain, card rewards only | 69.3%, +0.054, P=0.952; dev +0.051 P=0.876 | keep | A0 |
| `bfbonus10_g1` | deck value | graded labels (damage fraction vs act boss and a next-act boss) | 73.3%, +0.051, P=0.947; dev +0.023 P=0.663; as replacement `bfpick_g1` −0.066, `bfrem_g1` −0.068 | keep (bonus only) | A0 |
| Model 2 (g1m: A0+A10 fights) | deck value | pooled-ascension model | A0 −0.028 / −0.006; `a10_bfbonus25_g1m` 29.3%, +0.062, P=0.950 (dev +0.035, P=0.738) | keep A10 | A0/A10 |
| Batch 37 | deck value | bonus at shop purchases and upgrades | `bfshop10` −0.020 P=0.073; `bfupgb10` −0.000 (identical games); `bfupgb25` −0.029 | discard | A0 |
| Models 3/4/5/11, 7/8 | deck value | all-pooled (128,594 rows), A10-only, A20-only, clean newer-agent A10 data; models 7/8 (fragment: "worse than 2/1") | `a20_bfbonus10_g2m` +0.017 P=0.881 (pooled-rule keep P=0.935); A10 with model 3 −0.063 / −0.030; model 4 −0.034; model 5 −0.003 / +0.011; `g2c` +0.014 P=0.642 | keep A20 g2m only | all |
| Batches 38/41/42/43/45/47 | shop-rest-event-relic policy | A20/A10 knobs: boss potions, bonus 25×/50×, Neow ranking, pre-boss heal, AoE/scaling/power-upgrade bonuses, rest thresholds | `a20_potb2` +0.013 P=0.998 and `a20_bfb25` +0.036 P=0.991 are val-only (dev −0.002; −0.026); `a20_restboss1` pooled P=0.912 keep; 50× saturated; Neow +0.010 / +0.004; `a10_restboss1` −0.048, `a10_aoe3` −0.035; rest all neutral | mostly discard | A10/A20 |
| Batches 39/40 | path/map | map-path discount, event/shop-rest weights | `mapd085` 75.0%, +0.018, P=0.913 (dev exactly +0.000); `mapd080` −0.021; A10 weights within noise | marginal keep, "not claimed" | A0/A10 |
| Heart chain | shop-rest-event-relic policy; deck value | `heart_cur`; model 6 bonus; `heart_ka3` (ruby key act 3); `heart_wba3`; `heart_a4pot`; `heart_bfrelic10` (boss relics by KB + 10× simulated next-act fights) | pooled 500 Heart wins 8.2% → 12.0%; bfrelic10 12.0% vs 10.4%, P=0.986; model-6 bonus: floors replicate, Heart wins flat (−5, +1, 0) | keep | Heart |
| Heart-fight attempts | combat search; compute allocation | potion value `heart_pvh0`, rollout horizon `heart_brt8`, search reallocation `heart_hm2/hm2b`, combo, Heart deck model on 3× data | only `heart_hm2b` "−, P=0.83" and `heart_bfb25_g12` +0.006, P=0.633 given | discard | Heart |
| M5 test | statistics/method | 500 test seeds, `bfbonus10_g1` | 66.6% [62.4%, 70.6%]; vs chest1 test 54.6%: +12.0 pts, P=1.000; val 73.3% | info | A0 |
| Phase S / S2 / scale-up | rollout/leaf evaluation | learned leaf value nets replacing rollouts at equal wall-clock | large net −0.57; `sv11` −0.449 (42.0% wins); `sv12` (h=220) −0.61; scale-up (3.2×10⁶ states): `sv16` −0.348, `sv17` −0.384 | open by its rule, all lose | A0 |
| Phase R v1 / stage 2 | run value V | V on 10,000 runs; ridge on 8,100 runs; card-pick bonus | v1 cells 1–5 lose to the mean; `vrb50` +0.026, pooled P=0.884; `vrb50_nobf` −0.090 | discard | A0 |
| R2 (`vr_41`) | run value V | 90,000 A0 games; h64; card picks + 20 × V | `vr2b20` val 81.0% (+6.0), pooled 400 +4.8 pts P=0.973; A10 34.3% (+5.0, P=0.979); A20 3.6% (P=0.978); Heart 14.8% vs 12.0% (P=0.996); 10/30/50/100 discard | keep on all tracks | all |
| R3 | run value V | 90,000 more games from the vr2b20 agent, bonus 40 | `vr3b40` pooled 400 80.5% vs 78.0%, P=0.900; ports P=0.870 / 0.604 / 0.148 | keep A0 only | all |
| R4-local / R4 | run value V | A10-trained V (30k local; 90k workers; A0+A10 → `vr_81`) | holdout RMSE 0.548 / 0.544 vs A0 model 0.709; play pooled-400 P = 0.855, 0.802 (+1.3 to +1.8 pts) | discard | A10 |
| R5 | run value V; relic policy | V on boss relics (KB + 20 × gain), shop cards, upgrades | `vr5relic20` 84.7%, +0.0286, P=0.951, dev +6.0; ports A10 +0.0681 P=0.999, A20 +0.0262 P=0.999, Heart +0.0132 P=0.904; relic40 / shop / upg40 discard | keep relic hook | all |
| R6 | run value V | V on rest sites, removals; relic weight 10 | `vr6rest` −0.0290 P=0.089; `vr6rm` −0.0554 P=0.018; `vr6relic10` −0.0034 ("P=0.000" as written) | discard | A0 |
| R7 | run value V | A10-trained `vr_81` in the A10/A20 bests | `a10_r81` 44.0% val, +0.0584, P=0.942, dev +0.1116; `a20_r81` −0.0328, P=0.020 | keep A10 | A10/A20 |
| R8 / R8b → fresh | run value V | 90,000 A20 games → `vr_121`; scale-matched weights (bonus 32, relic 32) | selection seeds: 3.6% vs 5.6% P=0.009; R8b −0.0140 P=0.204. Fresh slices: `a20_r121s32f` +0.0232 P=0.972 and +0.0279 P=0.993 (6.35% vs 4.80% over 2,000 seeds) | discard, then KEEP | A20 |
| R9 | relic policy | V on shop relics | −0.0046 P=0.343; +0.0026 P=0.559 | discard | A0 |
| R10 | run value V | mixed-data `vr_121` at A0, plain and scale-matched | +0.0014 P=0.505; −0.0113 P=0.340 | discard; "scale idea dropped" | A0 |
| B20 / B20b / `a20_bm15f` | compute allocation | search moved to boss fights (×1.5, ×2 neutral; ×2 extra); less search (bosses ×0.5; sims 400) | selection seeds: 3.8% / 3.0% / 2.6% vs 5.6%; bm05 2.4%, s400 3.8%. Fresh: bm2x 4.5% vs 4.4% (dead heat); bm15f −0.0131 P=0.097 | discard | A20 |
| Selection-bias diagnostic | statistics/method | bests re-run on fresh seeds | `a20_relic20` 5.6% → 4.4%; A0 84.7 → 80.4%; A10 44.0 → 37.2%; Heart 15.0 → 12.7%; A20 6.35% | fresh-slice rule adopted | all |
| M-A10 tests | statistics/method | one test run per claim | `a10_relic20` 32.2% [28.3%, 36.4%] not confirmed; `a10_r81` 36.8% [32.7%, 41.1%] confirmed | info | A10 |
| X1 | run value V | exploration run data for V (how the exploration is done is not described in my range); `vr_151` = explore + all subsamples, h64, 413,100 runs | `x1add` 85.8% vs 83.8%, +0.0294, P 0.908 keep; `x1pure` (V only, no KB) −0.0080, P 0.395; `x1pure61` (old V only) −0.0815, P 0.001 | keep x1add | A0 |
| P1 stages 1–3 | move prior; compute allocation | prior v4 (h128, 2,000-sim targets, ≈8,000 games) at 50/100/200/500 sims; prior depth 0–2 | 50 sims: v1 −21.7, v4 −13.2 pts vs 500 sims; 100 −6.2; 200 −4.4; 500 +0.6 (P 0.668); depth 2 adds nothing for v4 | discard; v4 kept as artifact | A0 (fork) |
| Sim switch re-baseline | simulator fidelity | bests on fork dfda16c | A0 85.1 → 79.6; A10 37.2 → 36.1; A20 6.35 → 3.0; Heart 12.7 → 11.8 | info | all |
| Infra | data/infra; simulator fidelity | zlib run-data shards (72.7×), HF store, models as runtime data, Neow's Lament reserved-slot fix | recorded runs bit-identical after the fix | keep | — |

## 2. Sensitivities

- **Sims → strength, legacy A0 (100 train games):** 500 sims 83 wins, floor 48.5, 110.2 s; 50 sims 62 wins, 44.6, 10.5 s; 5 sims 33 wins, 34.7, 1.8 s. "Search is ≈ 98% of game cost."
- **Sims curve, fork, 1,000 paired games:** 50 sims / prior v1 −21.7 pts; 50 / v4 −13.2; 100 / v4 −6.2 ("~4.6x less search time"); 200 / v4 −4.4 (0.41× wall); 500 / v4 +0.6.
- **Prior quality:** v4 vs v1 = +8.5 pts at 50 sims ("closes 39% of the 50-vs-500 gap"), ~0.6 at 500. Prior on vs off at 50 sims: +24 wins / 200 (fork), 109 → 143 (legacy). Prior depth 2 with v1: +14 wins / 200 at ≈ 3× time.
- **Prior fit:** v1 49% of CE captured (8 epochs, 500-sim targets); v4 one set / 1 epoch 54%; h64 60%, h128 62% (val_ce 0.2431; uniform 0.2859, floor 0.2165).
- **Compute:** "the 4×-compute probe gave +4.5 pts", extrapolated to "~+2 pts" for a 2× faster search. A20: ×2 boss search with +47% compute = dead heat on fresh seeds.
- **Rollout quality vs breadth:** 2.43× per-simulation cost for prior rollouts → −0.062.
- **Learned leaf vs data:** sv11 −0.449 → sv16 −0.348 with 7,500 more games; root-trained nets RMSE 15.1–15.7 vs 12.1 leaf-trained.
- **V fit vs data (A0 holdout RMSE, mean 0.5451):** 1,000 runs 0.5466; 35,100: ridge 0.5135, h64 0.5128, h256 0.5321, h1024 0.5289; 89,100: ridge 0.5125, h64 0.5046, h256 0.5108; 170,100: h64 0.5019 (confounded by policy shift). A10: 40k → 80k runs moved cells ≤ 0.011; adding A0 data 0.557 → 0.544. A20 holdout: A0 model 0.8800, A10 model 0.4206, A20-only ridge 0.3452, mean 0.4233. Exploration holdout: vr_61 0.5536 / corr 0.221, vr_121 0.4631 / 0.388, vr_151 0.4592 / 0.405; at 413,100 runs h256 0.4598 vs h64 0.4592 ("bigger nets need more data" was not borne out [inferred]). Almost every net has best_epoch=1.
- **V in play:** card-pick bonus peaks at 20 (10/30/50/100 lose); +4.8 pts A0, +5.0 A10. Boss-relic hook: A0 +2.3, A10 +4.3 win pts; A20 +2.0 on selection seeds but +0.1 on fresh seeds. V-only picks: −7.0 pts with the old V, −0.4 with the exploration-trained V.
- **Deck-value bonus:** +3.0 pts (g0), +4.0 pts val (g1); weight optimum 10× A0, 25× A10/A20, 50× flat. Paired label correlations 0.098–0.18. bf_k=3 labels cost 3.5×.
- **Selection inflation:** A0 val 73.3% vs test 66.6% (~7 pts); A10 38.7% → 32.2%; A20 ≈ 1–3 win points.
- **Simulator:** fork vs legacy at 200 games: −14 / −24 wins at 50 sims, −7 at 500.
- **Elite HP gates:** A0 0.6, A10 0.75, A20 0.8.

## 3. Diagnoses

- **A0 losses are boss losses** (potsave2, val 300, counted from death logs — strong): 101 losses, 68% at bosses; Champ 15, Collector 14, Donu & Deca 10, Automaton 8, Time Eater 6, Awakened One 6, Hexaghost 5, Slime Boss 4, Guardian 1; elites Giant Head 6, Three Sentries 4, Gremlin Leader 3; hallway ≤ 2 each. "Act-2 bosses alone are 37% of all losses — the deck arriving at floor 33 is the problem." Potions left at death: mean 0.05.
- **A20 dies early** (counts, strong): 160/300 base runs die in act 1, 89 before the boss; "53% of A20 runs die in act 1 with a starter deck; Neow choice and the first two card picks are the only levers before the first elite"; Hexaghost 35 and Slime Boss 27 of ~297 deaths. "A20 runs rarely live long enough for deck quality to matter."
- **A20 bosses are not search-limited** (source tag [INFERRED, MED]): boss fights are 39% of A20 losses and 47% of search compute; boss-floor deaths 186 / 177 / 175 / 184 for ×1, ×1.5, ×2, ×2 extra — "the losses come from the deck/HP the run arrives with."
- **Heart = conversion problem** (funnel, strong): final act 1/2/3/4 = 12/61/65/162; Shield & Spear 9; died to the Heart 113; beaten 40 → "≈ 26% (40/153)". Every Heart deck-value keep moved floors, not Heart wins.
- **Deck-value model is myopic and noisy:** as a replacement, act-3 boss deaths doubled (Donu & Deca 20 vs 10); "the absolute predictor is good, the option differences are too noisy and myopic"; removals "keep immediate damage and drop what act 3 needs". Works only as an additive bonus at card rewards. No transfer across ascensions; pooling all ascensions dilutes.
- **V blind spots:** fails on rest sites and removals — "least reliable for HP trade-offs" because trained on the agent's own outcomes under those rules ([INFERRED, LOW–MED]); "helps where the KB prior is weak and the choice is between different items". Greedy-policy data predicts exploration states poorly; "exploration data is what makes V usable on its own". "A better holdout fit does not by itself improve play."
- **Combat search:** "the search's value comes from breadth, not rollout quality"; "at 500 sims the search already corrects most prior errors".
- **Ascension-specific policy:** A0 saves potions, A10 must spend; at A10 the pre-boss upgrade beats the heal, at A20 the heal wins.
- **Fork** "is a harder game, most at low search".

## 4. Mentioned but never tested (within this range)

1. **Heart-fight log analysis** (`bg_heart` + `bg_heart2`: HP and deck at Heart entry vs outcome, turns survived, damage per turn against Invincible 300) and a Heart batch built "from what separates the 26% that win" — declared next step; no result in my range.
2. **A20 act-1 survival "needs a different idea than map weights"** — no idea named; only Neow ranking (neutral) was run.
3. **2× faster simulator/search** (per-simulation BattleContext copies, action enumeration; "the earlier fixed-pile attempt was reverted for stack corruption and must be redone safely") — plan only here.
4. **Cheap agent for data generation at scale** (100 sims + v4, ~6 pts weaker at ~4.6× less search time) — flagged as "relevant for data generation", never used.
5. **Prior v4 on the full data:** trained on every 3rd game of six stored sets (≈8,000 of 24,000 games), 4 epochs, h64/h128 only (h256 dropped for time; w18's set never stored). No second distillation round.
6. **B20 anomaly:** "a pathology at high simulation counts is possible; not investigated."
7. **A20 fresh-seed re-tests never run:** `a20_bm2`, B20b (bosses ×0.5, sims 400), `a20_r121` unscaled — "their fresh-seed effects are unknown"; the same holds for `a20_r81` [inferred]. If sims 400 ties on fresh seeds, A20 compute is over-allocated [inferred].
8. **`cf_pick`** (closed): "must not be revived as is" because hypothetical fights are seeded from `gc.seed + gc.floorNum` and coincide with the real fight's streams — a revival with independent streams is not discussed.
9. **`bg1k3`** (labels averaged over three fights per option) — data generated; no model result in my range.
10. "The next A10 model should be bg1 + bg_a10 with **more A10 games**" — only a same-size regeneration (model 11) is reported.
11. **Rest/removal/shop hooks re-tested with the exploration-trained V** — R6/R9 used greedy-data V; not re-run here.
12. **Re-check of every A20 floor-driven keep on win rate "once the track reaches ~10%"** — pending.
13. **Three unresolved positive A10 V variants** (P 0.870, 0.855, 0.802) — never run at a size that resolves +1.5 pts.
14. **Retraining V on fork data** — "cost and plan to be written" (outcome outside my range).
15. Small Heart batch (potions spent at the Heart, pre-boss heal, bonus 50×, power upgrades) — planned; individual results not in my range. "A0 threshold retune" appears only in a fragment.

## 5. Method lessons

- Comparing a challenger against a best on the seeds that selected it is biased against the challenger and inflates every kept number; it produced a false discard (R8b) and a false "−3.0 points, P=0.005" (bm2x). Keep decisions need fresh slices.
- Val SE at n=300 is ~2.5 pts: "a +5.7 reading from a true ~0 is a 2σ event — expected once in ~40 keeps/discards". Keeps right at the bar are fragile: `mapd085` (P=0.913; dev exactly +0.000), `vr3b40` (P=0.900; all three ports failed), `x1add` (P 0.908; on the fork "no sign that X1's legacy gain transfers", source tag [INFERRED, MED; unpaired]).
- [inferred] R6, R9, R10 and the A10 near-misses were all judged on the best's own selection seeds before the fresh-slice rule (adopted 2026-11-05 07:50), so those discards carry the same bias against the challenger; only two A20 cells were re-tested.
- A20 / Heart are evaluation-limited: "a 2-point Heart-win effect needs n > 1,000"; floor gains and wins can diverge ("a passive policy survives longer and wins less").
- Offline metrics are weak proxies: holdout RMSE (R4, R7, R8, R10) and validation CE (P1) did not predict play; holdout RMSE is confounded by policy shift; net fits vary by 0.002 RMSE from memory layout alone.
- Learned models paid as additive bonuses on hand rules, never as replacements, until exploration data existed.
- Nothing transfers across ascensions without re-validation (elite weight, potion policy, deck model, V).
- Results across simulators are never compared; timing claims need same-session calibration (1.57 vs 0.93 s/game); shared-CPU timings are confounded.
- Compiled-in model weights broke reproducibility; models must be runtime data. Workers need owner authorization; 15 GB RAM, disk and store quotas caused repeated stalls.

## 6. Hints of a big untested lever

1. **[inferred] Search-free or near-search-free play for massive data.** Search is ≈ 98% of cost; one distillation round on a third of the data, with an h128 MLP, closed 39% of the 50-vs-500 gap (lines 692, 705, 714–719, 778–779). Iterated distillation was never run, and nothing in this range uses the resulting speed to generate orders of magnitude more games.
2. **[inferred] The strategic layer is limited by its data distribution, not model capacity.** V-only picks went from −7.0 to −0.4 pts purely by adding exploration data (lines 689–691), while nets stop at epoch 1 and ridge ≈ nets. Systematic exploration over HP trade-off decisions (rest, removal, elites, potions), where V failed (line 898–900), is untested here.
3. **[inferred] A20 is an HP/deck-on-arrival problem inside act 1** (lines 829–830, 1221, 1372–1373), yet no experiment in this range changes what combat search optimises (HP kept vs speed, judged by run value) — the combat objective stays hand-written and ascension-blind.
4. **[inferred] Decision-time simulated fights.** The only kept use (`heart_bfrelic10`, P=0.986, line 1162) and the bonus models (label correlation ~0.1–0.18) suggest signal limited by noise; many-fight evaluation with independent RNG streams (`cf_pick` was closed for a stream coincidence, lines 1297–1301) was not explored.
5. **[inferred] The Heart fight itself.** 113 of 300 runs die at the Heart and conversion is ≈ 26% (lines 1135–1142); every strategic change moved floors only. A Heart-specific deck target or fight-specific search was not found here, and the planned winner-vs-loser analysis has no recorded result.
