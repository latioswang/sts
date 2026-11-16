# h6 — history digest: program.md lines 1–105 and 1377–2012

Scope: overview/goals/workers (1–105) and early phases (1377–2012: Phase 2 sim holes, Phase A, keys/Act 4 design, batches 12–27, backlog, eval protocol, near-miss table, bf label and expert-iteration results, replication batch 44, Phase S). Numbers are quoted as written. `[inferred]` = my reading. `NIR` = result not in my range. Deltas are paired score deltas vs the then-best on val 300 unless stated.

## 1. Experiments and idea lines

| ID / name | Area | What was changed | Result | Verdict | Track |
|---|---|---|---|---|---|
| Pre-Phase-2 keep chain | shop-rest-event-relic policy | hand rules: rollout END_TURN weight, events, KB boss relics, AoE, scaling, map events, near-miss combo, no hallway potions | val 3% → 10% → 14% → 13.7% → 18.3% → 20.3% → 23.0% → 30.0% → 32.0% → 35.7%; test 33.8% before sim fixes | keep | A0 |
| `rb2`, `rb3` | simulator fidelity | player.cc init, Blood Potion, Iron Wave, Dark Shackles, Disarm+, Smoke Bomb, Ornithopter, 7 events, Colosseum, honest Dome | rb2 34.7% (n=297) vs 35.7%, neutral; rb3 38.0%, floor 40.5, score 1.174, +0.043 vs rb2 (P=0.89) | keep (new reference) | A0 |
| `nodome` | simulator fidelity | ban Runic Dome | −0.003 (P=0.38) | discard | A0 |
| enumerator; fixed piles | compute allocation | allocation-free enumerator; fixed-size card piles (+10%) | 20.2 s vs 20.7 s, no speedup; piles crashed 3/300 games | reverted | A0 |
| `spd_s650`, `spd650b` | compute allocation | sims 500 → 650 | +0.0375 (P=0.86, n=297); +0.0225 (P=0.75) | info | A0 |
| `budget_actions=7500` (`bud_base`) | compute allocation | equal total actions per decision | −0.065 (P=0.03) | discard | A0 |
| `bud_rt2/rt3/rt6`, `rt3_s750` | rollout/leaf evaluation | rollout cutoff + heuristic leaf | −0.373 / −0.285 (P=0.00) / −0.173; −0.216 | family closed | A0 |
| `esc15`, `lowhp1`, `lowhp2`, `neow2`, `maxhp05` | combat search (value knobs) | escape penalty, risk aversion, Neow, max-HP weight | +0.007 (P=0.76); +0.010 (P=0.61); −0.306; −0.017 (P=0.34); −0.023 (P=0.25) | discard | A0 |
| `chest1` | shop-rest-event-relic policy | open treasure chests (inherited handler never opened one) | 57.7% from 38.0%, floor 44.2, +0.270, P=1.000; test 273/500 = 54.6% [50.2%, 58.9%] | keep; "largest single gain of the program" | A0 |
| leaf V: outcome-regression; bootstrapped | rollout/leaf evaluation | MLP 204→64→1 replaces rollouts; targets = battle outcome, then search root mean | RMSE 12.4 vs 28.2, floors 6/11 at 500 sims vs 33/23/44; RMSE 9.73 vs 33.9, 0%/16, 5%/20, 25%/26 at 500/1,500/3,000 sims vs 60%/46.7 (20 dev seeds) | closed "at this data scale" | A0 |
| `prior1`, `prior1_s450`, `prior2`, `prior2_s450` | move prior | bilinear PUCT prior (31% of information) | −0.010 (P=0.39), −0.004 (P=0.45); +0.002 (P=0.52), +0.022 (P=0.73) | discard | A0 |
| `priormlp_d0` | move prior | MLP prior (49% of information), root only | 61.3%, floor 44.6, score 1.488, +0.044 vs chest1, P=0.900 | keep (first learned component) | A0 |
| prior variants | move prior | depth-1 use; PUCT c 1.0/0.7/0.5/1.2; gen2-distilled (2,000-sim targets); 465k decisions + hidden 128; temperature 0.7/1.5 | "−0.01 at equal wall-clock and at +compute"; `pm_c10` 64.0% +0.031 (P=0.82), 62.3%, 62.7%, `pm_c12` +0.016 (P=0.70); `priormlp2_d0` 58.3% −0.034 (P=0.15); `priormlp3_d0` 62.7% +0.014 (P=0.65); `pm_t07` −0.145, `pm_t15` −0.026 | all discard; c sweep closed | A0 |
| gen2 | compute allocation | 2,000-sim self-play on train seeds | 65.8% win; "+4.5 pts for 4× sims" | info | A0 |
| `rvpick1`, `rvpick2`, `rvpick2_m` | run value V | observational V_run, one-step-lookahead picks | −0.343 (32.7%); −0.333; −0.621 | closed | A0 |
| `cfpick1`, `cfpick2` | deck value | online counterfactual fights (3×60, 4×100 sims) | −0.267 (37.7%); −0.224 (42.3%) | closed in this form | A0 |
| `bossprep1/2/_h/_a10` | deck value | per-boss card bonuses | −0.026 (P=0.23); +0.004 (P=0.54); Heart −0.019; A10 +0.009 | closed | A0/Heart/A10 |
| `kbsyn1`, `kbsyn2` | deck value | KB synergy pairs (1,422 card-card, 1,741 relic-card) | 63.3% −0.007 (P=0.43); 60.0% −0.037 (P=0.18); `kbsyn_rel`, `a10_kbsyn` NIR | discard | A0 |
| `ocpot1`, `ocpot_h` | shop-rest-event-relic policy | out-of-combat potions | −0.022 (P=0.26); Heart 6.0% | discard | A0/Heart |
| `INFO_keys1` | path/map | keys=1 + chest policy | 35.0% reach Act 4 (105/300); 19 Heart wins = 18% conversion, 6.3% of runs | info (Heart baseline) | Heart |
| `wb3`, `krh05`, `act4prep1` | path/map; deck value | burning-elite weight 3; ruby key at 50% HP; Act-4 card bonuses | Heart wins 6.0% / 7.0% / 5.0% vs 6.3% ("all noise-level"); deltas −0.015 (P=0.10), −0.036 (P=0.01) [pre-correction metric, inferred], −0.014 (P=0.12) | discard | Heart |
| `base_a10`, `a10_rest06` | shop-rest-event-relic policy | A0 params at A10; heal at rest below 60% | 16.3%, floor 30.8; +0.040, P=0.988 → 18.7% | keep | A10 |
| `a10_elite075`, `a10_ct4`, `a10_combo`, `a10_eh08`, `a10_wrl4`, `a10_ct35` | path/map; policy knobs | elite / card-threshold knobs | +0.031 (P=0.84); +0.028 (P=0.89, 20.3%); +0.030 (P=0.78); +0.023 (P=0.74); +0.014; +0.007 | discard | A10 |
| `bfbonus10_g1` | deck value | interventional bf model as card-value bonus | val 73.3% (M5); test 66.6% [62.4%, 70.6%], +12.0 pts vs chest1 test; dev +0.051, dev2 +0.171 (P=0.996), dev3 +0.147 (P=0.988) | keep, replicated on 3 slices | A0 |
| `a10_bfbonus25_g1m`; `a20_bfbonus10_g2m` | deck value | model-2 bonus; model-3 bonus | A10: dev +0.035 (P=0.74), dev2 +0.161 (P=0.991), dev3 +0.205 (P=1.000). A20: pooled +0.0192 (P=0.935), dev2 +0.007 (P=0.608) | A10 keep replicated; A20 keep not replicated | A10/A20 |
| bf variants | deck value | bonus weight 25/50, threshold `ct45`; expert iteration (models 8, 7); 3-fight label averaging (models 9, 10) | pooled P=0.609 / 0.780 / 0.732; `bfbonus10_g1b` 72.0% −0.019 (P=0.285), `a10_bfbonus25_g_a10b` 24.0% −0.060 (P=0.031); `_g1k3` 72.7% −0.008 (P=0.402), `_g1k3p` 74.3% +0.011 (P=0.639) | discard; "strategic model line is closed on A0 at g1" | A0/A10 |
| near-miss extensions | shop-rest-event-relic policy; run value V | `potsave`, `potsave2` (save potions for bosses); `vrb50`, `vr3b40`, `a10_vr3b40` (vr weights [inferred]); `a10_potv2`; `a10_wev25` (event rooms 2.5) | pooled P=0.866, **0.949**; 0.884, **0.900**, 0.870; 0.811; 0.743 | `potsave2`, `vr3b40` keep | A0/A10 |
| `a20_restboss1` | shop-rest-event-relic policy | always heal at pre-boss rest | pooled 400: win 0.035 vs 0.030, +0.0138, P=0.912 | keep (A20 best) | A20 |
| a10ports | path/map; policy | five A0 keeps ported | `a10_mapd085` −0.039 (P=0.011); `a10_potsave` +0.001 (P=0.517); `a10_wea1` −0.010 (P=0.331); `a20_mapd085` pooled-600 +0.0033 (P=0.704); `a20_potv8` pooled −0.0287 (P=0.002) | 0 of 5 keep | A10/A20 |
| Phase S `sv1`–`sv9`, `svctrl`, `sv*_s500` | rollout/leaf evaluation | 3×3 data × size grid; leaf replaces rollouts at equal wall-clock; fixed-500-sim reads | best `sv7` 36.0%, −0.5653; control 11.7%, −0.9548; base `mapd085` 75.0%; `sv1_s500` win=0.047, −1.1490; `sv5_s500` win=0.283, −0.7153; `sv9_s500` NIR | no cell near base | A0 |
| s2scale `sv16`, `sv17` | rollout/leaf evaluation | 4.2× leaf rows (3,225,306) | RMSE 11.736 (h=42), 11.216 (h=96) vs sv_11 12.060; play NIR | info | A0 |

## 2. Sensitivities

- **Search compute:** 4× sims → "+4.5 pts" (65.8% on train seeds vs 61.3% val; unpaired). +30% sims → +0.0375 / +0.0225, below the keep bar.
- **Policy hole (chests):** +0.270, 38.0% → 57.7%. **All sim fixes:** +0.043 (P=0.89); player.cc fix alone neutral.
- **Root MLP prior:** +0.044. Information captured 31% → 49% → 50% (more data, bigger net): no play gain past 49%. Sharper 2,000-sim targets: −0.034. Sharpening temperature: −0.145.
- **Rollout truncation + heuristic leaf:** −0.373 / −0.285 / −0.173 for rt2/rt3/rt6 [inferred: 2/3/6 turns].
- **Fixed action budget instead of fixed sims:** −0.065.
- **Leaf value, fit vs play (Phase S):** holdout RMSE 16.59 → 5.65 across the grid (predict-mean 33.57), play flat beyond 10⁵ decisions. By data: small net −1.12 → −0.58 → −0.57; mid net −0.72 → −0.70 → −0.63 ("+0.07 per decade", "≈ 9 more decades of data to reach the base"); large net (50-sim floor) −0.74 → −0.71 → −0.67.
- **Inference cost (candidate/best wall-clock):** 0.400 at 10⁴ params; 3.326 and 4.656 at 9.4×10⁴; 37.8 and 83.791 at 9×10⁵; heuristic leaf 0.146.
- **Leaf decomposition:** learned leaf ≈ +0.38 score (+24 points of win rate) over the heuristic leaf; random rollouts ≈ +0.57 over the best learned leaf. 3.3× more simulations for the mid net buys nothing (−0.7153 vs −0.6952).
- **Leaf data:** 4.2× rows moves h=42 only 12.06 → 11.74 ("capacity-limited"); h=96 reaches 11.22.
- **bf deck value:** g0 → g1 "+4 pts from better labels"; 2× games −0.019; label averaging raises paired corr 0.14 → 0.19 but −0.008; A10 model with 1,500 games −0.060 vs 1,000 games.
- **Online counterfactual picks:** 3×60 sims −0.267, 4×100 −0.224, cost 2.3× per game.
- **Keys:** raising Act-4 reach from 35.0% to 36.7–38% does not raise Heart wins.
- **Ascension:** A0 best params give 16.3% at A10. Val → test: 73.3% → 66.6% and 57.7% → 54.6% [inferred: val optimistic by 3–7 points].

## 3. Diagnoses

- **Bosses dominate losses.** chest1's 127 losses: Act 1 hallway 4, Act 1 boss 20, Act 2 hallway 13, Act 2 boss 35, Act 3 hallway 20, Act 3 boss 35; "Bosses are 71% of losses." Later: "57% of the A0 best's losses and 30% of all A10 runs end at an act boss." Direct counts; strong.
- **"Combat search is not the main bottleneck — the deck is."** Rests on one unpaired 4× probe plus the boss share. Medium.
- **Heart:** ~90% of Act-4 deaths at the Heart (floor 55), ~10% at Shield & Spear; Heart wins finish with ~28 HP → "the Heart's damage race is the constraint, not reaching it"; "Act-4 conversion tracks overall deck strength, not Act-4-specific card bonuses." Counts plus five neutral/negative experiments. Medium-strong.
- **Leaf V → passivity:** every row of a battle shares one label, so V is "nearly indifferent to within-turn progress" and search ranks siblings by model bias. Strong (two target types, nine Phase S cells).
- **Mid-size leaf is limited by value error, not depth** (`sv5_s500`); source's own [INFERRED]: RMSE 10.7 ≈ 30% of the win bonus (35). It "contradicts the 'inference is the wall' reading" for the mid net.
- **Observational V_run → deck bloat:** "bigger deck / more relics ⇒ survived longer ⇒ wins." Strong.
- **Online counterfactual picks:** winner's curse over 3 noisy candidates + skip; weakly played fights "reward cards that help weak play." Asserted, not isolated.
- **Fixed action budgets** "starve the long, complex fights that matter most." **Risk aversion → passivity** (`lowhp2`).
- **Prior:** the remaining 51% is "largely irreducible at 500-sim targets"; sharper targets "over-focus the root at c=1.5."
- **bf expert iteration:** current agents' decks are already shaped by the bonus, so option sets "carry less contrast" ("likely reason").
- **A20:** "floor ≈ 26–27, wins 1–4% (noise)"; nothing solid after `a20_restlo`. Expectation only: Ascender's Bane and the double boss make "card-removal priority matter more."
- **Parameter tuning exhausted** (declared at batch 14 and again at batch 25). The built-in cheating agent scored only "~2% (dev)", so it is no ceiling [inferred].

## 4. Mentioned but never tested (in this range)

1. **cf_pick as veto only:** KB's single top candidate vs skip, ≥8 fights, a margin; cost ≈ 2×. Backlog.
2. **Iterated expert iteration for combat V/π** ("regress V on those (AlphaZero-style), iterate"). Only one-shot fits; data always came from the rollout agent [inferred].
3. **Prior-guided rollouts at measured equal wall-clock** — named a "next structural item" after batch 25; NIR.
4. **Learned leaf with rollouts kept** (hybrid). The `leaf_vf` hook sits "at rollout cutoff", but only `rollout_turns=0` results appear [inferred gap].
5. **Inference hardware:** "a GPU or a batched evaluator" — never tested; the gate `sv9_s500` was pending at the end of the range.
6. **Phase S continuation to 10⁷ decisions** (25,000 games, ≈ 7 h) — deferred.
7. **Counterfactual-rollout data augmentation at decision points** for V_run (Phase C plan).
8. **Direct bf decisions** `bfpick_g0`, `bfpick_g0_m02`, `bfupg_g0`, `bfrem_g0` — pipelined; only the bonus form has results here. NIR.
9. **Another bf label family.** Labels = one act-boss fight + one hallway/elite fight, `bf_sims` 100, no potions; "at the ceiling of this label family." No longer-horizon, stronger-play or with-potion labels [inferred].
10. **Unstruck backlog:** `aoe_bonus=3`; `card_size_pen` 0 / 0.2; `shop_card_thresh=6`; `rest_heal_frac` 0.4 / 0.6; `potion_v` 2 / 8; `hp_w=2`; `end_turn_w=0.02`; upgrade priority beyond KB tier (Bash, key powers first); `scaling_bonus 3` / `scaling_want 3`, upgrade scaling cards first; `sims=1000` (informational only). Names like `a10_rest06`, `a10_potv2`, `a20_potv8` suggest some were tried on other tracks [inferred].
11. **Keys/Heart:** skip the sapphire key when the chest relic is rare; `chest_cursed_open=1`; A10/A20 ladders on the Heart track; "Heart-specific prep" beyond hand bonuses.
12. **Disabled content:** Match & Keep; Prismatic Shard (only Ironclad + colorless cards implemented).
13. **A20 card-removal priority** — no experiment here.
14. **Depth-1 prior with prior_c=1.0** (n=20 probe: 80%/49.0) — val result for that setting NIR.
15. **sims=650 adoption** if the build is ≥1.3× faster; standing rule that verified speedups convert into more simulations. Outcome NIR.
16. **A10 near-misses never extended:** `a10_elite075` (P=0.84), `a10_ct4` (P=0.89), `a10_combo` (P=0.78).
17. **Cheating-combat diagnostic** (backlog #1, marked tried): its number is NIR; no A20/Heart version here.
18. **Phase F:** Silent port; publishing engine + seed sets + protocol. **Phase E:** fuzz runs on every sim change (0 crashes per 1,000 games); status NIR.
19. **Worker items:** R2 batch (`r2w1`, `r2w2`), v1 cell 9 (9,000 runs, h=1024; dropped), `sv16`/`sv17` play tests — outcomes NIR.

## 5. Method lessons

- Keep rule: dscore > 0 and paired bootstrap P ≥ 0.9 on val 300. A near-miss (0.7 ≤ P < 0.9) may be extended once, declared in advance, and the fresh slice alone must be positive. Text tally "3 kept of 9"; with three later table rows [inferred] 4 of 12. Bonus-weight and threshold families were "val-only artefacts."
- Replicate keeps on independent slices: A0 and A10 bf keeps held on three; A20 keeps after `a20_restlo` did not.
- Ports across ascension fail (0 of 5, as predicted); one A0 keep is significantly negative at A10.
- Fit metrics do not predict play: leaf RMSE, prior CE and bf paired correlation all improved without play gains.
- Observational models cannot be used counterfactually; argmax over noisy estimates needs margins and common random numbers.
- Equal compute = equal sims at calibrated wall-clock on an idle machine (the "~13%" speedup was contention noise). Every speedup needs a ≥300-game crash sweep.
- n=20 probes are "suggestive only" (SE ≈ 11 pts).
- Data integrity: three logger bugs, one aborting 146/1,000 games and biasing the data. Assert row and label sanity before training; export a placeholder when a model fails the base rate.
- Metric definitions bite: `win` counted key-less Act-3 clears ("47% conversion" was 18%). The frozen protocol says `floor/55 + win`, the header `floor/51 + win`.
- Compiled weights are part of the agent; the harness is deterministic.
- A pre-registered continuation rule can be "met only in form"; add controls (`svctrl`) before reading a grid.
- Test budget is spent (one further run per confirmed milestone). 4 vCPUs per VM; throughput comes only from more sessions; GB-scale data cannot travel through git.

## 6. Hints of a big untested lever (all [inferred])

1. **Action-coverage audit.** The largest gain ever (+0.270, lines 1527–1548) was an action the agent never took, not a model. Disabled or unexamined actions remain (Match & Keep, Prismatic Shard, Cursed-Key chests, sapphire-key choice). A systematic "what does the agent never do" audit per track has the best historical return.
2. **Rollouts and learned value are complementary, but only replacement was tested.** Learned leaf +0.38 over heuristic, rollouts +0.57 over the best leaf (lines 1935–1940). Informed rollouts or leaf + short rollout were named (line 1646) with no result here.
3. **Longer-horizon interventional strategy.** The only learned component with large replicated gains is the crude bf bonus (two fights, 100 sims, paired corr ≤ 0.20; lines 1649–1668, 1830–1840), and its label family is "at the ceiling." Paired multi-fight or act-level continuations, as labels or as online planning, were never tried.
4. **Compute where the losses are.** Bosses are 57–71% of losses and uniform budgets "starve the long, complex fights" (lines 1391, 1452–1454). Fight-aware allocation at equal per-game wall-clock is absent here and needs an owner ruling on the fixed-compute rule.
5. **Track-native models for A20/Heart.** A0 ports fail, the A20 bonus does not replicate, and the Heart is a damage race (lines 78–94, 1400–1402, 1841–1844). Deck-value labels generated at A20 against the double boss or the Heart itself do not appear here.
