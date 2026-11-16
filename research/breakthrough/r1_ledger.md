# R1 — Ledger digest (results.tsv, results.d/*.tsv, REPORT.md)

Sources read completely: `research/results.tsv` (699 data rows), `research/results.d/a10ports.tsv` (11 rows), `research/results.d/s2scale.tsv` (2 rows), `research/REPORT.md`.

Conventions: score = floor/51 + win. `dscore` = paired score difference against the track best at that time; P = paired-bootstrap probability of improvement. Numbers are quoted as written; [computed] = my arithmetic, [inferred] = my reading.

Limits of the material:
- The ledger has no date column, so promotions are identified by commit. REPORT.md is dated 2026-11-01/02 and stops at Phase S; everything from the run-value model onward (ledger rows 429-700) exists only in the ledger.
- Paired comparisons are valid only within an era. Simulator eras: original (rows 2-72), fixed `rb3` (73-524), "fidelity fork" (525-550), `fidelity-1b05bdc` (551-686), `cb0390e` (687-692), `1b3b779` (693-700). Decision rules: val 300 with P >= 0.9, then pooled val+dev, then fresh slices of 500-4,000 seeds from row 520.

## 1. Timeline of bests per track

Format: name `commit` — mechanism: win rate after, dscore.

**A0** (val 300 until x1add)
- fair0 `302ea3e` — leak-free MCTS + legacy policy: 0.0% dev, 0.4% test.
- pol1 `302ea3e` — hand-written out-of-combat policy: 5.0% dev, +0.1745.
- c_etw02 `e9630ee`, c_etw005 `c17f587` — rollout END_TURN weight 0.2, then 0.05: 10.0%, 14.0%; +0.1866, +0.0682.
- rebase1 `f68ad01` — honest binary (Runic Dome never taken): 11.0%, -0.0338.
- ev2, boss_kb, aoe2 `f68ad01`; scal15 `5ba5edc` — event rules, KB boss relic, AoE and scaling need bonuses: 23.0%; +0.0479, +0.0525, +0.0455, +0.0377.
- wev15 `71c9c1f` — map event value 1.5: 30.0%, +0.0937 (test 27.6%).
- combo2 `649c86f` — six near-miss parameters together: 32.0%, +0.0510.
- hallpot0 `13d3c84` — no potions in hallway-fight search: 35.7%, +0.0492 (test 33.8%).
- rb3 `59cbbae` — simulator fix batch, re-baseline: 38.0%.
- chest1 `3c39f0c` — open treasure chests (inverted action index): 57.7%, +0.2703, P 1.000 (test 54.6%).
- priormlp_d0 `0fb2c40` — MLP root move prior: 61.3%, +0.0437.
- wea1p1 `6c81998`, potsave2 `e66a8cf` — act-1 elite weight, potions saved for bosses: 63.7%, 66.3%; +0.0339, +0.0346.
- bfbonus10 `ed9dd2e`, bfbonus10_g1 `2f1e61a` — KB value + 10x deck-value model: 69.3%, 73.3%; +0.0539, +0.0507 (test 66.6%).
- mapd085 `504030c` — map discount 0.85: 75.0%, +0.0181.
- vr2b20 `7538b72`, vr3b40 `fd8c53a`, vr5relic20 `f5b62ff` — run-value V on card picks, then boss relics: 81.0%, 82.3%, 84.7%; +0.0649, +0.0230, +0.0286.
- x1add `b89d04b` — exploration-trained V (vr_151), fresh 500: 85.8%, +0.0294.
- Switch to the fidelity fork `bcf029c`: 79.6% (n 1000).
- f2a0w `81028d3` — V retrained on fork data at 1.5x weights: 83.4%, +0.0537.
- f6a0_shop `d95ee5c`, f6a0_upg `d6d5698` — V on shop cards and upgrades: 85.4%; +0.0254, +0.0268; joint re-measure +0.0229 (n 2000).
- f45a0_sf `3353388` — soft early stop: +0.0144, confirmation +0.0155.
- Level: cb0_a0 `4344024` 85.7% (n 2000).

**A10**
- base_a10 `af273d7`: 16.3%.
- a10_rest06 `d26a9b6`, a10_kbsyn `83dfe3a`, a10_eh075 `e6aa6ab` — rest threshold, KB synergy bonuses, elite HP gate: 18.7%, 24.3%, 27.0%; +0.0399, +0.0915, +0.0650.
- a10_bfbonus25_g1m `bca48c8`, a10_vr2b20 `dfb53f4`, a10_relic20 `724966b`, a10_r81 `215bad6` — deck value, run value, V on relics, A10-trained V: 29.3%, 34.3%, 38.7%, 44.0%; +0.0620, +0.0707, +0.0681, +0.0584.
- Fork switch `a7ea6d0`: 36.1%.
- f2a10w `d9ae002`, f3a10p `112e257` (V alone, no KB): 41.8%, 44.0%; +0.0733, +0.0702.
- f5a10 `8b7d057`, f6a10_norest `207653d` — V on shop/upgrade/removal: 47.8%; +0.0522, +0.0778; joint re-measure +0.0472 (45.7%, n 2000).
- f23a10 `b119d7a` — V weights x1.5: +0.0291; confirmation -0.0023.
- f35a10 `d56c278` — learned path model pm_3: 50.5%, +0.0614 (confirmation +0.0262).
- f40a10_es550 `c4e31ee`, f43a10_sf `1104cdb` — early stop: 54.9%; +0.0169, +0.0505 (confirmations +0.0390, +0.0323).
- Level: cb0_a10 52.5%.

**A20**
- base_a20 `6c81998`: 2.3%, floor 21.78.
- a20_eh08 `50c35df`, a20_noel1 `099e29f`, a20_restlo `c128857` — elite avoidance, rest value: 1.3%, floor 25.61; +0.0371, +0.0192, +0.0091.
- a20_bfbonus10_g2m `a9d28f6`, a20_potb2 `7630235`, a20_bfb25 `d3c5c0b`, a20_restboss1 `6a9831f`: 4.0%, floor 28.36; +0.0167, +0.0131, +0.0356, +0.0144.
- a20_vr2b20 `76b12cc`, a20_relic20 `c11cd34`: 6.7%; +0.0220, +0.0327. a20_r121s32f `c4cc1c7` (A20-trained V, fresh 1000): 6.2%, +0.0232.
- Fork switch `ae348cc`: 3.0%, floor 27.09.
- f2a20w `8739cfe`, f3a20w `e01753f`, f5a20 `df498d6`, f7a20 `49ad22b`: 4.8%; +0.0145, +0.0210, +0.0160, +0.0163; f7a20 re-measured against f3a20w +0.0139.
- f23a20 `c8c50ba`, f24a20 `c840001` — V weights x1.5 twice: +0.0103, +0.0188; confirmations -0.0024, +0.0032. f29a20 `accf8a7`: +0.0050.
- f35a20 `238e414` — path model pm_1: 6.2%, +0.0320 (confirmation +0.0250).
- f43a20_sf2 `bffead2` — early stop: 7.7%, +0.0228 (confirmation +0.0206).
- Level: cb0_a20 8.0%, floor 31.22.

**Heart** (A0 with keys; win = Heart beaten)
- INFO_keys1 `64e149a`: 6.3%.
- heart_cur `f9c460e` (A0 chain ported), heart_bfbonus25_g_heart `58d78fd`: 7.3%, 9.3%; +0.0473, +0.0522.
- heart_a4pot `a0e5125`, heart_wba3 `b689e1f`, heart_ka3 `d3d4240` — act-4 potions, key rules: 11.3%; +0.0133, +0.0031, +0.0033.
- heart_hm2 `502183a` (+0.0524) was revoked: it used +3.8% search actions.
- heart_bfrelic10 `95458f7`, heart_vr2b20 `9675a6f`, heart_relic20 `550d25a`: 13.3%, 15.3%, 16.0%; +0.0246, +0.0499, +0.0203.
- Fork switch `c3a1592`: 11.8%.
- f1heart `40e3fa3`, f2heartp `ecea57a` (V alone), f6heart_shop `b3ac7bc`: 17.0%; +0.0354, +0.0323, +0.0189. f6heart_upg `a34c5af` +0.0158, re-measure -0.0090.
- f15heart `f7191d1` — Heart V with features v2: 25.1%, +0.0458; re-measure +0.0058 (23.8% win, floor 45.87).
- f16heart `a1794de` — expert-iteration keys-aware V: 29.0%, +0.0731; re-measure +0.0211 (25.3%).
- f45heart_sf `f1abe63` — early stop: 29.8%, +0.0410 (confirmation +0.0297).
- Level: cb0_heart 28.3%; at 2x search on test 500: 35.0% (`ffffee5`).
- A20 Heart (`4f334d0`, n 2000, measurement only): 0.3%.

## 2. What actually moved the needle

Largest single keeps: chest1 +0.2703; c_etw02 +0.1866; pol1 +0.1745; wev15 +0.0937; a10_kbsyn +0.0915; f6a10_norest +0.0778; f2a10w +0.0733; f16heart +0.0731; a10_vr2b20 +0.0707; f3a10p +0.0702.

Sum of first-measurement dscore over all promoted rows, four tracks [computed; heart_cur and the revoked heart_hm2 excluded]:

| Class | Rows | Sum | Share |
|---|---|---|---|
| Learned model | 43 | 1.6557 | 50.2% |
| Parameter tweak | 14 | 0.6839 | 20.7% |
| Policy hole closed (new hand rule) | 10 | 0.5185 | 15.7% |
| Bug / fidelity fix | 2 | 0.2947 | 8.9% |
| Search efficiency (early stop) | 5 | 0.1456 | 4.4% |
| Total | 74 | 3.2984 | 100% |

- Learned breakdown: run value vr_* 1.2229 (33 rows, 37.1%); deck value bf_* 0.2957 (7 rows, 9.0%); path model 0.0934 (2 rows); move prior 0.0437 (1 row).
- Parameter tweaks include the rollout END_TURN weight (0.2548); without it they are 0.4291 (13.0%).
- Per track: A0 is balanced (bug 18%, policy 25%, parameter 30%, learned 25%, search 1%); A10 is 70% learned, A20 70%, Heart 86%.
- A0 in win points on val, 0 to 84.7 [computed]: parameter 26.7, bug/fidelity 22.0, learned 20.3, policy 18.7, honest re-baseline -3.0.

The keep sum overstates what was banked. Net score movement is 1.4116 (A0) + 0.5822 (A10) + 0.2401 (A20) + 0.2925 (Heart) = 2.5264; keeps including heart_cur sum to 3.3457. Of the 0.8193 difference, 0.3349 is negative re-baselines (rebase1 -0.0338; fork switch -0.0738, -0.1051, -0.0551, -0.0671, measured across slices) and about 0.48 is selection bias [computed]. Examples: A10 F5+F6 keeps 0.1300 against a 0.0472 re-measure; A0 shop+upgrade hooks 0.0522 against 0.0229.

## 3. Discards by area

372 discard rows: median dscore -0.0057; 63% within +/-0.02; 46 rows at -0.05 or worse; 34 positive with P >= 0.8 [computed]. Area assignment by row name is mine; first-slice rows of later keeps are included.

| Area | Rows | Median | Range |
|---|---|---|---|
| Run value vr_* (weights, retrains, extra hooks, pure V, events) | 101 | -0.0046 | -0.3263 to +0.0374 |
| Map weights, elite gates, simulated elite gate | 53 | -0.0050 | -0.1689 to +0.0307 |
| Deck value bf_* (replace, bonus weights, retrains, shop, relic) | 41 | -0.0084 | -0.2075 to +0.0410 |
| Card-pick heuristics (threshold, need bonuses, KB synergy, boss prep) | 33 | -0.0070 | -0.0685 to +0.0295 |
| Compute reallocation, throughput, early-stop variants | 26 | -0.0029 | -0.0624 to +0.0375 |
| Potion policy | 23 | -0.0006 | -0.0907 to +0.0346 |
| Search and terminal-value parameters | 22 | -0.0036 | -0.3062 to +0.0176 |
| Move prior variants | 18 | +0.0049 | -0.1452 to +0.0310 |
| Heart key rules, act-4 prep | 14 | +0.0032 | -0.0356 to +0.0293 |
| Rest / upgrade / removal rules | 13 | -0.0056 | -0.0483 to +0.0144 |
| Rollout cutoff + heuristic leaf, action budget | 9 | -0.1402 | -0.3727 to -0.0412 |
| Rollout policy bias | 6 | -0.0649 | -0.4548 to +0.0180 |
| Neow | 5 | -0.0091 | -0.0234 to +0.0098 |
| Lookahead picks replacing rules (rvpick, cfpick) | 5 | -0.3333 | -0.6212 to -0.2240 |
| Path-model ports to A0 / Heart | 2 | — | -0.0017, -0.0075 |
| Mixed learned leaf (f52_mix05) | 1 | — | -0.0215 |

- Saturated (noise-level, many tries): map weights, card heuristics, potions, rest rules, search constants, move-prior variants, V weights and retrains.
- Reliably harmful: truncated rollouts with a heuristic leaf, hand-coded rollout biases, and any model that replaces the rules outright (bfpick -0.0661 to -0.2075; rvpick/cfpick -0.22 to -0.62).
- Learned leaf value (23 `scaling` rows plus 2 in s2scale.tsv, all A0): always below rollouts, see section 4.
- Ports between tracks mostly fail: a10_potsave2 -0.0907, a10_mapd085 -0.0390 (P 0.011), a20_potv8 -0.0341, heart_wea1 -0.0248.

## 4. Reference and diagnostic rows

- Cheating references use lightspeed's own search only: scum_cheat 2.0% dev, 0.8% test (fair0 0.4%). DIAG_cheatcombat (best policy + cheating lightspeed combat search): 9.6%, -0.2047, 16 crashes. No row gives our MCTS perfect information, so the value of hidden information is unbounded by the ledger [inferred].
- Search scaling, never adopted. F39 (n 1000): A20 2x 8.5% (+0.0514), 4x 9.3% (+0.0762); A10 2x 52.8% (+0.0762), 4x 57.5% (+0.1393); Heart 2x 30.7% (+0.0626), 4x 32.5% (+0.0914). After early stop (F44/F48, n 2000): A0 89.0% (+0.0520), A10 56.6% (+0.0856), A20 9.1% (+0.0298), Heart 32.7% (+0.0688). Phase 1: INFO_sims1000 36.3% vs 30.0% (+0.0805).
- Less search: f52_ctl675 (675 instead of 1000 sims) -0.0183; a20_s400 -0.0522 on val.
- Learned leaf replacing rollouts at equal wall-clock (A0): heuristic-leaf control 11.7%; sv7 36.0%; sv11 42.0%; sv16 48.7%, all against 75.0%. F52: sv54 61.6%, sv55 61.8% (n 2000), -0.2904 and -0.2975 against the roughly 85% best. Mixed leaf at 675 sims 82.6% against the 675-sim control 82.7%.
- Fork switch as a fidelity reference: A0 85.8% to 79.6%, A10 44.0% to 36.1%, A20 6.2% to 3.0%, Heart 16.0% to 11.8%.
- REPORT loss locations: A0 (wea1p1) bosses are 57% of losses; base A20 53% of runs die in Act 1; about 90% of Act-4 deaths are at the Heart.
- No row isolates strategy headroom (for example a fixed strong deck).

## 5. Oddities

- The A20 val batch a20_bm15 / bm2 / bm2x / bm05 / s400 is negative in every direction, including bm2x with +47% compute (pooled -0.0378, P 0.005), which contradicts F39/F42. In the same era a20_r121s32 read -0.0140 pooled on val and +0.0232 (P 0.972) on a fresh slice. Reading: the A20 val/dev seeds were exhausted by selection [inferred]; A20 val-era discards are unreliable.
- Heart wins rose from about 16.5% to 23.8% with f15heart while the score moved +0.0058, because floor fell about 3. The score metric barely registers the north-star quantity.
- "Pure V" (no KB) is a keep at A10 (+0.0702) and Heart (+0.0323), negative at A0 (-0.0080, -0.0277, -0.0815), a failed near miss at A20.
- Expert iteration of V worked once (f16heart) and failed everywhere else: F4 (vr_191), F17 (vr_232), F20 (vr_242, -0.0186). f11heart (Heart-track V) collapsed to floor 34.14 (-0.3263).
- The path model is a keep at A10 and A20 and flat at A0 (-0.0017) and Heart (-0.0075).
- V on the rest decision is harmful at A10 (removing it +0.0778), helpful at A20 in "unbiased" form (+0.0163), negative at A0 and Heart.
- Early stop is not monotone: es_frac 0.1 kept on all tracks; 0.05 with more sims -0.0215 (P 0.003) at A20; 0.15 and 0.2 negative at A10.
- Small slices mislead: sv54 69.3% on 300 seeds, 61.6% on 2,000; c_c15 +0.0176 then -0.0559 on retest; val 73.3% against test 66.6%.
- REPORT cites a "4x-compute probe (+4.5 pts)" at A0; no such row exists in the ledger before F39.
- Measured once, not followed up: a10_vr4lb40 (+0.0374, P 0.856), heart_kud_krw (pooled-600 +0.0123, P 0.806), spd_s650 (+0.0375 with 3 crashes).

## 6. Hints of a big untested lever (all [inferred])

1. **Effective search multiplier.** F39 4x rows: A10 +0.1393, Heart +0.0914, still rising from 2x. The one rollout-policy parameter ever kept (c_etw02, +0.1866) is the second largest gain in the ledger; a learned rollout policy was tried once, at cost ratio 2.430 (rollprior_cal, -0.0624). A cheap, better simulation policy is untested. At A20 4x yields only 9.3%, so this alone does not reach the north star.
2. **Systematic interface and fidelity audit.** chest1 (+0.2703) and the fork switch (-0.055 to -0.105) are the largest moves in the ledger and neither is algorithmic. hallpot0 gained +0.0492 by removing potions from hallway search, which suggests the search misuses a whole action class.
3. **Objective-matched value for A20 Heart.** f15heart and f16heart moved Heart wins about 16.5% to 25.3% by training V on the track's own outcome. A20 Heart has only measurement rows with borrowed settings (f47a20h_a/b, 0.3%); nothing was ever trained or selected on that objective.
4. **Training data with winning examples at A20.** A20 models learn from the agent's own games (bg_a20: 1.6% wins in 500 games). The only data-distribution change tested, exploration-trained V (x1add, +0.0294 at A0), was a keep; A20-specific retrains were mixed (a20_r121 -0.0366 on val, +0.0232 on a fresh slice; f20a20 -0.0186).
5. **Perfect-information bound for our own search.** DIAG_cheatcombat used lightspeed's weaker search (-0.2047), so the ledger cannot say whether belief modelling of hidden state is worth 1 point or 20. This is a cheap diagnostic that decides whether a whole direction exists.
