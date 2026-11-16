# h7 — history digest of `research/program.md` lines 2013–2994

Scope: Phase R (learned run value V, `vr_*`) R–R10, P1/X1 pre-registrations, B20/B20b, fresh-slice rule, Phase S gate + S2 (learned leaf), magic-number audit, pooled rules, batches 48–53 (Heart/A20), speed work, operational lessons, stop condition, caveat. The range is not chronological (later phases appear first). Numbers are quoted as written. "n.i.r." = result not in my range (only the pre-registration, or a later back-reference, is). dscore = paired score difference vs base; P = P(improve).

## 1. Experiments and idea lines

| ID | area | change | result | verdict | track |
|---|---|---|---|---|---|
| R v1 grid (vr 1–9) | run value V | MLP h=64/256/1,024 on 666 run features, target min(floor,51)/51+win, raw inputs | every pasted cell loses to predict-mean (0.5466–0.5704 vs 0.5454), corr 0.173–0.256; cells 7–9 OOM-killed | info (trainer flawed) | A0 |
| R v2 grid + ridge (11–22) | run value V | standardised inputs, early stop, 16 rows/run | only h=64 @8,100 runs beats mean (0.5309); ridge 0.5221 / 0.5199 / 0.5135 (corr 0.339) -> `vr_10` | info | A0 |
| `vrb20/50/100`, `vrb50_nobf` | run value V | card value += bonus x (V(deck+card) − V(deck)); nobf = V replaces deck-model bonus | −0.020 P=0.262; +0.026 P=0.761 (ext. pooled-400 +0.0386 P=0.884); −0.050 P=0.107; nobf −0.090 P=0.011 | discard all | A0 |
| R2 `vr_41` + `vr2b20` | run value V | +90,000 games (89,100 train runs), per-state weights, h=64 | holdout 0.5046, corr 0.387; `vr2b20` 81.0% win, +0.065 P=0.978; pooled-400 78.0% vs 73.3%, +0.0548, P=0.973 | keep ("first keep driven by a learned value") | A0 |
| `vr2b10/30/50/100` | run value V | bonus sweep vs `vr2b20` | −0.048 P=0.052; −0.044 P=0.031; −0.021 P=0.238; −0.105 P=0.002 | discard | A0 |
| ports `*_vr2b20` | run value V | A0-trained V, bonus 20 | A10 34.3% win, +0.071 P=0.979 (dev +0.085 P=0.918); A20 pooled-500 3.6% vs 2.8%, +0.0312 P=0.978; Heart 14.8% vs 12.0%, +0.0513 P=0.996 | keep x3 | A10/A20/Heart |
| R3 `vr_61` + `vr3b10/20/40` | run value V | 170,100 train runs, data from the stronger agent | h=64 0.5019 corr 0.397 (h=256 0.5042, h=1,024 0.5074, ridge 0.5141); b10 −0.033, b20 −0.019, b40 +0.023 P=0.758 -> ext. pooled-400 80.5% vs 78.0%, +0.0392, P=0.900 | b40 keep (exactly at the bar) | A0 |
| ports of R3 model | run value V | `vr_61` at bonus 40 and 20 | A10 b40 pooled-400 +0.0348 P=0.870, b20 +0.017 P=0.684; A20 b40 +0.0037 P=0.604, b20 −0.0253 P=0.038; Heart b40 11.8% vs 14.8% P=0.148, b20 11.6% vs 14.8% P=0.022 | discard all | A10/A20/Heart |
| R4-local `vr_82` | run value V | 40,050 A10 runs (+A0 data) | A10 holdout 0.5481 vs `vr_61` 0.7093; `a10_vr4lb20` −0.0059 P=0.437; `lb40` +0.0374 P=0.856, ext. pooled-400 +0.0331 P=0.855 | discard | A10 |
| R4 proper / R7 (`vr_81`) | run value V | 90,000 A10 games; `vr_81` on cards + relics | n.i.r.; back-references: A10 holdout 0.544; card picks alone pooled-400 P=0.802; R7 "kept on A10 (+5.3 val, +7.0 dev) but lost on A20 (pooled −1.6 win, every slice negative)" | A10 keep / A20 discard (referenced) | A10/A20 |
| R5 `vr5relic20/40`, `vr5shop`, `vr5upg40` | shop-rest-event-relic policy | ΔV added to boss-relic value, shop-card value, upgrade choice | relic20 84.7% win, +0.0286 P=0.951 (dev +0.0696 P=0.983); relic40 −0.0080 P=0.000; shop −0.0035 P=0.417; upg40 +0.0017 P=0.540 | relic20 keep; rest discard. Ports n.i.r.; later text: boss relics kept "on all four tracks" | A0 |
| R6 `vr6rest`, `vr6rm`, `vr6relic10` | shop-rest-event-relic policy | V replaces rest-vs-smith HP rule and removal order | n.i.r.; later text: V "lost where it replaced HP / removal rules (R6)" | discard (referenced) | A0 |
| R8 `vr_121`, `a20_r121`; R8b `a20_r121s32` | run value V | 90,000 A20 games; mixed A20+A10+A0 net (332,100 runs); R8b rescales weights by prediction SD | A20 holdout 0.3384 corr 0.603 (A20-only 0.345, `vr_81` 0.421, `vr_61` 0.880); play 3.6% vs 5.6%, −0.0366 P=0.009; R8b n.i.r. ("scale matching did not rescue A20") | discard | A20 |
| R9 `vr9srel/srelg`; R10 `a0_r121/s` | policy / V | shop relic by ΔV; mixed-data model on A0 | play n.i.r.; R10 fit on fresh A0 holdout: `vr_121` rmse 0.4012 corr 0.328, `vr_41` 0.4277 / 0.283, `vr_61` 0.4769 / 0.146, `vr_81` 1.6605 / 0.041 | pre-registered only | A0 |
| X1 | data/infra (V) | `explore_eps=0.2` random card / boss-relic picks, 90,000 games; pure-V picks (`vr_pure`) | n.i.r. | pre-registered only | A0 |
| P1 | move prior | prior depth {0,1,2} x c {1.0,1.5,3.0} at 50 sims; prior v4 on 2,000-sim targets (28,000 games); fidelity fork | n.i.r. (diagnostics in §2) | pre-registered only | A0 |
| B20 / B20b | compute allocation | budget moved to bosses (`bm15`, `bm2`), +47% (`bm2x`), less search (`bm05`, `s400`) | pooled-500 wins 3.8% / 3.0% / 2.6% / 2.4% / 3.8% vs 5.6% — against a selection-inflated base; `bm2x` "−3.0, P=0.005" "was a dead heat on fresh seeds"; fresh re-tests `a20_bm15f`, `a20_r121s32f` n.i.r. | discard, then doubted | A20 |
| fresh-slice rule | statistics/method | each test uses a never-used slice, base re-run on it | `a20_relic20` 5.6% (selection slices) vs 4.4% (1,000 fresh) | adopted | all |
| Phase S gate | rollout/leaf evaluation | root-trained value net as leaf, cost ignored (`sv9_s500`) | −0.5724, win 35.7% vs base 77.0%; heuristic leaf `svctrl` 16.0%, −0.9198 | closed; "no batched evaluator, no 10⁷-decision run" | A0 |
| S2 `sv11/12/16/17` | rollout/leaf evaluation | value net trained on tree-leaf states | `sv11` 42.0%, −0.4489; `sv12` (h=220, 87 sims) −0.6135; `sv16` (4x data) 48.7%, −0.348; `sv17` (h=96) −0.384 | "scaling"; left open in range | A0 |
| magic-number audit | deck value | fit the 8 deck-head weights to run score | CV corr 0.288 -> 0.301 | info | A0 |
| `ldw35`, `ltw1`, `brt8` | rollout/leaf evaluation | loss-value damage weight, survival credit, boss rollouts cut at 8 turns | Heart −0.0205 P=0.111 / −0.0026 P=0.436 / −0.0329 P=0.011; A20 −0.0239 P=0.024 / −0.0057 P=0.312 / −0.0409 P=0.000 | "value-shape line closed at both ascensions" | Heart/A20 |
| `heart_pvh0`, `heart_a4pot` | shop-rest-event-relic policy | no potion credit in Heart fight; act-4 shop buys potions | −0.0020 P=0.251; +0.0160 P=0.996 (9.8% vs 8.2%) | discard; keep | Heart |
| `esim` family | path/map | elite node valued by 3 simulated 100-sim fights | Heart +0.0023 P=0.573; A20 −0.0154 P=0.108; `esim3` identical games; `esim_a23` pooled-600 +0.0061 P=0.807; A0 −0.0493 P=0.003 | closed on three tracks | Heart/A20/A0 |
| batch 49/52 keys | path/map | burning-elite and ruby-key rules; undiscounted key channel | `wba3` pooled-600 P=0.958 keep; `ka3` P=0.949 keep (chain 8.2% -> 9.3% -> 9.8% -> 10.2%); `bhp05`, `wb4`, `kpb08` discard; `krw1` vs `ka3` P=0.649 (500) / 0.746 (600); `kud` +0.0009 P=0.539; `kud_krw` pooled-600 +0.0123 P=0.806; `heart_combo` −0.0038 P=0.425 | 2 extension keeps; "key acquisition is closed as a lever" | Heart |
| `heart_hm2`, `hm2b` | compute allocation | 2x sims at the Heart | hm2 (+3.8% compute) 13.6% vs 10.4%, P=0.995, informational; neutral hm2b pooled-600 +0.0140 P=0.817 | no keep | Heart |
| batch 50 | deck value | +1,000 Heart-labelled games, model 12 | boss_bce 0.4046 vs base 0.6739; pair_corr 0.094 (export line) / 0.155 (per-epoch val), pair_sign_acc 0.593; play n.i.r. | open in range | Heart |
| batch 51 ports | move prior / policy | four A0 keeps ported | `pmlp` −0.0059 P=0.365; `potsave` −0.0145 P=0.143; `wea1` −0.0189 P=0.064; `mapd` −0.0038 P=0.272 (the verdict sentence says −0.007) | none transfers | Heart |
| batch 53 `bfrelic` | shop-rest-event-relic policy | boss relic by simulated fights (K=2, 100 sims) | A0 +0.0020 P=0.559, w=25 −0.0070; A10 −0.0005 P=0.498; A20 pooled-600 +0.0030 P=0.743; Heart 12.0% vs 10.4%, +0.0181 P=0.986 | Heart keep only | all |
| `a20_neow1p` | shop-rest-event-relic policy | Neow ranking | −0.0215 P=0.086 | closed on all tracks | A20 |
| speed | data/infra | gprof profile; move overloads, model preload | 1.029x | kept; line parked | all |
| fidelity fork `dfda16c` | simulator fidelity | P1 runs on the fork | 100 A0 games: old sim 84 wins / floor 48.3 / 99 s vs fork 81 / 47.3 / 104 s | info | A0 |

## 2. Sensitivities

- Search depth (A0, 100 games): 500 sims 110 s / 83 wins; 50 sims 10.5 s / 62; 5 sims 1.8 s / 33. Search ≈ 98% of game cost. "The 4×-compute probe (+4.5 pts)". +3% sims ≈ +0.1 pt.
- Move prior (200 train games): 50 sims + prior 143 wins (71.5%); 50 sims without 109 (54.5%); 500 sims without 166 (83%) — prior "worth +17 points at 50 sims", 11.5-point gap to 500 sims at 8.8x less time. The root MLP prior is elsewhere quoted as "+3.6 pts at A0". Prior v1 (298k decisions, 500-sim targets, root only) "plateaued" (v3 neutral).
- V fit vs data: unweighted h=64 0.5551 / 0.5479 / 0.5309 (800 / 2,700 / 8,100 runs); weighted 0.5230 / 0.5128 / 0.5046 (8,100 / 35,100 / 89,100); 0.5019 at 170,100; ridge flat (0.5135 -> 0.5141). Corr 0.339 -> 0.387 -> 0.397. Every net >= 2,700 runs peaks at epoch 1–2. Bigger nets never best at A0.
- V play value: ridge 8.1k runs +0.0386 (P=0.884); 89k-run net +0.0548 (+4.8 win pts, pooled 400); 170k-run net a further +0.0392 (+2.5 win pts, P=0.900), A0 only.
- V weight: R2 model peaks at bonus 20 (10 / 30 / 50 / 100 all lose to it); boss-relic weight 20 keeps, 40 loses (P=0.000); weight 10 n.i.r.
- Additive vs replacement: replacement −0.090; "deck model as replacement −10 to −16".
- Weighting fix: per-run mean 1.5645 vs per-state 1.6892 — a ≈ 0.125 offset that lifts RMSE 0.518 to ≈ 0.533.
- 16-state cap + policy shift cost ≈ 0.003 RMSE.
- Track fit: `vr_61` on A10 holdout 0.7093 vs 0.548–0.557 A10-trained; on A20 0.880 vs 0.338. Policy shift: `vr_61` corr 0.146 on the current agent's games.
- Leaf value: root-trained nets leaf RMSE 15.1–15.7, bias ≈ +3 (root RMSE 10.2 / 8.0 / 5.65); leaf-trained 12.06, bias +0.06. Leaf data slope (h=42): 10⁴ rows 17.887 -> 10⁵ 13.363 -> 7.7×10⁵ 12.060 -> ≈ 3.3×10⁶ 11.736 (−2.7%, different holdout). "4× leaf data bought +0.10". Cost removed: best-fitting net +0.05 over the small net, +0.11 over itself at 50 sims, "still 0.57 below random rollouts". Net cost: h=42 0.730x (685 sims), h=96 1.415x (354), h=220 5.734x (87).
- Deck-head weights learned: CV corr 0.288 -> 0.301; 8 heads ≈ 3 independent signals.
- Compute split: A20 boss fights ≈ 0.469 of search compute; Heart fight ≈ 10% of a run's search actions.
- Keys: key-less clears 37 -> 18 gave ≈ +4 Heart wins per 300; keys ≈ 96% complete (`kud`) moved nothing; Heart conversion 20%.
- Costs: `esim` +37% (Heart), +54% (A20); `bfrelic` +28%; root MLP prior 4.21 s vs 3.18 s per game.
- Selection inflation at A20: "≈ 1–3 win points". Selection-slice levels of the bests on 2026-11-05 (from the re-measurement predictions): A0 84.7%, A10 44.0%, Heart 15.0%, A20 5.6%. Targets quoted in range: A10 35%, A20 25%.
- Earlier failures cited as priors: Phase C V_run (P(win) only, ~2,000 runs) lost as a card-pick replacement; "expert iteration failed at A0 (models 7/8)" for the deck-value model; global rollout cutoffs (3/6 turns, all fights) "lost badly at A0"; A0 `realloc` (400 sims, bosses x2.25) −0.042, P=0.067.
- V is called ≈ 50 times per run ("no inference wall"); ≈ 158.0 states per run (wins 176.4, losses 120.8).
- Parameters: 161 named, 97 tested, 51 never.

## 3. Diagnoses

- A20 deaths (val 300): act 1 81 (53 at the boss), act 2 156 (64 boss, 40 elites: Slavers 19, Gremlin Leader 11, Book 10), act 3 51; "150 of 288 deaths are boss fights". `a20_relic20` pooled 500: boss deaths 186 of 472 losses (floor 16: 84, floor 33: 102). Strong (counts).
- "In a boss fight the random rollouts cannot win"; the only gradient is `10 × damage fraction + 0.2 × turns survived`, without player HP. The repair attempts (`brt8`, `ldw35`, `ltw1`) all lost: "HP is invisible in boss-fight values" is rejected. The premise stands; no fix found.
- A20: "the lever is deck strength by act 2, not elite selection" (act-2 elite deaths 38 vs 40 with the gate). Moderate (one experiment).
- Elite-sim gate: 100-sim fights without potions underestimate the 500-sim agent; the test starts from current HP before the preceding hallway fight; outcomes are all-won or all-lost.
- Heart (val 300): 104 deaths at the Heart, 66 act-3 boss, 44 act-2 boss; potions_left at death 0.11; 85 of 600 runs (14%) cleared act 3 without keys. After key fixes: "what remains is conversion". The source labels two keeps as conversion gains: `heart_a4pot` and `heart_bfrelic10`.
- Relic simulation "helps only where the next fights are the Heart and Shield & Spear (a known, fixed pair)"; it is blind to out-of-combat drawbacks.
- V "kept wherever it chose between *items*" and "lost where it replaced HP / removal rules"; "every pure-V replacement so far lost". Stated cause: V sees only states the KB-guided agent reaches, while decisions need counterfactual differences. Hypothesis (X1 n.i.r.).
- Better fit is not better play: A20 `vr_121` (corr 0.603) lost 2.0 win pts; sharper `vr_61` hurt A20/Heart at both weights ("the model ... is the problem off A0"); `sv12` lower RMSE, worse play.
- Deck model: "the ceiling is how well the heads predict the run, not how they are weighted"; caveat that level, not with-minus-without difference, was measured.
- A0 tuning does not carry to the Heart track (0 of 4 ports); the prior "steers the Heart fight worse than uniform priors" (post-hoc).
- Search cost "spread thin", nothing above 9%; "the remaining cost is architectural".
- The Heart is "the one fight where rollout noise at 500 simulations plausibly costs wins (750 HP, Beat of Death, Invincible)" — unproven (P=0.817).
- B20 +compute loss "stays unexplained"; later attributed to selection bias [INFERRED, MED in source].

## 4. Mentioned but never tested (within this range)

1. Phase R stage 3: fight terminal value = V(state after the fight) replacing win bonus 35, 1 per HP and potion constants — "does not start"; no later start in range.
2. Batched leaf evaluator and the 10⁷-decision leaf run — cancelled by rule. Leaf line "closed on this CPU": other hardware never mentioned.
3. S2 next step: more leaf data ("the slope is the open question").
4. Simulator action-queue rewrite for a 2x speedup ("multi-day job with real risk") — parked.
5. Per-fight budget accounting in the harness ("a cleaner version" of Heart reallocation).
6. Mechanism check of high-sim search: "potion use / rollout horizon at high sims".
7. The 51 never-tested parameters; KB tables (73 card values, 21 boss-relic values, synergy pairs, event rules); hard-coded encounter weights in the hypothetical-fight sampler. The user's principle (every constant derived from V) was applied only to card picks and boss relics.
8. Deck-head weights as a play test; the with-minus-without accuracy of the heads.
9. X1: exploration data, pure-V play (`x1pure`); exploration is limited to card rewards and boss relics.
10. P1: prior used deeper than the root, exploration constant, 2,000-sim targets, 50-sim agent "within 3 points".
11. R9 shop relics by V; R10 mixed model on A0; honest re-measurement of A0/A10/Heart; fresh re-tests `a20_bm15f`, `a20_r121s32f` — all n.i.r.
12. `heart_bfb25_g12` (model 12) — "running".
13. Rest-site seeking on fresh slices on top of `ka3`; `heart_combo` "noise until a larger evaluation says otherwise".
14. A10 bonus 40: two near misses, "below what 400 games resolve" — no larger test.
15. Elite-sim repairs implied by its diagnosis (potions, more sims, HP after the hallway fight) — closed instead.
16. Relic simulation that sees out-of-combat drawbacks — never built.
17. A move prior trained on keys-track / act-4 games [inferred from the `heart_pmlp` post-mortem].
18. Remaining 18 key-less clears (emerald path never reached at ≥ 60% HP; no eligible rest).
19. V beyond 666 flat features / h=1,024, or beyond 1.9×10⁵ A0 runs [inferred: not mentioned].
20. Any change to the rollout policy itself (only leaf values, cutoffs and loss shaping appear here) [inferred: not mentioned].
21. Final `test` run + `REPORT.md` per the stop condition; milestone test runs (none reached; "the M-A10 run is spent").

## 5. Method lessons

- Reused val/dev slices inflate the base; all five A20 search changes "lost" for that reason. Use fresh slices and re-run the base (2x games).
- A20/Heart are evaluation-limited (wins 1–4%; a 2-point Heart effect is one or two runs per 100 seeds): pooled-500/1,000 rules, one near-miss extension.
- Winner's curse is visible: `vr2b20` val +6.0 vs dev +1.0. P ≥ 0.9 over dozens of trials gives false keeps.
- Three "P ≈ 0.8" positives did not add up (`heart_combo`).
- Ports are hypotheses: 0 of 4 A0 keeps transferred to Heart; a HIGH-confidence prediction failed.
- Equal compute must be verified by search actions (`heart_hm2` was +3.8%); a global sims change causes lucky trajectory changes (+11 wins came with more act-4 runs).
- Offline fit is not a decision metric. Gate against predict-mean on the matching distribution; per-state vs per-run weighting can fake a failure.
- Score-definition trap: under Heart win mode a key-less act-3 clear and a Heart death score the same.
- "A validation metric that jumps 3× on a routine change is a bug"; run ids unique per dataset; asserts at every consumer; tools take the win definition explicitly.
- Ops: no `pgrep -f` chains; never edit a running script; 2-hour limit needs resumable chunks; `| tail` masks exit codes (OOM); smoke sets hide O(n²) loaders; numbers pasted, never typed; `repro_check` after every rebuild.
- Tracks drift: diff params against A0 at every wake-up.

## 6. Hints of a big untested lever [all inferred]

1. [inferred] Combat objective = run value. The search still maximises a hand-set terminal (win bonus 35, 1 per HP, potion constants); stage 3 never started although V later kept on all four tracks (lines 2025–2026, 2114–2116). Risk: V lost wherever it replaced HP rules.
2. [inferred] Decision-difference (counterfactual) learning instead of level regression. V corr tops out near 0.40 at A0, best-fit models lose in play, deck-model pair_corr is 0.155 (lines 2290–2293, 2385–2389, 2786–2788).
3. [inferred] Boss fights as their own search problem. 150 of 288 A20 deaths, rollouts "cannot win", three value-shape fixes failed, and the rollout policy is never changed in this range (lines 2599–2604, 2689–2694).
4. [inferred] Order-of-magnitude cheaper search. 5 / 50 / 500 sims = 33 / 62 / 83 wins, prior +17 pts at 50 sims, 4x compute +4.5 pts, engine cost "architectural" (lines 2272–2278, 2899–2921).
5. [inferred] Deck strength by act 2 at A20 as the target of the strategic layer, rather than elite gating or boss-relic choice (lines 2681–2685).
