# Plan C — one loop, one currency: unified expert iteration over whole runs

Conventions. File names refer to `research/breakthrough/`. [estimate] = my arithmetic, [inferred] = my reading. Box-hour = one 4-vCPU machine for one hour; the fleet is 240 box-hours a day; USD at $1.5 per box-hour (h1 §2: ten workers for 2 h ≈ $30).

## 1. Thesis

The missing strength sits in the seam between the two halves: fights maximise `35 + HP + flat potion credit` and never see the run, while the strategy layer takes one-step argmaxes of a value fitted to one noisy label per run and never sees a fight, so neither can learn what the other needs. The mechanism is one expert-iteration loop over whole runs with one currency, the probability of reaching the run objective (act-3 win or Heart kill), used both as the terminal utility of every fight and as the criterion of every strategic choice; two entity-level networks are trained on search-improved decisions and on realised outcomes of the agent's own search-guided play. Now, because the cheap forms are exhausted (372 discards, median dscore −0.0057, r1 §3), the fleet produces 1–2.5M search-played runs a day (s5), and the loop's two ingredients are the two things the history never tried: iterated policy improvement on run outcomes (00 §3 n) and a fight scored by run value (00 §3 a).

Evidence:

1. **The split is literal in the code.** The fight value is a fixed linear utility, blind to what follows the fight; a loss even pays for unused potions (c1 §6.3). V "is never asked what a fight outcome is worth" (c2 §6.1). The repair, Phase R stage 3, never started (h7 §4.1).
2. **The strategic signal saturated, not its fit [inferred].** V has RMSE 0.455 on a target with spread 0.715 (c2 §2); better fit gave nothing or a loss in play six times (00 §2); decision-trained V stopped at +0.002–0.003 per decision on 8-playout labels (h3 §2). Yet the one model trained on within-decision differences of a compounding decision turned +0.013 per decision into +0.030 per run at A20, and +0.011 into +0.050 at A10 (h2 §2), and "a joint policy change was never tested" (00 §7.3).
3. **Matching the label to the objective is the largest learned effect on record:** the Heart-labelled V took Heart wins from ≈16.5% to 25.3% (r1 §6.3). At A20 no arm ever optimised wins; the kept gains are floors (h3 §6.1, h4 §3).
4. **The combat evaluator failed the way the AlphaGo lineage predicts.** It regressed the mean of 8 random rollouts on 204 aggregates, saturated at RMSE 11.8 and lost 0.30 at 10× data (00 §2). "No system in the lineage trained a value net to imitate rollouts of a weak policy" (s1 §2a). The only outcome-trained test had 9.1k battles (00 §3 b) against the ≥10⁵ independent samples Expert Iteration needs (s5 §4.2).
5. **Search alone cannot close the gap [inferred].** A20 goes 6.0 → 8.5 → 9.3% over two doublings; the Heart fight stays at 49 → 47% lost; 25% of act-2-boss entrants with ≥85% HP still die (d1 §3, §4; 71 traces).
6. **Agents trained on the run outcome are ahead at a comparable budget.** spire-engine self-reports 93.1 / 75.2 / 21.9% on its own engine with whole-run PPO and no search, so "the 3 ms limit does not explain the gap" (s6 §4). Spire Mentor's PPO with search in the loop gained +7.5 to +15.5 points per campaign of 38k–144k runs, and learned strategy plus search was the best cell of the layer-swap table (s4 §1, §6). "No strong system learned deck building from a value of the deck in isolation" (s4 §2).
7. **A network fits in the tree if built for it:** an NNUE-style evaluation costs 1.5–4.6 µs measured, against ≈6 µs per simulation today (s5 §2, c1 §4); KataGo's efficiency kit is worth ≈9× (s1 §2g).

Counter-evidence: three outside projects failed to make learned priors or values beat rollouts at equal budget, or to distil search into a policy (w1 dead ends); Mentor deploys a hand-tuned leaf evaluation (s4 §1). Hence the staging: the currency and the strategic half do not depend on the combat network.

## 2. Target design

**Currency.** Objective flag o ∈ {act-3 win, Heart kill}. Every value is P(o | state) under the agent's own play. Progress (floors, acts cleared) is an auxiliary head, never the decision criterion.

**Run network R (strategy).** Inputs: the deck as a multiset of (card, upgrade) embeddings, sum- and mean-pooled; relic embeddings with counters; potion identities; HP, max HP, gold, floor, act, ascension, o, keys; act boss; the path model's 20 map features; one token per legal option (type, pointed card / relic / room, price). Trunk 2×512, ≈0.6M parameters. Heads: policy logit and Q per option (action as input, masked softmax; s3 §2.1, s4 §5); values P(o), P(clear current act), P(survive to the next rest), floors remaining, HP fraction at the next boss; a **u-head**, monotone in HP, that prices post-fight states. R takes every non-combat decision, including those now random or never taken (Neow, lift / dig / toke, potion discard; c2 §1).

**Combat network C.** NNUE-shaped: ≈8k sparse features (card × upgrade × pile, the draw pile as an unordered multiset; monster slot × id / intent / bucketed HP, block, powers; player powers; relics; potions; energy, turn; encounter), ≈100 active; a 256-wide integer accumulator; a 256→64→heads tail. About 2M table weights, of which ≈25k are touched per evaluation: the cost class of a ≤30k dense net. Card and relic embeddings are shared with R (sharing helped ByteRL, s4 §3). Heads: policy logit per legal (card, target); P(death in this fight); HP-loss histogram (16 bins); potions used; turns left; HP lost over the next 1 and 3 turns (s1 §4).

**The bridge.** At fight start R's u-head fills a table U[h, p] = P(o | post-fight state with HP h and potion set p): 12 HP knots × ≤32 potion subsets. A simulation that wins the fight is worth U[h_end, p_end]; death is worth 0 plus a damage-fraction term held below the smallest win value, so today's boss-fight gradient survives (00 §7.7). C's leaf value is Σ_bins P(survive, bin) · U[bin]. C never has to learn run value and survives every change upstream (s1 §4).

**Search at play time.** Fights: the existing observation-keyed tree and determinisation, with four staged changes, each its own paired test: (1) U-table terminal; (2) C's value at the END_TURN afterstate, reached by a random completion of the current turn, mixed with the rest of the rollout until a test removes it (complementary estimates of strong and weak play, s1 §2a); (3) C's policy as PUCT prior at every expanded node; (4) Gumbel root with Sequential Halving on common random numbers (c1 §6.4). Strategy: one batched pass of R over the legal options; shops enumerate affordable bundles; argmax of Q. No fight is simulated at decision time.

**Play-time cost** (one core; today ≈3.5 ms ≈ 500 effective simulations × ≈7 µs, rollout 87%; c1 §4):

| Item | Cost | Source |
|---|---|---|
| Copy, in-turn descent, expansion | ≈0.8 µs (13% of 6 µs) | c1 §4 |
| Random completion of the turn, ≈4 engine actions | ≈1.6 µs | c1 §4 |
| C at the afterstate (accumulator refresh + value heads) | ≈3 µs | s5 §2 (1.5 µs incremental at width 256) + [estimate] |
| C at the expanded node for the prior (stage 3) | ≈3 µs | same |
| Rest of the random rollout, while mixed | ≈3.6 µs | c1 §4 |
| Simulation: value only / + prior / + prior + rollout | ≈5.4 / 8.4 / 12 µs → ≈650 / 415 / 290 per 3.5 ms | [estimate] |
| U-table, once per fight | ≤384 × 19 µs ≈ 7 ms ≈ 0.35 ms per decision over ≈20 decisions | s5 §2, c1 §2 |
| Strategic decision | ≤30 options × 19 µs < 1 ms, ≈100 per run | s5 §2 |

Only the value-only form runs more simulations than today; prior and rollout must each pay for themselves at equal calibrated wall-clock (`tools/calibrate.sh`, c2 §5).

**Training loop** (one generation ≈ one fleet-day):

1. *Self-play runs* with the current C, R and search. Ascension uniform over {0, 5, 10, 15, 20} (Mentor's choice, w1 §1.1); 30% of episodes restart from the state bank (saved act and fight starts), weighted to where runs die (+3.6 points for one character, +0.4 for another, s4 §5); ε = 0.2 random strategic picks (the X1 precedent, h7 §1).
2. *Playout-cap randomisation.* A quarter of combat decisions get a 4–8× search and are recorded as policy targets (completed-Q, forced visits pruned); the rest run at ¼ budget so games finish (s1 §2g).
3. *Strategic improvement operator.* On 10–25% of strategic decisions: clone the run, take each option, re-sample all hidden randomness with the same seeds across options, play 64 continuations in total (Sequential Halving over options) at ¼ search budget to the end of the act's boss fight, bootstrap with R's P(o); target = softmax of completed-Q. ≈4.5 core-seconds per labelled decision → ≈0.3M a day on 40% of the fleet [estimate], against 19.5k decisions for the whole of F28 (h3 §1).
4. *Targets.* C-policy ← deep-search distribution. C-heads ← the realised fight outcome under search play, two decorrelated afterstates per fight (s5 §4.2), ≈20M fights a day [estimate]. R-policy and Q ← step 3. R-values ← the realised run outcome. u-head ← interventional labels: the same post-fight state restarted at a grid of HP and potion sets.
5. *Promotion* only by paired test (§3); stored states are re-searched with the newest networks (Reanalyse, s5 §4.7).

**Hidden information, training only.** (a) Common random numbers across options and siblings. (b) Privileged critics that also see the episode's luck (the A20 second boss, upcoming offers and elites; the next draws and enemy rolls); public heads regress on ½ outcome + ½ privileged prediction, which keeps the mean and cuts variance (AlphaStar 22% → 82% with a privileged value, s2 §2; PerfectDou 0.524 vs 0.486, s3 §2.3). (c) Perfect-information search as a ceiling diagnostic only; as a target it is biased upward (s3 §2.7). C and R read observations only; the leak test is extended to every non-combat decision (uncovered today, c2 §5).

**Reused:** simulator, tree, determinisation, early stop, harness, the in-process forks (they become the state bank), the 519k logged runs (R's pre-training), the four bests as generation-0 teacher and as baseline of every gate. **Replaced:** terminal constants; the 204-feature root prior; random playouts as sole evaluator; vr_*, bf_*, pm_*, KB rules, event table, shop order, rest thresholds; four parameter files → one agent conditioned on ascension and objective.

## 3. Roadmap

All play gates: never-used paired seeds, equal calibrated wall-clock (ratio ≤ 1.01); keep iff the difference is > 0 with P ≥ 0.9 on 2,000 seeds and again > 0 on a fresh 2,000-seed confirmation. Metric: win-rate difference (dwin); dscore is reported.

| Phase | Deliverable | Eng. days | Compute | Gate | Kill |
|---|---|---|---|---|---|
| **P0 Foundations** | Entity-state export; save / restore state bank; fight suite (5,000 A10 / A20 elite and boss entries, 2,000 Heart entries, paired seeds); per-fight logs; wall-clock calibration; diagnostics: perfect-information ceiling, 8× and 16× search on 500 seeds | 5 | 150 box-h, $225 | Unchanged agent bit-identical; fight-suite SE ≤ 0.7 points | none |
| **P1 One currency** | U-table from interventional labels as the fight terminal value, on the unchanged search (§4 E1) | 4 | 150 box-h, $225 | A10 or A20 passes the play gate | Pooled 4,000 seeds within ±0.01 score on both tracks: drop the component, cut T1 by a third |
| **P2 Combat half** (needs the owner to reopen the evaluator line) | C; search stages 2–4; two expert-iteration rounds on the fight bank, then whole runs | 15 | 600 box-h, $900 | (i) offline sibling ranking (§4 E2); (ii) fight suite: A20 boss deaths −10% relative; (iii) runs: A10 dwin ≥ +3 points, A20 > 0; (iv) round 2 beats round 1 | (i) fails, or A10 dwin < +1.5 points after two rounds: close the line for good; keep the U-table and the search changes that passed |
| **P3 Strategic half** (parallel with P2) | R cloned from the bests' decisions, then three generations of improvement targets on all decision surfaces | 15 | 700 box-h, $1,050 | Honest headroom (§4 E3); A10 dwin ≥ +4 points and A20 ≥ +2 points (7.6 → ≥ 9.6%) | After ≥1M improved decisions A10 < +1.5 and A20 < +1: one fallback campaign of PPO with search in the loop (150k runs); if that is also < +1.5 at A10, stop |
| **P4 Closed loop** | Both networks regenerate each generation; curriculum, auxiliary heads, privileged critics, Reanalyse; optional GPU teacher (set transformer, 1–5M parameters) distilled into R | 20 | 2,400 box-h, $3,600; optional 336 GPU-h, $115–300 | Promotion per generation; A20 ≥ 15% on 4,000 fresh seeds by generation 5; T1 measured once on the test split | Three generations without a confirmed promotion on any track, or A20 < 12% after six |
| **P5 Heart at A20** | Objective flag = Heart; keys as policy actions; act-4 state bank and boss practice; the first A20 Heart track | 15 | 2,400 box-h, $3,600; optional 336 GPU-h | A20 Heart ≥ 5% on 4,000 seeds after three generations; T2 on the test split | Heart kills per act-4 entry < 30% after four generations while act-4 reach rises |

Total: 74 engineering days; ≈6,400 box-hours (27 fleet-days), ≈$9,600 at the project's rate (≈$1,150 at the raw cloud price of $0.18 per box-hour, s5 §3); GPU optional, ≤ $600.

## 4. First two weeks

Three experiments, under 250 box-hours (≈$375). E2 needs the owner's reopening; E1 and E3 do not.

**E1 — run value as fight utility (days 1–6).**
- *Build:* save / restore at "fight just ended"; a monotone u-model (the existing 666 run features + next-room context → 64 → 64 → 12 non-negative increments over HP knots, plus an additive term per potion held).
- *Data:* 20,000 post-fight states from A10 and A20 games; each restarted at 6 HP levels (10–100% of max), with and without one potion; 8 continuations per variant by the track best, the same 8 re-seeds across the 12 variants: 1.92M partial runs ≈ 80 box-hours [estimate]. Loss centred within the state, as `train_dec.py` does.
- *Test:* A10 and A20, 2,000 fresh paired seeds. Arms: best; best + U-table; best + U-table read from the existing vr_* (shows whether interventional labels matter); best + U-table + potions back in hallway search (they are priced now; 7.4% of A20 games die in a hallway holding a potion, d1 §4).
- *Go:* any U-table arm passes the gate on either track. *Stop for this component:* all arms within ±0.01 on 4,000 pooled seeds. Either way it measures the HP : potion : win exchange rate by context, fixed at 1 : 4–8 : 35 in the code (c1 §5).
- *Different from what failed:* hp_w, win bonus and low-HP penalty were global scalars (00 §2); V's rest hook used observational labels (h5 §3). This is context-dependent and interventional.

**E2 — can an outcome-trained entity value rank better than rollouts? (days 1–10).**
- *Build:* sparse entity export at END_TURN afterstates; C trained on CPU.
- *Data:* 1M fights from the state bank, weighted to elites and bosses, played by the current agent at 4× simulations ≈ 40 box-hours [estimate]; two afterstates per fight with realised death, final HP, potions used, turns.
- *Test, offline:* 3,000 held-out decision points × 3 sibling afterstates; reference = mean U of 32 continuations by the 4× agent on common seeds (≈5 box-hours). Compare pairwise ranking accuracy of C with the mean of 8 random rollouts, of 64 rollouts, and the closed sv_55.
- *Go:* C beats 8 rollouts by ≥ 5 points of pairwise accuracy, overall and on boss fights. *Stop:* otherwise; the line stays closed and P2 is cancelled.
- *If go, days 10–14:* fight suite at equal time with search stage 2; continue iff A20 boss deaths fall ≥ 10% relative (5,000 paired fights resolve ≈2 points [estimate]).

**E3 — does the strategic improvement operator improve? (days 5–14).**
- *Build:* extend `branchEval2` with truncated continuations (¼ budget, to the end of the act, V bootstrap, independent RNG streams; cf_pick's streams coincided with the real fight, h5 §4.8).
- *Data:* 1,500 decisions each of card reward, shop, rest and map, at A10 and A20; 64 continuations per option in two halves ≈ 20 box-hours [estimate].
- *Test:* F33's split-half honest headroom H. F33 found H = −0.020 / −0.009 at cards and +0.014 at map nodes with 8 full-cost playouts per half, the halves agreeing on the best card in 35% of decisions (h2 §1).
- *Go:* H ≥ +2 SE and ≥ +0.01 on at least two surfaces. *Stop:* H ≤ 0 everywhere; search-as-teacher is then dead at the strategic level and P3 starts with its fallback (policy gradient with search in the loop, untested in the history).

If E2 and E3 both stop and E1 is flat, the plan ends after two weeks and about $400.

## 5. Expected outcome

- **T1, A20 act-3 ≥ 25%: 30%.** Contributions [estimate]: strategic loop +4 to +10 points (whole-run RL reaches 21.9% without search, s6; +7.5 to +15.5 per Mentor campaign; discounted because engines differ and the fidelity fork alone halved this project's A20 rate, h4 §1); combat half +1.5 to +3 (one to two effective doublings, d1 §3); currency +0.5 to +2. Median outcome 15–18%.
- **T2, A20 Heart ≥ 20%: 8%.** It needs roughly 40% keyed act-3 clears and 50% Heart conversion (the arithmetic of d1 §7). It rests on P5, where the objective-matched signal starts from 0.3% and conversion has never responded to search. The best published fair specialised figure is 8.0%, for Watcher (s6 §3).
- **T3, A20 Heart ≥ 45%: 2%.** Only a clairvoyant hybrid is near it (50.1% with true draw order, s6 §1a); experts convert ≈90% of act-3 clears (w1 §3), a level of deck building no gate here tests.

Most likely failure: E2 stops, or C passes offline and ties at equal time (the pattern of three outside projects); the strategic loop yields +3 to +6 points at A20 and then stalls, because a +1-point generation needs ≥ 4,000 paired seeds to be seen (paired SE 0.012 score per 1,000, 00 §4); the plan ends at 11–14% as a costlier strategy-only plan. Second: integration of two networks, a state bank and three target types takes twice the estimate.

## 6. Conflicts and owner decisions

| Closed or rejected | What this plan does differently |
|---|---|
| Learned leaf value and the mixed leaf (closed by the owner) | **P2 needs the owner to reopen the line.** Target: realised outcome of search-guided play in run currency, not the mean of 8 random rollouts. Representation: sparse entities with card identity by pile, not 204 aggregates. Role: turn-boundary value mixed with a rollout (two complementary estimates; F52 mixed two estimates of the same quantity). The log itself calls a new representation "a new line, not a continuation" (h1 §4.1). The reopening is bounded by E2's offline gate |
| Prior below the root (−0.010, and −0.009 even at +40% compute); sharper prior targets (−0.034) | Entity inputs; completed-Q targets with pruning instead of raw 2,000-simulation visit counts; deep nodes are no longer scored by one random playout. It is its own paired stage and is dropped if it fails again |
| V iterations F4, F17, F20 | They re-fitted an outcome regression on newer games with no improvement operator; here the targets come from search |
| Models replacing rules (−0.07 to −0.62) | Those were observational level models used as an argmax; R starts as a clone of the bests and changes only through interventional targets and paired promotion |
| Online counterfactual fights (cf_pick) | Offline, whole continuations, common seeds, used as training targets |
| Fight-outcome surrogate in strategy training (Mentor's failure, s4 §1) | Not used: training fights are played by the real search |

**Rule changes requested.** (1) Equal compute by calibrated wall-clock instead of search actions. (2) The U-table counted as amortised over the fight's decisions, with simulations lowered to match. (3) Gates on win rate, since the score hides wins (r1 §5). (4) A new A20 Heart track (P5). The per-decision budget itself is not changed.

**Hardware and money.** The fleet for ≈27 fleet-days (≈$9,600); bulk storage outside git for the state bank and replay data; optionally one rented consumer GPU for 2–4 weeks ($115–600) from P4.

**Before the start the owner decides:** reopen the evaluator line for E2 and P2, or run the plan without its combat half; accept rules 1–3; accept three to five weeks without a promoted best while P2 and P3 run; say whether T1 or T2 has priority after P4.

## 7. What this plan deliberately does not do, and what from the other possible directions it would adopt later

Not done: tuning the remaining constants; simulator speed work; boss-fight budget reallocation; imitation of human run logs; a search-free policy; learned dynamics; hand-coded archetype plans; draw bucketing and whole-turn transposition tables.

Adopted later:

- From a combat-engine-first plan: bucketed draw chance nodes and in-turn transpositions once C supplies a turn-boundary value; a lookup-table playout policy fitted by simulation balancing if the mixed leaf keeps its rollout (w2 B2); SPSA over surviving constants.
- From a strategy-first plan: PPO with search in the loop (already P3's fallback); the expert behaviour table (w1 §2.2) as an audit of every promoted generation; counterfactual replay attribution (w1 §1.2).
- From a diagnostics-first plan: replicate runs per seed; sequential tests for cheaper gates.
- Coarse deck-plan conditioning (s2 §4) if the loop's decks stop diversifying.
