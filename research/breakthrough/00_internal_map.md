# 00 — Internal map: what was tried, what was found, what was never tried

Written 2026-11-16 from the eleven stage-1 digests (h1–h7, r1, c1, c2, d1), with disputed points checked in `research/program.md`, `research/results.tsv`, the four best-parameter files and `research/cpp`. Nothing was built or run.

Conventions. `pm MM-DD hh:mm` = program.md entry of 2026-MM-DD (UTC); early entries are cited by section name. Score = min(floor,51)/51 + win. dscore = paired score difference against the track best; P = P(improve). "1×" = the current sims caps, calibrated to the mean search actions of 500 full simulations. Tracks: A0, A10, A20, Heart (A0 with keys; win = Heart beaten). `[inferred]` = my reading. Levels at 1× (d1 §1): A0 85.4%, A10 52.5%, A20 7.6%, Heart 28.8%, A20 Heart 0.3%.

## 1. The agent in one page — as implemented

**Combat search** (`cpp/FairSearch.cpp`; c1 §1, checked in code).
- One tree per decision, not one per determinisation. Every simulation copies the real state, shuffles the unknown part of the draw pile and reseeds six RNG streams once, at the root. Nodes are keyed by an observation hash (turn, HP, block, energy, the hand in order, pile sizes, per-monster id/HP/block/intent, potions). Statuses and pile contents are not hashed.
- Chance has no nodes. Each edge holds a map from observation hash to child. After END_TURN the new hand is in the hash, so most simulations open a fresh child: the tree effectively ends at the turn boundary and the rest of the fight is one playout [inferred by c1 §6.1; tree depth and simulations per decision are not logged].
- One node is added per simulation. The playout runs to the end of the fight. It is not uniform: END_TURN has weight 0.05, every (card, target) pair and potion weight 1.
- Value is hand-set and per fight: win = 35 + HP + potion_v × potions kept − 0.01 × turn; loss = 10 × enemy damage fraction + 0.2 × turn + half the potion credit. Backup is a plain mean; Q is min-max normalised over all values seen in the search.
- Selection, A0/A10/A20: PUCT (c 1.5) at every node, MLP prior at the root only, uniform prior below. **Heart: no prior at all**, so UCB1 with c 0.7 (`best_heart_params.txt` has no `prior_mlp`; c1's parameter table is wrong here, c2 and d1 are right).
- Final move = most-visited root edge. Soft early stop: stop when the visit lead exceeds 0.1 × simulations left. Caps: 1000 / 950 / 1050 / 775 (A0 / A10 / A20 / Heart).
- A new searcher is built for every card play (no tree reuse, no transpositions, no whole-turn plan). Forced moves get no search. Potions are removed from hallway-fight search unless HP < 25%.
- Cost: 6–8k simulator actions and about 3 ms per decision; 286–480 decisions and 0.8–1.8 s per game; search is about 98% of game cost.
- Quirk: a new node counts a visit without a value, so non-root node means are biased low (c1 §1).

**Learned parts in combat.** Only the root prior: 204 aggregate state features + 16-d embedding of the candidate action's card or potion id + 4 target features → 64 → softmax; trained once on 298k root visit distributions of the 500-sim search. Learned leaf values, the mixed leaf, prior-guided rollouts, tree reuse, action budgets and rollout cut-offs exist in code and are off in all four bests.

**Strategy layer** (`cpp/FairAgent.cpp`; c2 §0–§3, parameter files checked).

| Decision | A0 | A10 | A20 | Heart |
|---|---|---|---|---|
| Card reward | KB tier + hand bonuses + 10×bf_g1 + 60×ΔV | argmax V, taken iff > V(skip) | KB + 25×bf_g3 + 162×ΔV | argmax V |
| Boss relic | KB + 30×ΔV | argmax V | KB + 162×ΔV | argmax V |
| Run value V | vr_171 | vr_171 | vr_251 (decision-tuned) | vr_222 (670 inputs with keys, Heart-win label) |
| Upgrade target, shop cards | KB + 60×ΔV | KB + 45×ΔV | KB + 162×ΔV | KB + 20×ΔV |
| Shop relic | first affordable | best ΔV | best ΔV | first affordable |
| Removal target | curse, Strike, Defend, lowest KB | V | V | as A0 |
| Rest vs smith | HP < 50% | HP < 60% | V(heal) vs V(own upgrade) | HP < 50%, key recall at HP ≥ 65% |
| Map | hand DP | path MLP pm_3 | path MLP pm_1 | hand DP + key rules |
| Root move prior | yes | yes | yes | no |

- The deck-value model bf_* and the simulated-fight relic rule are inert on A10 and Heart (pure-V branches return first).
- Events: one hand table of 52 cases written for "A0, act-3 boss", used on every track. Neow: legacy rule, mostly a uniformly random pick. Shop: removal first whenever affordable, then relic, then card. Lift, dig, toke, potion discard and out-of-combat potion use never happen.
- No active configuration simulates a future fight or rolls a copy forward at decision time. No plan or archetype state is carried.
- V: 666 run features (24 scalars, boss one-hot, relic one-hot, card-count vector) → 64 → 64 → 1; one label per run (its final score); about 519k runs.

**Harness.** Paired seeds, 4,000-sample bootstrap; keep iff dscore > 0 and P ≥ 0.9 on a never-used slice, counted only after a second fresh 2,000-seed confirmation. Equal compute = mean search actions per game within 1%. The per-game log has no deck, relic, path or per-fight columns.

## 2. Tried-map

Verdict codes: K = kept, D = discarded, C = closed line, I = information only.

| What | Best result | V | Reason stated in the log |
|---|---|---|---|
| **Combat search structure** | | | |
| UCB c 0.3 / 0.5 / 1.5; PUCT c 0.5–1.2; prior temperature | within ±0.03 (one retest −0.056); temperature 0.7 −0.145 | C | — |
| Robust-child final move | +0.003, +0.010 | D | — |
| Tree reuse in a fight | legacy A0 −0.0088 (P 0.37); F41a A10 −0.0015 (P 0.445, n 4,000) | D | subtree mostly resets when a draw or enemy roll changes the observation |
| Fixed action budget per decision | −0.065 (P 0.03) | D | starves the long fights that matter |
| Potions out of hallway search | +0.0492 | K | saves branching, keeps potions for elites and bosses |
| **Rollout policy** | | | |
| END_TURN weight 1 → 0.2 → 0.05 | +0.1866, +0.0682 (A0, 3–14% win era); 0.02: +0.018 (P 0.70) | K | second-largest gain in the ledger |
| Potion weight 0.2 | +0.016 (P 0.72) | D | — |
| Hand biases: play order; block when attacked; attack when safe | −0.455; −0.067; −0.093 (11–18% win era) | D | — |
| Prior-guided rollouts (bilinear prior, ε 0.25) | cost 2.43× → 210 sims: −0.062 (P 0.033) | C | "value comes from breadth, not rollout quality" |
| **Leaf / terminal value** | | | |
| Rollout cut at 2 / 3 / 6 turns + heuristic leaf | −0.37 / −0.28 / −0.17; bosses only at 8 turns: A20 −0.041, Heart −0.033 | C | — |
| Terminal constants (hp_w 2, win bonus 100 / 15, low-HP penalty, max-HP weight, loss shape) | −0.064; −0.117 / +0.002; +0.010 and −0.306; −0.023; −0.003 to −0.024 | C | risk aversion → passivity |
| Learned leaf, realised battle outcome (173k rows, 500 games), pure replacement | floors 6 / 11 vs 33 / 23 / 44 | C | all rows of a fight share one label → indifferent to within-turn progress |
| Learned leaf, search root-mean target (Phase B; Phase S 3×3 grid to 10⁶ decisions) | best cell 36.0% vs 75.0% (−0.565); cost ignored −0.572 | C | value error, not depth; inference up to 84× cost |
| Learned leaf on tree-leaf states, target = mean of 8 random rollouts (S2, F52) | 0.8M rows −0.449; 3.2M −0.348 / −0.365; 22.5M −0.290; 32.4M −0.298; h=96 −0.30 | C "for good" | RMSE saturates near 11.8 (target sd 33); the residual costs more than 1.75× sims buy |
| Mixed leaf, λ 0.5, with one rollout | −0.0215 at 675 sims vs control −0.0183 | C | adds nothing per simulation |
| **Move prior** | | | |
| Bilinear prior, all nodes | −0.010 to +0.022 | D | captures 31% of information |
| MLP root prior | A0 +0.044 (P 0.900); Heart ports −0.006 and +0.009 (P 0.79) | K (not Heart) | — |
| Prior below the root (depth 1, 2) | −0.010 at equal wall-clock, −0.009 at +40% compute; at 50 sims depth 2 +14 wins / 200 at 3× time | D | chance branching makes depth-1 nodes numerous |
| Better priors: 2,000-sim targets; 465k decisions + h128; v4 | −0.034; +0.014; +0.6 pts n.s. at 500 sims, +8.5 pts at 50 sims | D | at 500 sims the search corrects most prior errors; sharper targets over-focus the root |
| 4×-search teacher distilled on boss fights (Direction A) | not run | — | withdrawn on the three results above (pm 11-09 23:17) |
| **Compute allocation** | | | |
| 2× and 4× search | see §4 | I | owner: 2× for milestone scores only |
| Budget moved to bosses or the Heart at equal compute | A0 −0.042; A20 −0.013 (fresh); +20% compute +0.006; Heart fight 3× sims: +2.8 pts kill rate, −2.2 pts act-4 reach, net −0.004 | D | budget "near-balanced already" |
| Safe early stop + reinvest (F40) | A10 +0.024 pooled; A20, Heart, A0 ≈ 0 | K (A10) | saves 8–15% |
| Soft early stop 0.1 (F43/F45) | confirmations: A0 +0.016, A10 +0.032, A20 +0.021, Heart +0.030 | K all | 0.05, 0.15, 0.2 are worse |
| Speed work | micro-optimisation 1.029×; native flags 7–8%; fixed piles crashed | I | no single hotspot |
| **Run value V** | | | |
| Observational P(win) V as picker | −0.33 to −0.62 | C | deck bloat |
| V as additive bonus on card picks (89k runs) | A0 +4.8 pts, A10 +5.0; kept on all tracks | K | helps where items differ |
| V-only card and relic picks | A10 +0.070, Heart +0.032; A0 −0.028; A20 n.s. | K (A10, Heart) | needs exploration data |
| V on boss relics | +0.013 to +0.068, all tracks | K | — |
| V on shop cards, upgrades, shop relics, removal, rest | track-specific signs; rest hook −5.2 pts at A10; unbiased rest keep at A20 only | mixed | max over noisy upgrade values biases toward smithing |
| V on events (4 re-seeded copies rolled forward) | ≤ +0.01; Heart −0.046 | D | V has no battle states |
| Weight steps after the first | confirmations −0.002 | I | selection luck |
| More data, bigger nets, iteration | F4 +120k games: nothing; h256 / h1024: nothing; A20 corr 0.554 → 0.608: −0.019 twice | D | level accuracy is not ranking accuracy |
| Exploration data (X1) | +0.029 (P 0.908, legacy sim) | K | makes V usable alone |
| Heart V: plain; + key features; iteration 2; iteration 3 | −0.326 (collapse); +6 pts wins, score +0.006; +1.5 pts confirmed; n.s. | K (two) | label must match the objective |
| Decision-trained V (8-playout labels, 19.5k decisions) | +0.0019 per decision; play +0.0042 (n 8,000); A10 transfer −0.015 | K (A20) | "further, not more wins" |
| **Deck value bf_*** | | | |
| Fight model replaces rules (pick, remove, upgrade) | −0.07 to −0.21 | C | myopic; option differences too noisy |
| Fight model as bonus at card rewards | A0 +0.054, +0.051 (three slices); A10 +0.062; A20 +0.017 not replicated | K | — |
| Variants: weights, shop, upgrades, iteration, label averaging, pooled ascensions | −0.06 to +0.01 | D | iterated decks carry less contrast |
| Online counterfactual fights for card picks | −0.267, −0.224, cost 2.3× | C | winner's curse; weak hypothetical play |
| KB synergy pairs; boss and act-4 prep bonuses | A0 −0.007; A10 +5.7 pts val, 0 on dev; neutral | weak K / D | — |
| **Path model** | | | |
| Hand DP weights and elite HP gates | A0 +0.034; A10 +0.065; A20 floors 21.8 → 25.6; 53 discards, median −0.005 | K then saturated | — |
| Elite node valued by 3 simulated fights | Heart +0.002; A20 −0.015; A0 −0.049; +37–54% cost | C | 100-sim fights without potions misjudge the agent |
| Learned path MLP (F35/F36) | A20 +0.030 (+1.3 pts); A10 +0.050 (+3.0 pts), pooled | K | paths compound |
| Second round, 2× data; Heart model; transfer to A0 / Heart | not armed; −0.002; −0.0075 with act-4 reach −6 pts | C | saturated at 8-playout labels; no key knowledge |
| Elite veto below 50% HP (F51) | +0.0003 (P 0.57) | D | alternatives cost as much |
| **Other strategic hooks** | | | |
| Open treasure chests | +0.270 | K | inherited handler never opened one |
| Event rules, KB relics, AoE and scaling bonuses, rest thresholds | +0.04 to +0.09 each, early | K | — |
| Potions: save for bosses; values per fight type; out-of-combat use; act-4 shop potions | A0 +0.035, A10 −0.091; neutral; −0.022; Heart +0.016 | mixed | A0 saves, A10 must spend |
| Neow ranking | −0.017, +0.010, A20 −0.0215 | C | — |
| Boss relic by simulated next-act fights | Heart +0.018 (P 0.986); others ≈ 0 | K (now inert) | helps only before a known fixed fight |
| Honest 8-playout headroom (F33/F34) | cards −0.020 / −0.009; boss relic −0.007 / +0.004; rest +0.004; map +0.014 | I | only paths are below the ceiling |
| **Heart / keys** | | | |
| Key rules | 8.2% → 10.2%; key completeness 96% moved nothing | K, then C | conversion is what remains |
| Heart-fight settings (potion credit, horizon, 2× sims) | no keep | D | — |
| A20 Heart (F47) | 0.3%, act-4 reach 2.5% | I | key policy costs ≈ 4 floors at A20 |
| Heart milestone at 2× search (F49, test split) | 175 / 500 = 35.0% [30.9%, 39.3%] | I | — |
| **Simulator fidelity** | | | |
| Phase-2 fixes; fidelity fork; 28 more fixes; crash and relic-order fixes | +0.043; −5.5 / −1.1 / −3.35 / −0.9 pts; noise; ±1 pt | K | the fork is harder, most at low search |
| Real-game regression, 1,765 recordings | 1466 pass, 250 limited, 49 explained | I | — |
| **Data and method** | | | |
| Fresh-slice and confirmation rules | kept gains shrink by about half | K | selection bias |
| Split-half oracle | in-sample "headroom" +0.06 to +0.14 was noise | K | — |
| Ports across tracks | 0 of 5, 0 of 4 | I | — |
| Offline fit as a proxy (leaf RMSE, prior cross-entropy, V correlation) | better fit gave no gain or a loss in play: F4, F11, F20, F21, sv_12, the 2,000-sim-target prior | I | fit measures level, play needs within-decision ranking |
| Slice size | one model read −0.19 on 300 seeds, −0.29 on 2,000 | I | decide on ≥ 2,000 fresh seeds |
| Leak tests | event test caught a real leak (relic-pool order) | K | combat and events are covered; other out-of-combat hooks have no test (c2 §5) |

## 3. Checklist

| | Item | Status | Evidence and result |
|---|---|---|---|
| a | Fight end scored by the run value | **never** | Planned as Phase R stage 3 (pm 11-02 "Phase R" plan); "conditional on a stage-2 keep and does not start" (same section). Never revisited after V was kept on all tracks. No V call in `FairSearch.cpp`. Only constants were re-tuned |
| b | Leaf or turn-boundary value trained on realised outcomes of the search agent's fights | **partly, once, tiny** | pm 11-01 04:20: battle-outcome labels, 173k rows / 9.1k battles / 500 games, pure leaf at every in-turn state → collapse. Every later net regressed the search root mean (≈ 10⁶ decisions) or the mean of 8 random rollouts (up to 32.4M rows). Never at scale, never at the turn boundary, never with rollouts kept |
| c | Card-identity or entity-level combat inputs | **never** (state); partly (action) | State = 204 aggregates with 26 named-card counts; the prior embeds only the candidate action's id. F52 verdict names a new representation "a new line" (pm 11-11 12:45) |
| d | Move prior below the root | **tried** | Bilinear at all nodes: neutral. MLP depth 1: −0.010 at equal wall-clock (365 sims), −0.009 at +40% compute. P1 depth 0–2 at 50 sims (pm 11-05 22:10–23:19) |
| e | Learned or tuned playout policy | **partly** | Tuned: END_TURN weight (+0.19, +0.07; cost ≈ 1×); three hand biases (−0.07 to −0.45). Learned: bilinear-prior rollouts at cost 2.43× → −0.062. Nothing since batch 28; no MLP or distilled fast policy; nothing on A10 / A20 / Heart |
| f | Chance nodes, draw bucketing, turn-boundary fix | **never** | No mention in program.md (keyword search); c1 §6.1 |
| g | Common random numbers or sequential halving at the root | **never** | Common random numbers used only offline (strategic labels) |
| h | Whole-turn plans or transpositions | **never** | c1 §2, §6.6 |
| i | Tree reuse | **tried twice** | Legacy A0 −0.0088 (P 0.37); F41a A10 −0.0015 (P 0.445), pm 11-09 20:40. Not on A20 / Heart |
| j | Search above 4× | **never** | 4× only on pre-early-stop bests, 1,000 seeds (F39, pm 11-09 16:16); current bests only at 2× |
| k | Perfect-information run of our own search | **never** | The one "cheating" diagnostic used lightspeed's search: 9.6%, −0.205, 16 crashes (results.tsv row 27) |
| l | Same combat agent on strong reference decks | **never** | r1 §4; d1 §6.5 |
| m | Strategic choice by simulated fights or run playouts at decision time | **partly** | Fights: card picks −0.27 / −0.22; elite nodes closed; boss relics Heart-only keep; events (rolled to V) discard. Run-level playouts never in play (cost est. ≈ 100×); the offline equivalent at 8 playouts beats the agent only at map nodes (+0.014). More than 8 playouts never |
| n | Reinforcement learning | **never** | Only supervised regression and one-step distillation |
| o | Hidden information for a critic or teacher only | **never** | — |
| p | Restart from saved states | **partly** | In-process forks for offline labels (branch evaluation, deck-value fights). No saved-state files, no fight-level benchmark, no training restarts from hard fights |
| q | SPSA / CMA-ES | **never** | One parameter at a time; a six-parameter combo (+0.051); 51 of 161 named parameters never tested (h7 §2) |
| r | Objective trained for A20 Heart | **never** | F47 measured borrowed settings only (pm 11-10 06:10) |
| s | Potion policy beyond flat credit | **partly** | Hand rules only: per-fight-type credits, save-for-boss, hallway ban, act-4 shop potions. No identity or context value |
| t | Neow, events, shop learned | **partly** | Neow: hand ranking only, neutral. Events: V roll-forward, discarded. Shop: V bonus on cards (kept) and relics (A10 / A20); order and removal rule hand-written |
| u | Behaviour profile vs expert humans | **never** | The 1,765 recordings serve simulator regression only |

## 4. Established sensitivities

**Search budget (score, win points).**

| Track | 2× on older bests (F39 / F42) | 4× (F39) | 2× on current bests (F44 / F48) |
|---|---|---|---|
| A0 | +0.036 | — | +0.052 (84.8 → 89.0%) |
| A10 | +0.076 / +0.054 | +0.139 (48.2 → 57.5%) | +0.086 (50.4 → 56.6%) |
| A20 | +0.051 / +0.048 | +0.076 (6.0 → 9.3%) | +0.030 (7.65 → 9.10%) |
| Heart | +0.063 / +0.045 | +0.091 (26.8 → 32.5%) | +0.069 (27.9 → 32.7%) |

- Second doubling: A10 +0.063, A20 +0.025, Heart +0.029. Less search: A0 at 0.675× −0.018. Low end (A0, fork): 50 / 100 / 200 sims = −13.2 / −6.2 / −4.4 pts.
- 2× lowers every stage's death rate by 1–4 points; relative reduction per doubling 20–35% at A0, 8–20% at A10, 5–13% at A20 (d1 §3).
- Heart fight loss rate: 49 → 47% at 2×, 49% at 4×. Three times the simulations in that fight alone: +2.8 pts kill rate.
- Equal-compute re-allocation: early stop +0.016 to +0.032 confirmed per track. Speed: 7–8% from compiler flags, about 3% from micro-optimisation.
- Cost split: rollout ≈ 87% of a simulation; one small-MLP evaluation ≈ 45% of a simulation.

**What each class of change was worth** (r1 §2; first-measurement sums, inflated by about 0.48 of 3.35 through selection).
- Learned models 50% (V 37%, deck value 9%, path 3%, prior 1%); parameter tweaks 21% (END_TURN weight alone 8%); policy holes 16%; fixes 9%; early stop 4%.
- Largest single moves: chests +0.270; END_TURN weight +0.187; first hand policy +0.175; fidelity fork −0.055 to −0.105.
- V used additively: +4.8 to +5.2 pts. Objective-matched Heart V: Heart wins ≈ 16.5 → 25.3%. Path model: +1.3 pts (A20), +3.0 (A10). Root prior: +0.044 at 500 sims, +17 pts at 50 sims.

**Data scaling.**
- Leaf value: 3.2M → 22.5M → 32.4M rows = −0.365 → −0.290 → −0.298; RMSE 11.93 → 11.83. Earlier: 0.8M → 3.2M = +0.10.
- V: holdout RMSE 0.5128 → 0.5046 → 0.5019 at 35k → 89k → 170k runs; ridge ≈ nets; nets peak at epoch 1–2; +120k on-policy games: nothing in play.
- Decision-trained V: 19.5k decisions +0.0019 per decision; 37.7k joint +0.0028 ± 0.0024.
- Path model: first round took about half of A20's honest headroom (+0.0133 of +0.0251); 2× data +0.0071 ± 0.0039.
- Prior: information captured 31 → 49 → 62%; play gain only at low sims.
- Deck value: 2× games −0.019; label averaging −0.008.

**Stakes and noise.**
- Random pick vs own, per decision: boss relic −0.19 (A10) / −0.10 (A20); card −0.059 / −0.030; map −0.025 / −0.015; rest −0.017 / −0.011.
- Keys cost 1.6 pts of act-3 clears at A0 and about 4 floors at A20; each missing key collected free is worth ≤ +3.4 pts of Heart wins.
- Paired SE ≈ 0.012 at 1,000 A20 seeds; effects of +0.004–0.008 need 4,000–8,000 seeds. Of the seeds won at 1×, 52% are won again at 2× (A20).
- Data cost: 97,500 A0 games ≈ 2 h on 10 workers ≈ $30; 8-playout branch labels ≈ 340–600 seeds per worker-hour.

## 5. Loss anatomy in ten lines

1. A20 (14,000 games): 24 / 51 / 25% of losses end in acts 1 / 2 / 3; 53% on boss floors.
2. A20 act 2 is the wall: 34.5% of entrants die before the boss, 39.6% of the rest at it; 25% die at the act-2 boss even from ≥ 85% HP.
3. A20 act 3: 30.7% die before the boss, 30.2% at the first boss, 49.4% at the second.
4. A20 killers by share of games: boss 48.7%, elite 23.2% (Slavers 24% per fight), hallway 18.5%.
5. Attrition (240 traces): a hallway fight costs 11–16% of max HP, an elite 25%, a boss 25–38%.
6. Winners differ in resources, not end state: relics at the act-2 boss 10.2 vs 8.4, removals 3.7 vs 2.35, shop gold 705 vs 325; final deck size and max HP do not separate them.
7. A10: 50% of losses in act 2; act-2 boss 17.4% and act-3 boss 14.7% of entrants. A0: 68% of losses on boss floors.
8. Heart: 59% reach act 4 and 49.9% of Heart fights are lost (40% of all losses); 34% die before clearing act 3 vs 14.6% on the A0 track; 9.5% of act-3 clears lack a key.
9. More search shifts every stage a little and removes no death floor; it does not move the Heart fight.
10. Potions: 40% of A20 hallway deaths (7.4% of games) end with an unused potion; boss deaths hold almost none.

## 6. Never-tested list (union, deduplicated)

**Combat search**
- Chance nodes, hand bucketing, afterstates or a turn-level search (c1 §6.1).
- Common random numbers and sequential halving at the root (c1 §6.4).
- Canonical turn plans, transpositions (c1 §6.6).
- Fixing the under-counted node mean; a prior or completed-Q below the root (c1 §6.7).
- Search at 8× and beyond (h2 §4, d1 §6.2).
- Perfect-information ceiling (r1 §6, d1 §6.1).
- Tree reuse on A20 / Heart with the known-top fix (h1 §4).
- The "pathology at high simulation counts" check (h5 §4, h7 §4).

**Playout and value**
- Any learned playout policy other than the 2.43× bilinear one; any rollout-policy change for boss fights (h7 §4, r1 §6).
- Outcome-trained value at scale (h1 §6).
- Learned leaf with a short rollout kept (h6 §4).
- A new state representation (h1 §4).
- Training states matched to visited states (h1 §4).
- Batched or GPU evaluation (h6 §4, h7 §4).
- Fight end scored by V (h7 §4, c1 §6.3, c2 §6.1).
- Iterated distillation; a prior trained on act-4 games (h5 §4, h7 §4).
- A 100-sim agent with prior v4 as a cheap data generator (h5 §4).

**Strategy**
- A fight-outcome model composed along the map (c2 §6.2, §6.5).
- A structured deck encoder (c2 §6.3).
- Sequence-level planning or reinforcement learning over the ≈ 100 strategic decisions (c2 §6.4).
- Identity-aware potion value (c2 §6.6).
- Headroom at shops, events, removals, upgrade targets, Neow, potions; all headroom on A0 / Heart (h2 §4).
- Labels with more than 8 playouts (h2 §4, h3 §4).
- A card-only decision V; V ensembles (h3 §4).
- V on battle states (h4 §4).
- Re-test of hook signs on the current V (h4 §4, h5 §4).
- Action-coverage audit: Match and Keep, lift / dig / toke, sapphire-key choice, full-belt potions (h6 §6, c2 §6.7).

**Objectives and diagnostics**
- An A20 Heart track and A20 key policy (h1 §4, r1 §6).
- A win-aligned objective at A20 (h3 §6).
- Heart-fight traces and the winner-vs-loser analysis (h5 §4, d1 §6.3).
- Boss experiments from the audits: race vs block, potion timing, targets (h1 §4).
- Deck-versus-piloting test (d1 §6.5).
- Replicate runs per seed (d1 §6.6).
- Per-fight logs at scale (d1 §6.4).
- Human par statistics (d1 §6.8).
- Any research use of the real-game recordings (h2 §4).

**Method and engineering**
- Black-box tuning; the 51 untested parameters (h7 §4).
- A structural simulator speed-up (h5 §4, h7 §4).

## 7. Contradictions and weak evidence

1. **"The leaf-value line is closed for good"** covers one design only: a small MLP on 204 aggregates, regressing random-rollout means, replacing rollouts. The only outcome-trained test had 500 games. It does not rule out other targets, inputs or uses [inferred].
2. **"Combat search quality is the bottleneck"** (F39) against **"deck-strength gap"** (F42b, F50) and d1: extra search is a uniform small shift, A20 flattens after 2× and the Heart fight does not respond. Both audits are small, correlational trace readings; entering HP also reflects combat quality.
3. **"Out-of-combat decisions are near saturated"** rests on one-decision changes under the agent's own continuation with 8 noisy playouts (halves agree on the best option in 35–60% of decisions). The path model gained +0.030 / +0.050 per run from +0.013 per decision, so the local test understates coordinated change. A joint policy change was never tested.
4. **"A20 bosses are not search-limited"** (B20) was measured on selection-exhausted seeds and contradicts F39 / F44; A20 val-era discards are unreliable (r1 §5).
5. **Rollout-policy evidence is old**: the hand biases were tested at 11–18% win on the original simulator, and the one learned-rollout test used the weakest prior. "Breadth beats rollout quality" rests on that single cell.
6. **Tree reuse "tie"**: one valid test, on A10, with the known-top information frozen.
7. **"HP is invisible in boss-fight values" rejected**: only crude cut-offs and constants were tried; the premise (random rollouts cannot win boss fights) stands, without a fix.
8. **Heart conversion** is called both the deck's problem and unresponsive to search. It rose from ≈ 26% to ≈ 50% when the Heart-labelled V arrived, which supports the deck reading [inferred]; no Heart-fight traces exist.
9. **Soft stop "recovers 94% of the 2× gain"** is one slice; 2× on top of it still gave +0.086 at A10.
10. **Single-slice keeps never replicated**: exploration-trained V (P 0.908, no sign of transfer to the fork), map discount 0.85 (dev +0.000), A10 synergy bonus (0 on dev), A20 deck-value bonus (dev2 +0.007), the A20 potion and bonus-weight keeps (val only). The per-hook signs of F5–F7 were each judged on one slice; the joint re-measure kept +0.047 of a claimed +0.130 at A10.
11. **A0 "4× compute = +4.5 pts"** is unpaired and has no ledger row. The 4× points of F39 are 1,000 seeds on older bests.
12. **Digest errors**: c1's table gives the Heart track a root prior (it has none); h1's "206 features" is 204 features plus two label columns; program.md's frozen-protocol text says floor / 55, the code uses 51.
13. **Search constants are stale** [inferred]: the UCB constant the Heart track runs on, the win bonus and the HP weight were last tested at A0 in the 10–38% win era (results.tsv rows 8–9, 41–43, 62, 83–84). The loss-shape constants were tested at A20 and Heart on the legacy simulator only. Apart from potion credits, no terminal-value constant was ever tested at A10 or on the fidelity fork.
14. **Old loss statements** ("53% of A20 runs die in act 1", "≈ 26% Heart conversion") describe early agents; use d1.
15. **The score hides wins**: +6 pts of Heart wins moved the score +0.006; A20 keeps are mostly floors. No ledger row isolates strategy headroom or the value of hidden information.
