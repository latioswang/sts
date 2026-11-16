# pB — Plan B: combat engine first

Stage 2, architect B, 2026-11-16. Nothing was built or run. Sources are the stage-1 files in this directory, cited by name; `00_internal_map.md` and `s6_sts_sota_scoreboard.md` did not exist when this was written. [estimate] = my number; [inferred] = my reading. One simulator fact was read in source: the enemy turn ends in `BattleContext::afterMonsterTurns()` (sts_lightspeed `src/combat/BattleContext.cpp:1021, 3684`), the place for a pre-draw stop.

## 1. Thesis

The missing strength is in the combat engine: every fight is judged by a structurally biased estimator (a tree that ends at the turn boundary, one near-random playout to the end of the fight, a loss value that cannot see the player's HP, a utility of 35 + HP that the run does not share), so the agent leaks HP in every fight and cannot pilot the decks that win at Ascension 20. The mechanism is a rebuilt engine — draw nodes, a microsecond turn-boundary evaluator trained on realised outcomes of strong play, paired root comparison, utility in run-win-probability units — improved by expert iteration on fights, bosses first, after which the strategy layer is retrained on the new pilot's outcomes. Now, because stage 1 named the defects and the reasons the earlier leaf value failed, the fleet produces in a day the fight data those attempts lacked by three orders of magnitude, and one first-week experiment can falsify the thesis for about 350 CPU-hours.

Evidence:

1. **The tree ends at the turn boundary.** The node key holds the ordered hand and every simulation reshuffles, so statistics never build past the current turn (`c1` §6.1); reuse subtrees "mostly reset" (`h1` §3). Widening over effectively unique chance outcomes converges to a policy "suboptimal regardless of the computation time" (`w2` A2).
2. **Random playouts cannot win boss fights.** The only gradient there is 10 × damage fraction + 0.2 × turns, without player HP; three constant-level repairs lost and "the premise stands; no fix found" (`h7` §3; `c1` §6.2).
3. **The engine's limit is bias, not variance.** The F52 mix cut leaf variance and added nothing per simulation (−0.0215 against a −0.0183 control; `h1` §1, §6.2). More samples of a biased estimator flatten — the A20 curve (6.0 / 8.5 / 9.3%) and the Heart fight (49 → 47% lost at 2×; `d1` §3).
4. **Estimator quality has paid whenever it was cheap.** The one data-tuned playout parameter (END_TURN weight) gave +0.1866 and +0.0682, the second-largest gain in the ledger (`r1` §2, §6.1); the root prior is worth +17 points at 50 simulations (`h7` §2), the visit regime of every node below the root.
5. **The earlier leaf value failed for named reasons.** Target = mean of 8 random rollouts, 204 aggregate inputs, error floor 11.8 unchanged at 10× data, about 45% of a simulation per evaluation (`h1` §1–3; `c1` §3–4). No system in the AlphaGo lineage trained a value on weak-policy rollouts (`s1` §2a). The log itself says a different representation "would be a new line, not a continuation" (`h1` §4.1).
6. **The target design exists at this compute scale.** 2048: an afterstate n-tuple value with expectimax at 1 ms per move on one thread; NNUE: +92.77 Elo; an NNUE-shaped evaluation measured at 1.5 µs on a project-class core (`w2` B4; `w3` §1; `s5` §2). Bucketed chance nodes in Hearthstone: 60.1 → 72.3% (`w2` A2). Spire Mentor's deployed combat is chance-node search with a feature evaluation at the leaves and no rollouts; spire-engine's one-turn look-ahead on redrawn copies adds 5.6 points at A20 (`s4` §1).
7. **Combat and strategy share no currency.** HP is worth the same before a rest site as before a boss; "fight value = V(state after the fight)" was planned and never started (`c2` §6.1; `h7` §4.1); 40% of A20 hallway deaths hold an unused potion (`d1` §4).
8. **The "deck-strength gap" is not independent of the pilot.** Hallway fights cost 11–16% of max HP and elites 25%; attrition turns rest sites into heals (`d1` §4, §7). "Weakly played fights reward cards that help weak play" (`h6` §3); each agent's run policy suits its own combat player (`w1` §1.2).

**The fact against this plan.** At A20 the second doubling of search adds 0.8 points and the Heart fight does not respond (`d1` §3); two audits found "a deck-strength gap rather than a single bad decision" (`h1` §3); in an outside study 58% of final-boss losses were not winnable by replaying the fight (`w1` §1.2); Mentor found "combat ceiling at high ascension" wrong for one character (`w1` §1.1). My reading [inferred]: doubling simulations multiplies the same estimator, so a flat curve cannot separate "deck-limited" from "bias-limited"; the Heart is the fight a random playout is least able to win, so nearly every simulation there returns the HP-blind loss value; and an audit cannot see a leak of a few HP per fight. The data do not decide this. Phase 0 does, and stops the plan if the deck reading wins.

## 2. Target design

**State encoding.** Sparse binary entity features, emitted from the observation only (draw pile as a multiset, plus the known top cards):
- card id × upgraded × pile (draw, discard, exhaust, retained hand) × copy count (≥1…≥4);
- relics; potions; player powers × magnitude bucket;
- per monster: id × current move, id × HP bucket, powers × bucket;
- encounter, act, ascension, turn bucket;
- factorised card features (cost, type, knowledge-base tags) so rare cards share statistics.

About 12,000 features, 50–80 active [estimate]. A dozen dense scalars (HP, max HP, block carried, enemy HP fractions, next-turn energy) feed the tail directly, because sibling lines differ by a few HP.

**Model: one evaluator, NNUE class.**
- Sparse first layer of width 256 (int16; about 3M weights, 30 KB touched per evaluation) → 32 → 32 → heads.
- The binding quantity is operations per evaluation, about 25k [estimate], inside the "1,000 evaluations" regime of the rules; the parameter count sits in a lookup table.
- Value heads: P(fight won); distribution of end-of-fight HP in 16 bins given a win; potions used.
- Auxiliary heads (training only): HP lost over the next 1 and 3 turns, turns to the end (`s1` §4; `s5` §4.6).
- Policy head: logit(a) = action embedding · 32-d tail + target features, about 30 ns per action [estimate].
- Where: the value at the *pre-draw afterstate* (enemy turn resolved, next hand not drawn), i.e. an expectation over the draw; the policy at every expanded node.

**Training target.** The realised result of the fight as played by the strongest available agent, blended TD(λ ≈ 0.8) with that agent's later-turn search values; at most three states per fight; plus a ranking loss on sibling afterstates of one decision, labelled by continuations on common random numbers. Policy target: softmax of completed Q (`w2` A3).

**Utility.** At fight start, a table U[HP bin, potions] = run-win probability of the post-fight state, from a run-value model fitted on *interventional* labels (post-fight states with HP and potions set by hand, run played on). Leaf value = P(win) · Σ p(bin) · U; death = 0 plus a small damage tie-break. All values lie in [0, 1], which removes the min-max squeeze (`c1` §6.4).

**Search at play time.**
1. Root: sequential halving over at most 8 candidates ordered by the prior; within a round every candidate sees the same 16–64 sampled hidden states.
2. Within the turn: PUCT with head priors at every node; nodes keyed by a canonical observation (sorted hand, statuses included), so order permutations merge; node-mean bug fixed (`c1` §1).
3. END_TURN: simulator to the pre-draw afterstate, one evaluation.
4. Draw nodes with progressive widening open the next turn under afterstates that collect visits: a two-turn expectimax where it matters.
5. Optional: a one-turn playout by a per-card weight table, then the evaluator, mixed with weight λ.

**Play-time cost** [estimate, from 6 µs and 15 actions per simulation, `c1` §4, and 1.5 µs, `s5` §2]: in-turn descent 1.2 µs + expansion with priors 0.5 + enemy turn 0.8 + evaluation 1.5 ≈ 4 µs; two-turn simulations ≈ 7 µs. At 30% two-turn, about 700 simulations in 3.5 ms (today about 400–520 [inferred from `c1` §2, §4]), each ending in a learned expectation instead of twelve random actions. No budget change is requested.

**Training loop.**
- Round 0: the current agent at 4× simulations plays a bank of fight-entry states. Later rounds: the new engine at 4×, full budget on 25% of decisions (`s5` §4.5), old states reanalysed, the bank refreshed from the newest agent's runs and weighted to where runs die.
- Engine constants by SPSA on the bank with paired seeds (`w3` §4).
- Then the strategy layer is regenerated on the new pilot (Phase 4).

**Hidden information, training only.**
- Sibling labels on shared future seeds.
- A privileged critic that sees the next hands and enemy rolls, used as a control variate for the outcome target; unbiased because the action is independent of the future given the observation (`s3` §4).
- Clairvoyant search as a diagnostic bound, never as a policy teacher (hindsight play is not information-set play, `s3` §2.7).
- The leak test is extended to the feature emitter: re-randomised hidden state must give identical features and move.

**Reused:** simulator, action enumeration, `determinise`, harness, fresh-seed rule, the whole strategy layer until Phase 4, the run-value model, `branchEval`, the data store. **Replaced:** node structure, playout to the end, `terminalValue`, root PUCT with most-visited choice, root-only MLP prior, the 204-feature extractor.

**Why this is not what failed.**

| Tried and lost | What differs here |
|---|---|
| Outcome regression, 9.1k battles, one label per battle → passivity (`h6` §1, §3; `s5` §4.2) | ≥ 2M fights, ≤ 3 states per fight, TD targets, ranking loss; used only at turn boundaries, so in-turn progress is decided by exact search |
| Phase S / S2 / F52: mean of 8 random rollouts, 204 aggregates (`h1` §1) | Target = realised strong-play outcome; entity inputs; capacity not paid in time. Cost per evaluation is about the same; the gain is what it replaces (87% of a simulation, `c1` §4) |
| F52 mix: two estimates of one quantity (`s1` §4) | A strong-play value mixed with a table playout: complementary, as in AlphaGo |
| Prior-guided rollouts, 2.43× cost, −0.062; hand-set rollout biases, median −0.065 (`h5` §1; `r1` §3) | A lookup table at zero added cost, fitted for balance (value error against deep search, `w2` B2), not by hand or for move imitation |
| Cut-off + heuristic leaf, −0.17 to −0.37; `brt8` (`h6` §1; `h7` §1) | The cut-off value is learned from strong play, not `35·(1 − ratio) + HP` |
| Depth-1 MLP prior −0.01; depth 2 nothing; tree reuse a tie (`h6` §1; `h5` §1; `h1` §1) | Lookup-cost head, completed-Q targets, in a tree that has depth; reuse is retested only after draw nodes exist |

## 3. Roadmap

USD: $0.05 per CPU-hour at raw cloud price (`s5` §3) to $0.38 at the project's observed worker cost ("10 worker sessions ≈ 2 h, ≈ $30", `h1` §2). Compute and days are [estimate]. Every play gate is at equal measured CPU time per decision (±1%) on fresh paired seeds.

| Phase | Deliverable | Days | Compute | Gate | Kill |
|---|---|---|---|---|---|
| **P0** Bank and headroom | 15k frozen fight-entry states (A20 bosses, elites, hallways; Heart) with a fight-level paired harness; search curve 1–64×; clairvoyant bound; nested reference pilot | 5 | 350 CPU-h ($20–130) | Best reference pilot cuts A20 boss-class deaths ≥ 25% relative, or HP lost in won fights ≥ 20% (8k states × 2 replicates) | < 15% and < 10%: stop the plan; hand bank and results to the strategy plans |
| **P1** Skeleton, no learned value | (a) paired root with sequential halving; (b) canonical keys, draw nodes, node-mean fix; (c) utility table U | 9 | 600 CPU-h ($30–225) | Each item alone: A10 and A20, 2,000 fresh paired seeds, dscore > 0 with P ≥ 0.9, then a 2,000-seed confirmation | None keeps and the owner declines P2: stop here with the playout-table fallback |
| **P2** Evaluator, rounds 0–1, bosses and elites (needs reopening) | Feature emitter, trainer, integer inference, engine above | 12 | 1,200 CPU-h ($60–450); optional 20 GPU-h (≤ $20) | Offline: sibling regret below 8-rollout regret. Bank: A20 boss deaths −15% relative, P ≥ 0.95. Runs: A20 win +2.0 points on 2,000 fresh paired seeds (the old 2× gain at 1× cost); Heart-fight loss ≤ 44% | After round 1 and three pre-registered variants (afterstate only, λ-mix, two-turn) bank boss deaths not below baseline: line closed for good, plan stops |
| **P3** Expert iteration, all fights, rounds 2–5 | Priors at all nodes, balanced playout table, SPSA constants | 14 | 4,000 CPU-h ($200–1,500) | 4,000 fresh seeds at 1×: A20 ≥ 14% (above the old engine at 4×, 9.3%), A10 ≥ 62%, Heart ≥ 38%; each round ≥ +1.5 points of bank boss wins until plateau | A20 < 11% after five rounds: stop combat work; P4 proceeds only as another plan's programme |
| **P4** Strategy relearn on the new pilot | V, path and deck models regenerated; decision headroom re-measured (F33/F34 protocol); expert-profile audit; then strategy training with the search in the loop (adopted) | 15 | 4,000 CPU-h ($200–1,500); optional 50 GPU-h ($15–45) | T1: A20 ≥ 25% on 4,000 fresh seeds, then one test-split run | A20 < 18% after two strategy generations |
| **P5** A20 Heart | Act-4 bank with A20 decks, Heart-objective utility, key policy | 10 | 2,000 CPU-h ($100–750) | T2: A20 Heart ≥ 20% on 4,000 fresh seeds, then test | < 8% at the end of the phase budget |

Total: about 65 engineering days, 12,000 CPU-hours (12.5 fleet-days of ten boxes; $0.6k–4.6k), GPU optional (≤ 70 h, ≤ $65).

## 4. First two weeks

**E1 — pilot headroom (days 1–5; touches no closed line).**
- Build: snapshot of the battle state and run context at fight entry; a fight-only runner with agent seeds independent of the run seed (today they are tied, `d1` §6.6); switches for `determinise` off, simulations 1–64×, and a nested pilot (each playout step chosen by a 32-simulation search, 200 outer simulations).
- Data: entry states from 6,000 A20 and 3,000 Heart-track runs on fresh train-split seeds (3 CPU-h): 8k boss, 4k elite and hallway, 3k Heart and Shield-and-Spear.
- Test: death rate and HP lost per class, two hidden-seed replicates per state, paired.
- Reading:
  - Go if a reference pilot meets the P0 gate.
  - Clairvoyant minus fair at equal simulations prices hidden information: above 10 points on bosses [my threshold], draw nodes come first; below 5, the evaluator does (`s3` §5 E0).
  - A flat 16–64× curve with a much better nested pilot confirms "bias, not variance".
  - Stop if no pilot at up to about 1,000× compute meets the kill line. The decks are then the limit and this plan is wrong.
  - Heart bank: if no reference gains 8 points, the Heart leaves this plan's scope until P5.

**E2 — label and representation (days 4–11; needs the owner's reopening).**
- Build: feature emitter at turn boundaries; logger; PyTorch embedding-bag trainer (CPU).
- Data: 300k boss and elite fights at 4× from entry states disjoint from E1 (75 CPU-h). Test set: 4,000 decisions × up to 4 sibling afterstates × 32 continuations on shared seeds at 4× (70 CPU-h).
- Test 1, offline (`s1` §5): regret of the net's sibling choice against (a) the mean of 8 random rollouts, (b) the mean of 64, (c) the old `sv_55`. Go: below (a) overall and not above (b) on boss classes. Stop: above (a); the evaluator stays closed and the plan shrinks to P1 plus playout tables.
- Test 2, slow play: the float model at END_TURN afterstates in the present tree, on the bank at equal *simulations*, then at equal CPU time. Go: boss-class death rate not above baseline at equal simulations. This is the per-simulation test the F52 mix failed.

**E3 — paired root (days 8–12; no closed line).** Shared hidden-state samples across root candidates with sequential halving in the existing searcher; bank plus 2,000 fresh paired seeds on A10 and A20; standard keep rule.

Decision at day 12: E1 go and E2 go → P1 and P2 in full. E1 go, E2 stop → P1 and playout tables only; expected value drops to a few points. E1 stop → the plan ends.

## 5. Expected outcome

Arithmetic first [mine, from `d1` §2]: 7.6% is the product of seven stage survivals; 25% needs every stage's death rate cut by about 40% relative (0.957 · 0.906 · 0.793 · 0.762 · 0.816 · 0.819 · 0.704 = 24.6%). One doubling of search cuts A20 stage death rates by 5–13% (`d1` §3). The engine must therefore be worth far more than any search multiplier measured so far, and the retrained strategy must supply the rest.

- **T1 (A20 Act-3 win ≥ 25%): 28%** [all factors are estimates]. P(E1 passes) ≈ 0.55: the evidence is mixed. P(evaluator wins at equal time | headroom) ≈ 0.45: `s1` §5 gives 35–45%, and three outside projects lost with learned leaves and priors (`w1` dead ends), but none changed target, representation and tree at once. P(gain plus strategy relearn reaches 25% | it works) ≈ 0.6. Product 0.15, plus about 0.13 for the branch where a moderate engine gain is completed by the adopted strategy training of P4.
- **T2 (A20 Heart ≥ 20%): 7%.** Needs roughly 40% keyed act-3 clears and 50% Heart conversion, from 3.65% and 14% (`d1` §2). Reachable only if the Heart bank responds strongly to the new evaluator and the strategy relearn builds Heart decks.
- **T3 (A20 Heart ≥ 45%): 1.5%.** Expert level needs expert deck building; this plan does not claim it.

**Most likely failure:** the evaluator ranks sibling afterstates about as well as 1,000 random playouts at equal time (first-generation value nets lost to rollouts in AlphaGo's own table, `s1` §2b). Ordinary fights tie, bosses gain a little, A20 lands at 10–12%: a 2–4× search multiplier, not a new regime. What remains is the fight bank, the paired root, the run-value utility and a measured answer to "deck or pilot".

## 6. Conflicts and owner decisions

1. **Reopen the learned evaluator (closed).** It differs in target (realised strong-play outcomes with TD and ranking losses, not the mean of 8 random rollouts) and in representation (entity features, not 204 aggregates), and also in placement (pre-draw afterstate) and sampling (≤ 3 states per fight, ≥ 2M fights). Not a continuation: the closing entry itself calls another representation "a new line" (`h1` §4.1). Without this decision the plan is P0, P1 and playout tables.
2. **Expert iteration on boss fights ("Direction A").** Withdrawn without a run because root-prior distillation failed three times (`h1` §1). Here the distilled object is a turn-boundary value and an all-node prior in a restructured tree, not a sharper root prior.
3. **Other touched lines:** rollout cut-off, value-shape constants, depth-1 prior, tree reuse, rollout-policy bias. The table in §2 states the difference for each.
4. **Compute accounting.** "Equal mean search actions per game" stops being meaningful when a simulation spends time in an evaluator. Requested rule: equal measured CPU time per decision on an idle machine, with a cost-matched control arm. The play-time budget itself is unchanged; the evaluator's 3M table weights cost about 25k operations per evaluation, so the owner should confirm that CPU time, not parameter count, is the rule.
5. **Simulator fork changes:** a pre-draw stop and a state snapshot, under bit-identity gates and the real-game regression.
6. **Primary metric.** The score hides wins (`r1` §5). Requested: win rate as the primary metric for A20 and Heart gates, score as a guard, the fight bank as the development metric.
7. **Money and workers:** about 12,000 CPU-hours over 13 weeks; multi-hour worker phases need standing approval (`h1` §5). GPU optional, under $100.
8. **Test split:** two uses, after P3 and P4.

## 7. What this plan deliberately does not do, and what it would adopt later

- No whole-run reinforcement learning, transformer policy or plan state before P4.
- No network above about 25k operations inside the tree, no search-free combat policy, no learned world model, no language model.
- No retuning of hand rules, Neow, events or potions-as-inventory except through the utility table.
- No per-fight budget reallocation and no change to the play-time budget.
- No second character.

Adopted later from other directions:
- Strategy training with the real search in the loop (Spire Mentor, `w1` §1.1), in P4. If the new engine at a fraction of the budget equals today's agent, that training becomes proportionally cheaper.
- The expert-profile audit (elites, rest rate, skip rate; `w1` §2.2) and counterfactual replay attribution (`w1` §1.2), in P0 and P4.
- Failure-weighted restarts (`s4` §5), through the fight bank.
- The evaluator's fight-outcome heads as dense strategy targets (`c2` §6.2), but only after a closed-loop survival test, since a fight surrogate failed that test in Mentor (`s4` §1).
- If E1 stops this plan, its bank, harness and deck-or-pilot measurement are the first inputs of whichever strategy plan the owner chooses.
