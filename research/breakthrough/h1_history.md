# h1 — digest of `research/program.md` lines 106–230

Scope: 2026-11-09 16:16 UTC (F40 pre-registration) to 2026-11-16 06:50 UTC (owner closes all lines): phases F40–F52, S3, simulator fixes. Numbers are quoted from the source. `dscore` = paired score difference vs the track best; "ratio" = mean search actions new/base; "F25" = confirmation on 2,000 more fresh seeds. F39, F35, F36–F38, S2, F22, P1 are only referenced here (their entries lie outside this range). [inferred] = my reading.

Bests at the end of the range: A0 `f45a0_sf` (early_stop=1 es_frac=0.1 sims=1000), A10 `f43a10_sf` (sims 950), A20 `f43a20_sf2` (sims 1050), Heart `f45heart_sf` (sims 775); simulator fidelity-1b3b779.

## 1. Experiments and idea lines

| ID / name | Area | What was changed | Result | Verdict | Track |
|---|---|---|---|---|---|
| F40 safe early stop + reinvest | compute allocation | stop when the leader's visit lead > sims remaining (move provably unchanged); spend the saving on sims 500→550/525 | saving 8–15% of search actions, games bit-identical; A10 +0.0169 (n=4000, P 0.952), F25 +0.0390, pooled 6,000 ≈ +0.0243; A20 pooled 8,000 −0.0002; Heart −0.0055 (P 0.245) | keep A10; discard A20, Heart | A10/A20/Heart |
| F41a tree reuse in a fight | combat search | `tree_reuse=1` keeps the subtree under the real observation | −0.0015, P 0.445, n=4000, ratio 1.0223 | discard | A10 |
| F41b early stop + sims 550 | compute allocation | F40 on A0 | −0.0000, P 0.491, n=4000 | discard | A0 |
| F42a, F44b, F48: 2× search | compute allocation | sims doubled (owner test, never promoted) | older bests: A0 +0.0364, A10 +0.0537, A20 +0.0484, Heart +0.0449; soft-stop bests: A0 +0.0520 (0.848→0.890), A10 +0.0856 (0.504→0.566), A20 +0.0298 (0.076→0.091), Heart +0.0688 (0.279→0.327); all P 1.000 | info; owner: 2× for milestone/final scores only | all |
| F42b A10 replay audit | statistics/method | FAIR_TRACE traces, 180 games | no bug; see §3 | info | A10 |
| Direction A: 4×-search teacher distilled into the move prior | move prior | expert iteration on boss fights | NOT started; withdrawn because distillation had failed ×3 (`priormlp2_d0` −0.034 P 0.15; `priormlp3_d0` +0.014 P 0.65; P1 prior v4 +0.6 pts n.s.) | closed without a run | — |
| F43/F45 soft early stop `es_frac=0.1` | compute allocation | stop when visit lead > 0.1 × sims left; raise the sims cap to equal mean actions | first slice / F25: A10 +0.0505 / +0.0323; A20 pooled +0.0166 / +0.0206; Heart +0.0410 / +0.0297; A0 +0.0144 / +0.0155 | keep on all four | all |
| F44a, F46: other `es_frac` | compute allocation | f = 0.05, 0.15, 0.2 | A10: 0.05 −0.0016, 0.15 −0.0024, 0.2 −0.0237 (P 0.047); A20: 0.05 −0.0215 (P 0.003) | discard; closed at 0.1 | A10/A20 |
| F47 A20 Heart baseline | statistics/method | (a) A20 best + Heart key settings; (b) Heart best at A20 | a: win 0.003, floor 27.08, act4 0.025; b: win 0.003, floor 22.97, act4 0.017; 6/2000 wins each; b vs a −0.0796 | info; no A20-Heart track | A20 Heart |
| F49 Heart milestone | statistics/method | Heart best at 2× (sims 1550), 500-seed test split | 175/500 = 35.0%, Wilson 95% [30.9%, 39.3%]; act-4 reach 65.8% | info (≥ 20% confirmed) | Heart |
| F50 audit, F51 elite veto | path/map | 240 traced A20 games; `elite_veto_hp=0.5` forbids an elite below 50% HP when a non-elite is legal | audit: "no fixable misplay that moves the score"; veto +0.0019 (P 0.766), extension −0.0013, pooled 4,000 +0.0003 (P 0.571) | discard; closed | A20 |
| Crash fix cb0390e + S3 re-baseline | simulator fidelity | action-queue `back` pointer after victory; ViolenceAction 64-slot overflow; null-searcher deref | 0 crashes; A20 0.080, A10 0.525, A0 0.857, Heart 0.283 (act4 0.573), each within ±1 pt of its recent slices | keep | all |
| Relic-order fixes 469ee23, 1b3b779 | simulator fidelity | card-use and start-of-turn relics act in the order gained | 1 of 800 games changed; then 0 | keep | all |
| Real-game regression | simulator fidelity | 1,765 recorded real-game runs replayed | 1466 pass / 250 limited / 49 fail; all 49 explained, none an open simulator bug | info | — |
| `STS_NATIVE` build | data/infra | `-march=native -ffp-contract=off` | ≈ 7–8% faster, bit-identical; profile has no single hotspot | keep (opt-in) | all |
| F52 mixed leaf (AlphaGo-style, λ=0.5) | rollout/leaf evaluation | new-leaf value = 0.5·sv_16 + 0.5·one rollout; 1.455× cost → sims 675 | −0.0215 (n=2000); control (plain best at 675 sims) −0.0183 → the mix adds nothing per simulation | closed | A0 |
| F52 pure learned leaf, 7×/10× data | rollout/leaf evaluation | MLP on 206 features (h=42: 10,459 params; h=96: 29,089) predicts the mean of 8 rollouts and replaces rollouts; sims 1675–1800 (h=96: 850) | sv_16 (3.2M rows) −0.3653 (n=300); sv_54 (22.5M) −0.1922 (n=300), −0.2904 (n=2000); sv_55 (32.4M) −0.2975, dwin −0.238 (n=2000); sv_56 (h=96) −0.3016 (n=300); bar −0.2 | stop rule not met; closed for good | A0 |

Earlier failures quoted in this range: boss-fight search reallocation (A0 `realloc` −0.042; A20 `a20_bm15`/`a20_bm2` −0.026/−0.05; F22 "+20% search bought +0.006"); path-model iterations F36–F38 "never armed" (A10 held-out headroom −0.0048 ± 0.0074); early `reuse1` −0.0088 P 0.372.

## 2. Sensitivities

- **Search budget pays on every track.** 2× (table above): +0.030 to +0.086 score, +1.5 to +6.2 pts of win rate. Quoted from F39: A10 ×2 +0.076, ×4 +0.139; A20 ×2 +0.051. Heart at 2× on the test split: 35.0% vs 28–30% at 1×.
- **Less search:** A0 best at 675 sims instead of 1000: −0.0183 (dwin −0.015).
- **Safe early stop** saves 8–15% of search actions, which buys ≈ +10% sims at A10 and +5% at Heart ("the ratio climbs faster than sims, since stronger play makes longer runs").
- **Soft stop, neutral sims cap vs f** (A10, base 550): f 0.5 ≈ 575, 0.25 ≈ 710, 0.2 ≈ 725, 0.15 ≈ 814, 0.1 ≈ 970, 0.05 ≈ 1,360; A20 (base 500): 0.1 ≈ 1,125, 0.05 ≈ 1,520. Optimum f = 0.1 on A10 and A20.
- On A10 the soft stop "recovers about 94%" of the 2× gain at 0.96× compute (one slice).
- **Stacking:** 2× on top of the soft stop bought more on A10 (+0.0856 vs +0.0537), less on A20 (+0.0298 vs +0.0484). One slice each.
- **Under-spent compute hides gains:** Heart at ratio 0.8614 → +0.0000, at 0.9609 → +0.0410; A0 at 0.9359 → +0.0050, at 0.9739 → +0.0144.
- **Leaf-value data scaling:** 3.2M rows −0.365; 22.5M −0.290; 32.4M −0.298. Holdout RMSE 11.93 → 11.83 (h=96: 11.524; predict-mean 33.311). Projection was +0.17 for 10×; 7× gave ≈ +0.07 and 10× no more.
- **Leaf-value cost:** one sv evaluation ≈ half a full rollout; the pure leaf affords ≈ 1.75× the sims; h=96 costs 1.151× → sims 850.
- **Data cost:** 97,500 games → 32,404,409 rows (≈ 336 rows/game, ≈ 0.36–0.37 s/game); 10 worker sessions ≈ 2 h, ≈ $30.
- **Simulator fixes** left every best within ±1 pt. Crashes rose with search: 3/2,000 at 2× vs 0/2,000 at 1×.
- **A20 key policy** costs ≈ 4 floors (31.4 without keys vs 27.1 with).
- **Speed:** compile flags ≈ 7–8%; deeper hot-loop work "perhaps 10–20% more" (LOW-MED in source).
- **Elite veto** touches ≈ 26 elite picks per 240 games (≈ 8 died); net +0.0003.

## 3. Diagnoses

- **A10 losses are act-2 heavy** (strong; 1,982 losses of 4,000): act 1 351, act 2 1,005, act 3 626. Death fights: Collector 196, Automaton 194, Awakened One 180, Champ 147, Donu and Deca 109, Slime Boss 91, Hexaghost 78, Time Eater 77.
- **A20 losses concentrate in bosses** (strong; 4,000 games): Automaton 304, Collector 276, Awakened One 259, Champ 235, Hexaghost 214, Slavers 191, Slime Boss 178, Donu and Deca 172. Slavers are entered at 46% HP on average, Book of Stabbing at 38% (240 traced games).
- **Deck-strength gap, not single misplays** (weak to medium: small samples, correlational, found in both audits). A10 at the act-2 boss, runs that die vs win: HP entering 74% vs 90%, upgrades 6.7 vs 7.5, relics 7.9 vs 9.1, gold 124 vs 163, shop gold spent 321 vs 640, removals 2.1 vs 3.3. A20: 73% vs 88% HP. F50: "a deck-strength gap rather than a single bad decision".
- **Combat leads seen in traces, never tested** (anecdotal): racing instead of blocking at Automaton (1366518 turn 4: 34 incoming, two Shrug It Off kept, 52 → 17 HP; orbs never targeted); 4 potions on turn 1 vs Collector (1366510); 466 gold unspent at the act-3 boss, path had no act-3 shop (1366521).
- **Checked, not bugs:** 0-cost blocks unplayed (Corruption); rest at full HP (Fusion Hammer); low-HP map choices.
- **The learned path model can take an elite at any HP** (below 40% HP: 8 of 65, 4 died), yet forbidding it is neutral.
- **A20 Heart:** 0.3% wins, act-4 reach 2.5% / 1.7%; top killers Slime Boss 155, Hexaghost 152, Automaton 141, Collector 140, The Guardian 127. "The north star … is ≈ 50× away"; "A20 act-3 strength is the binding constraint".
- **Heart (A0):** The Heart is the main killer (1108 deaths in a 4,000-game run; 145 of 325 losses at 2×); act-4 reach 0.554–0.602 at 1×.
- **The loop's synthesis for the owner (2026-11-09):** "the big gains came from fixing broken behaviour; out-of-combat decisions are near saturated; combat search depth/judgment is the bottleneck (F39)".
- **Leaf value:** the MLP "saturates near RMSE 11.8 (rollout-mean target sd 33), and that residual costs more than the ≈ 1.75× extra simulations buy" [inferred in source]. A lower-variance leaf (the mix) adds nothing per simulation.
- **Search facts:** fresh searcher per decision; move = most root visits (`select_q = 0`); "most decisions are not decided early at 500 sims"; reuse subtrees "mostly reset (new draws/rolls change the observation)".

## 4. Mentioned but never tested

1. **A different state representation for the learned leaf value** — "a different representation (not more rows) would be a new line, not a continuation". Only the 206-feature MLP was tried; h=96 only on 300 seeds, "not extended".
2. **Matching training states to the states the search visits** — "may matter as much as the row count [inferred; the two are confounded here]"; never isolated.
3. **Mixed leaf with the 10×-data model** (retest allowed only if the pure leaf passed); other λ than 0.5; a **cheaper sv evaluation** exploiting "features are 80% zeros".
4. **Pooling the F52 data with the older S2 states** (deliberately not done).
5. **Direction A, expert iteration on boss fights.** Withdrawn before any run; only general distillation had been tried.
6. **F50 step 2 boss experiments** — potion timing, block vs race, target choice. Only the elite veto was run.
7. **A10 audit leads** — Automaton race-vs-block, orb targeting, early mass potion use; "waiting for the owner's read of the games"; no experiment followed.
8. **Late-shop / unspent-gold pathing** — noted, no experiment in this range.
9. **Elite veto on A10 and a 0.6 threshold** — conditional on a keep.
10. **A fourth path-model iteration** ("10 workers × ≈ 8 h") — not run without the owner.
11. **An A20-Heart track**, and any tuning of the hand-set key policy at A20 (`keys=1 key_act3=1 w_burning_act3=3 act4_shop_potion=1`).
12. **Tree reuse on A20/Heart**, and a fix of the frozen `knownTop` under reuse.
13. **`es_frac` 0.25 and 0.5 in play** (calibrated only).
14. **Search above 2× on the current bests** — no such run appears in this range [inferred].
15. **Deeper simulator speed work** across the hot loop — deprioritised.
16. **Deferred simulator gaps:** The Bomb among end-of-turn powers; Strength/Dexterity outside the status map; Rampage under Double Tap + Duplication; Empty Cage auto-removal; full-belt potion reward.
17. **Draft PRs #20/#21** stay open; their content is not in my material.

## 5. Method lessons

- 300 seeds are too noisy to decide: one model read −0.1922 (n=300) and −0.2904 (n=2000). Decide on ≥ 2,000 fresh seeds.
- Keep rule: dscore > 0, P ≥ 0.9, ratio ≤ 1.01; P 0.7–0.9 → one extension; a keep counts only after the F25 confirmation, which matters most for second tries.
- Compute calibration on 200 used seeds is off by up to ±14%; fresh-slice ratios are the reliable points. Under-spending caused two false discards (F43 Heart/A0); over-spending voided one result (ratio 1.0143).
- A cost-matched control (the plain agent at the arm's sims) separates per-simulation value from cost.
- Offline loss is not play strength: holdout RMSE across datasets did not predict it, and row-level holdouts leak (sv_11's 11.825 was partly in-sample; game-level ≈ 12.5).
- A log-linear projection from one slope point failed.
- Re-measure the baseline on the current agent and simulator before a large spend; re-read the history before recommending (Direction A).
- Two loss audits found no single fixable misplay.
- Simulator changes need bit-identical gates, the real-game regression and an ASan+UBSan sweep. `-march=native` alone changes play (12 of 16 games differ). More search exposes more simulator bugs.
- Operations: explicit task timeouts (default background limit 30 min), frequent container restarts, row counts verified at pull, no multi-hour worker phase without the owner, and no experiment before the owner sees the report.

## 6. Hints of a big untested lever

1. **Effective simulations per unit wall-clock** [inferred]. Every 2× still buys +0.03 to +0.086 (F42/F48) and F39's ×4 gave A10 +0.139, while the only keeps of F40–F52 re-allocate the same budget. Only ≈ 7–8% of raw speed was taken, and with "no single hotspot" a large speed-up needs a structural change.
2. **Leaf-evaluation bias, not variance** [inferred]. The mixed leaf cut rollout variance and gained nothing per simulation (−0.0215 vs control −0.0183). The learned leaf was trained to copy the mean of 8 rollouts, so its ceiling was the rollout policy's own judgment. A target from real fight outcomes or deeper search, or a stronger rollout policy, does not appear in this range.
3. **The form of the strategic layer, not its tuning** [inferred]. Both audits attribute losses to arriving at bosses weaker (HP 74% vs 90%, removals 2.1 vs 3.3, shop spend 321 vs 640), while the synthesis calls out-of-combat decisions "near saturated" and three path-model iterations never armed. Both hold only if the heuristics-plus-small-models form is what saturated; run-level planning is not mentioned in this range. Caveat: entering HP also reflects combat quality.
4. **Search across chance events** [inferred]. Reuse subtrees "mostly reset" on each draw or enemy roll, and the safe stop saves only 8–15%, which suggests a shallow tree whose multi-turn judgment rests on rollouts; the Automaton race-vs-block trace fits. Multi-turn planning in boss fights was not tried here.
5. **A20-specific key and risk policy** [inferred]. Heart settings copied to A20 cost ≈ 4 floors and leave act-4 reach at 2.5%; the key policy was never tuned at A20. This is a precondition for the north star, not a step change by itself, since A20 without keys is at 7–8%.
