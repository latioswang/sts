# h3 history digest — research/program.md lines 388–533

Covers 2026-11-07 09:01 UTC to 2026-11-08 15:17 UTC: phases F13–F32 complete; F33 pre-registered and started (its result is NOT in this range). Numbers are quoted from the source; `[inferred]` marks my own reading. dscore = paired difference in score (floor/51 + win); P = P(improve).

Scope: every phase here concerns the run value V, its weights, search-budget allocation or measurement method. Nothing in this range touched the rollout policy, the leaf value, the move prior, bf_* or pm_* [inferred from absence].

## 1. Experiments and idea lines

| ID | Area | What was changed | Result | Verdict | Track |
|---|---|---|---|---|---|
| F13 | statistics/method | Re-measure of the F5–F7 keep chain: current best vs pre-F5 best, 2,000 fresh paired seeds | A10 +0.0472 / +3.0 pts (P 0.999); A20 +0.0139 / +0.4 pts (P 0.954); A0 +0.0229 / +1.8 pts (P 0.989); Heart −0.0090 / −0.8 pts (P 0.105) | info | all |
| F14a | statistics/method | Heart revert check, f2heartp vs f6heart_upg | pooled n=3999: dscore +0.0052, P 0.849 | info, no revert | Heart |
| F14b | run value V | Features v2 (666 + 4 key features); 15,000 Heart games + ≈ 240k keyless runs; RMSE selection | vr_210 rmse 0.3701 corr 0.367; vr_202 0.3692 / 0.360; vr_171 0.7862 / 0.271 → RMSE kept vr_202 | closed, no arm | Heart |
| F15 f15heart | run value V | vr_210 with V-only picks | pooled 2,000: dscore +0.0314, dwin +0.072, P 0.990; act-4 reach −13 pts | keep | Heart |
| F15 f15heart_b | run value V | vr_210 as a weighted bonus (weights 20 / 20) | dscore −0.0885, dwin −0.046, P 0.000 | discard | Heart |
| F16a | statistics/method | Confirmation f15heart vs f6heart_upg, 2,000 seeds | dscore +0.0058, P(score) 0.663; dwin +0.060, CI [+0.038, +0.082] | info | Heart |
| F16b f16heart | run value V | Expert iteration 2 (15,000 games by f15heart + explore_eps=0.2), correlation selection → vr_222 | corr 0.443 vs 0.410; dscore +0.0731, dwin +0.062, P 1.000; act-4 reach +7.2 pts | keep | Heart |
| F17a | statistics/method | Confirmation f16heart vs f15heart, 2,000 seeds | dscore +0.0211, dwin +0.015, P 0.945 | info | Heart |
| F17b f17heart | run value V | Expert iteration 3 → vr_232 | corr 0.446 vs 0.431; pooled 2,000: dscore +0.0074, P 0.713 | discard | Heart |
| F18a | shop-rest-event-relic policy | Key cost: keys=0 vs keys=1 | act-3 boss beaten 0.660 vs 0.644 → +1.6 pts | info | Heart |
| F18b f18heart_vkey | shop-rest-event-relic policy | Ruby recall chosen by V instead of HP threshold 0.65 | dscore −0.0121, dwin −0.021, P 0.203 | discard | Heart |
| F19 f19heart_hm | compute allocation | sims 445, Heart fight at 1,500 (compute ratio 0.9986) | dscore −0.0038, P 0.413; Heart kill given act 4 0.516 vs 0.488; act-4 reach −2.2 pts | discard | Heart |
| F20 f20a20 | run value V | A20 expert iteration (30,000 games by f7a20 + explore); A20-only vr_240 by correlation | corr 0.608 vs 0.554; dscore −0.0186, dwin −0.009, P 0.041 | discard | A20 |
| F21 f21a20 | run value V | Same V, weights ×1.5 (108) to match its compressed scale | dscore −0.0189, dwin −0.014, P 0.036 | discard; "A20 V iteration closed" | A20 |
| F22 f22a20_bm | compute allocation | sims 350, bosses at 1,000 | dscore +0.0063, P 0.747; compute ratio 1.1999 (not neutral) | discard | A20 |
| F23 | run value V | V weights ×1.5 (A20 72 → 108; A10 → 45; A0 → 90/45/90) | A20 +0.0103 P 0.911; A10 pooled +0.0199 P 0.942; A0 −0.0058 P 0.392 | 2 keeps, 1 discard | A0/A10/A20 |
| F24a | statistics/method | 2,000-seed confirmations of the F23 keeps | A20 −0.0024 P 0.345; A10 −0.0023 P 0.389: "Both F23 keeps were selection luck" | info | A10/A20 |
| F24b | run value V | Second weight step (A20 → 162; A10 → 68) | f24a20 +0.0188 P 0.985; f24a10 pooled +0.0013 P 0.543 | keep / discard | A20 / A10 |
| F25 | statistics/method | Confirmation rule; f24a20 vs f7a20, 2,000 seeds | dscore +0.0032, dwin +0.001, P 0.671 | keep (≈ +0.003) | A20 |
| F26 | statistics/method | `--branch_eval`: per card-reward option, mean of M=8 common-random-number playouts; V scored by the playout mean of its top option | 1,390 decisions, vs vr_171: random −0.0285 ± 0.0026; own pick −0.0016 ± 0.0015; vr_240 −0.0044 ± 0.0023; oracle +0.0699 (biased up) | info, metric adopted | A20 |
| F27 | run value V | Pick among 91 models / 101 candidates by the decision metric, even/odd split | best: mean(vr_183, vr_162) B +0.0002 ± 0.0020; best single vr_163 B −0.0034 ± 0.0034 | closed, no arm | A20 |
| F28 | run value V | Decision-trained V (within-decision centred MSE on playout means + λ·anchor to vr_171); 19,531 decisions | vr_250 +0.0017 ± 0.0014; vr_251 +0.0019 ± 0.0010 (1.9 SE) | closed, below arm bar | A20 |
| F29 f29a20 | run value V | Play test of vr_251 | pooled 8,000: dscore +0.0042, P 0.911; win 0.049 vs 0.052 | keep | A20 |
| F30 f30a10 | run value V | A20-trained vr_251 at A10 | 4,000 seeds: dscore −0.0154, dwin −0.015, P 0.049 | discard | A10 |
| F31 | run value V | A10 decision-trained V; 18,154 decisions | held-out 1,510: vr_260 +0.0026 ± 0.0023; random −0.0563 ± 0.0045; oracle +0.1359 ± 0.0045 | closed, no arm | A10 |
| F32 | run value V | One decision-trained V on A10 + A20 (37,685 decisions) | A10: vr_270 +0.0028 ± 0.0024; A20 vs vr_251: −0.0006 ± 0.0012 | closed, no arm | A10/A20 |
| F33 | statistics/method | Split-half branch eval (two 8-playout halves) for card rewards and boss relics | pre-registered, data started; result not in range | open in range | A10/A20 |
| vr_model_card | data/infra | Separate V id for card-reward picks | built; "Not used by any test yet" | info | — |
| PR #10 | combat search | Owner's search perf change | bit-identical results; no speed figure in range | merged | all |
| Shared-map bug | simulator fidelity | State copies shared one map; act transition overwrote it | 330 crashed of 785 seeds (42%); fixed by a per-copy map | fixed | — |
| Crash records | simulator fidelity | Rollout crashes in `moveToDiscardPile` (SIGSEGV) and `ViolenceAction` (stack smashing, 4 seeds) | ≈ 0.1% per game; passed to the fork owner | info, unfixed in range | — |

## 2. Sensitivities

Search budget
- A20, +20% search actions (ratio 1.1999): dscore +0.0063 (P 0.747), win 0.048 vs 0.047, floor 29.48 vs 29.20.
- Heart fight at 3× simulations with 11% fewer elsewhere (neutral): Heart kill rate given act 4 +2.8 pts, act-4 reach −2.2 pts, net −0.0038. "the search budget is near-balanced already [inferred, LOW-MED]" (source's tag).
- Share of search actions: Heart fight 5.6% of a Heart run; A20 bosses ≈ 0.424 (stated as inflated). Actions per game: A20 1,473,220; Heart 3,901,072.
- History cited: `heart_hm2b` (2× at the Heart, old era) +0.7 pts, pooled P 0.817; A0 `realloc` (400 sims, bosses ×2.25) lost, P 0.067.

V weights
- A20 72 → 108: ≈ +0.0018 over 3,000 seeds. 72 → 162: +0.0032 (P 0.671), "≈ +0.1 pts wins".
- A10 → 45: ≈ +0.005 over 3,000 seeds; → 68: +0.0013. A0 → 90/45/90: −0.0058.

V trained on run outcomes
- Heart: 18× the runs plus key features moved holdout correlation only 0.354 → 0.367. Played with V-only picks: +6.0 pts Heart wins, −13 pts act-4 reach, score +0.0058.
- Heart iteration 2: corr 0.410 → 0.443, honest gain +1.5 pts wins / +0.02 score. Iteration 3: 0.431 → 0.446, +0.0074 (P 0.713).
- A20: corr 0.554 → 0.608 gave −0.019 in play, twice.
- h256 vs h64: no material gain (A20 0.588 vs 0.608; Heart 0.443 vs 0.442, 0.446 vs 0.443, 0.349 vs 0.367).
- All tracks (242,100 runs) vs A20-only (80,100): corr 0.606 vs 0.608.

Decision-trained V (M=8 labels)
- 19,531 A20 decisions → +0.0019 ± 0.0010 per decision ("≈ 3% of the oracle headroom") → +0.0042 score in play: "≈ +0.36 floors, with wins −0.3 pts".
- 18,154 A10 decisions → +0.0026 ± 0.0023; joint 37,685 → +0.0028 ± 0.0024. "saturated at ≈ +0.002–0.003 per decision".
- Transfer A20 → A10: −0.0154.
- −0.0044 per card-reward decision went with −0.019 run score; "≈ 12 such decisions per A20 run [inferred]" (source's tag).
- Per-decision range: random vs vr_171 −0.0285 (A20), −0.0563 (A10); in-sample oracle +0.0699 (A20), +0.136 (A10), biased up.

Other
- Keys cost +1.6 pts of act-3 boss wins; every missing key collected free is worth "≤ +3.4 pts Heart wins".
- Shrinkage on confirmation: F16 +6.2 → +1.5 pts; F23 +1.0 / +2.0 → ≈ 0; A10 chain claimed +3.6 then +5.2 → +3.0; A0 +3.6 → +1.8.
- Throughput: A20 0.57 s/game-thread; branch eval ≈ 18 decisions/min (A20), ≈ 8/min (A10).

## 3. Diagnoses

- A20 dies mostly to bosses. One 1,000-seed slice: Collector 80, Automaton 77, Champ 72, Hexaghost 67, Slime Boss 63 = 359 deaths; elites Slavers 52, Gremlin Leader 45. Strength: a count on one slice. Act-2 bosses 229, act-1 bosses 130 [inferred from boss identities]; A20 mean floor 27.89–29.71 across slices.
- Extra search does not fix it cheaply: +20% gave +0.006 (P 0.75). One slice, weak.
- "V accuracy is not A20's bottleneck": a V with better holdout correlation lost −0.019 twice (P 0.041, 0.036). The mechanism (holdout metrics measure between-run separation, not the ordering of one decision's options) is [inferred, MED] in the source; F26 supports it.
- vr_171 is over-spread and optimistic at A20: pred mean 0.719 SD 0.275 vs label 0.652 / 0.352, slope 0.71 (post-hoc).
- Card-reward ordering: none of 91 models beats vr_171; decision training adds ≈ +0.002. Whether the oracle gap is real headroom was unknown here (F33's question).
- The A10 loss from vr_251 (−0.015) is not at card rewards (tie, −0.0002 ± 0.0016) but in the other V hooks: shop, removal, upgrades, boss relics [inferred, MED-HIGH in source]. A10 card and relic picks are V-only.
- Decision-trained V "helps the agent go further, not win more" (n=8,000) and is track-specific: "A20 rewards survival, A10 has a real win rate to protect [inferred, MED]".
- Heart: f16heart dies to the Heart in about half of act-4 arrivals (0.256 / 0.569); beats the act-3 boss in 0.644 of runs vs ≈ 0.85 for the A0 best (not paired).
- That gap "is not the keys; it is the Heart track's own policy (Heart-labelled V-only picks etc.), which trades act-3 survival for Heart kills [inferred, MED]".
- Before features v2 the Heart V could not see keys (vr_171 on the Heart holdout: rmse 0.7862, corr 0.271); the earlier Heart keeps "were false positives or do not compose [inferred]".
- "most of the Heart outcome is not predictable from the run state these features describe [inferred]"; after iteration 3 "the Heart V's ranking is no longer the bottleneck [inferred, MED]".
- V does not value a ruby recall against heal/upgrade better than an HP threshold [inferred, LOW-MED in source].
- Pure vr_171 card picks are at least as good as the A20 agent's weighted mixture (+0.0016 ± 0.0015).

## 4. Mentioned but never tested

1. `vr_model_card`: decision-trained V for card rewards only, vr_171 in all other hooks. Built; "an option worth testing later"; never run.
2. F34, conditional on F33: boss-relic decision data at scale plus a relic-only decision-trained V (`vr_model_relic`), if boss-relic H > +2 SE and ≥ +0.01. Outcome not in range.
3. Decision-level measurement or training for the non-card hooks (shop, shop relic, removal, upgrade, rest): vr_251 "was never checked" there; branch eval supports only cards (kind 1) and boss relics (kind 2).
4. Ensemble V: mean(vr_241, vr_171) B +0.0018 ± 0.0022; "no ensemble support in the agent".
5. V-only card picks at A20 (F26 reading ii); no arm.
6. Decision labels with more than M=8 playouts per option, or far more than ≈ 20k decisions per track ("too few or too noisy").
7. A truly compute-neutral A20 boss reallocation (other fights at ≈ 130–230 sims): "too drastic to pursue".
8. Budget scaling beyond equal compute (+20% → +0.006; `f22c_bm2`, boss ×2 at 500 sims, win 0.058 on 500 used seeds): outside the rules.
9. Moving to simulator commit 8d8d2c4+ (one-pass playable masks): "an owner decision; not asked for". (The session task list, outside my material, shows a later move to a fixed simulator as completed.)
10. Fixing the rollout crashes. Unverified hypothesis: `ViolenceAction`'s 64-slot `attackIdxList` overflows when the draw pile holds more than 64 attacks; "a debug build or an assert on drawPile.size() would settle it". (Same task list shows a later fix as completed.)
11. Remaining key headroom (7.5% keyless act-3 victories, ≤ +3.4 pts); only the ruby rule was tested.
12. A paired measurement of the Heart agent's act-3 survival against the A0 best, and any policy that keeps A0-level act-3 survival while building for the Heart.
13. Recalibrating vr_171 (slope 0.71) instead of scaling weights.
14. Richer run-state features than 666 + 4 keys; a different architecture for decision training (only the vr_171-shaped h64 MLP was used).
15. Branch (decision-level) data on A0 or the Heart track: none in range [inferred from absence].
16. Arms defined but never run because their gate failed: f14heart, f14heart_b, f27a20, f28a20, f31a10, f32a10, f32a20; the A10 confirmation of f24a10.

## 5. Method lessons

- Winner's curse: one fresh slice at P ≥ 0.9 passes small or null effects when many arms run. Rule since F25: a keep counts as a gain only after dscore > 0 on a fresh 2,000-seed slice against the last confirmed best.
- Holdout RMSE and correlation do not predict play (F4, F11, F20, F21); RMSE rejected the best Heart V. The branch metric agreed with play.
- A max over ≈ 4 noisy 8-playout means is biased up; use split halves.
- A V swapped in under weights tuned for another V can lose badly (−0.0885); a card-reward metric says nothing about the other hooks; the play test decides.
- Selecting a model on a set biases later comparisons on it (vr_251 on the F26 decisions).
- Effects of +0.004–0.008 score need 4,000–8,000 paired seeds (paired SE ≈ 0.012 at 1,000 A20 seeds).
- Verify compute neutrality by measured search actions (≤ 1.01×); the F22 calibration formula was wrong.
- The score can hide win gains: +6 pts Heart wins moved it only +0.006.
- State copies must not share mutable simulator objects; playouts that cross an act boundary exposed this.
- Operations: pre-register with predictions and disclose post-hoc hypotheses; keep games.tsv; make generation idempotent; run local queues as tracked background tasks (seven container restarts in one day); check byte-identity across machines.

## 6. Hints of a big untested lever

1. [inferred] Win-aligned objective at A20. Labels on the track's own goal gave the largest gain in this range (+6 pts Heart wins), while the A20 score is ≈ 0.58 from floor and ≈ 0.05 from wins (f29a20: floor 29.48, win 0.049; my arithmetic) and the kept decision V "helps the agent go further, not win more". No A20 arm here optimised wins. Lines 517, 440.
2. [inferred] The non-card V hooks carry large, unmeasured leverage: a V that ties at A10 card rewards lost −0.0154 in play through shop, removal, upgrade and relic hooks. Lines 431, 438.
3. [inferred] If F33's honest oracle shows real headroom (in-sample +0.136 A10, +0.068 A20 per decision; learning captured ≈ 3%), choosing by playouts at decision time, a run-level lookahead, takes it directly instead of through a learned V. Lines 391, 444, 464.
4. [inferred] A20 losses concentrate in five boss fights (359 deaths per 1,000 runs) and barely respond to search budget, which points to boss-specific deck preparation or a boss-specific combat policy. Lines 485, 483.
5. [inferred] The run-state representation looks like V's limit: 18× data, larger nets and three iterations left Heart correlation at 0.37–0.45. Lines 522, 505, 489.
