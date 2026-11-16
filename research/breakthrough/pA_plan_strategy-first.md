# pA — Plan A, strategy first: learn the whole out-of-combat policy from complete runs

Stage 2, 2026-11-16. Nothing was built or run. Numbers carry their source file; **[estimate]** = my arithmetic or judgment, **[inferred]** = my reading. Outside figures are indicative (the verified scoreboard did not exist yet).

## 1. Thesis

The missing strength is in the ≈100 out-of-combat decisions of a run (c2 §6.4): each is a separate one-step argmax of an observational value with one noisy label per run, there is no plan and no price for HP or potions, and the score being optimised is at Ascension 20 about nine-tenths floors. The mechanism is one policy over every out-of-combat decision, started as an exact copy of the current agent and improved by policy-gradient reinforcement learning on complete runs in which the unchanged 1,000-simulation search plays every fight, with a win-aligned return, a training-only privileged critic, restarts from stored failure states and plan-conditioned exploration. Now, because the fleet produces 1–2.5M search-played runs a day (s5), seven to sixty-five Spire Mentor training legs [estimate], and two outside systems show that this recipe, not better one-step values, moves high ascensions.

Evidence:

1. **Same game, no search, three times our A20 rate.** spire-engine reports Ironclad 93.1 / 75.2 / 21.9% at A0 / A10 / A20 (27.5% with a shallow look-ahead) from whole-run PPO (s4 §1); this agent has 85.4 / 52.5 / 7.6% (d1 §1). Its fights are played by a few-millisecond policy, so the gap is unlikely to be tactics [inferred]. Own engine, self-reported.
2. **The closest analogue made this switch.** Spire Mentor replaced oracle-labelled pick ranking by PPO with its fair search in the loop; legs of 38,400–144,000 runs gave +7.5 to +15.5 points (s4 §2, §6). Its belief in a combat ceiling at high ascension was measured wrong: +18.5 and +21.0 points from one run-policy leg (w1 §1.1).
3. **Where our runs end.** 39.6% of A20 entrants die at the act-2 boss, 25% even from 85%+ HP; a search doubling moves boss death rates 2–4 points; A20 goes 6.0 / 8.5 / 9.3% over 1× / 2× / 4×; the Heart fight stays at 49 → 47% lost (d1 §2–4). The 11 traced winners hold 10.2 vs 8.4 relics, 3.7 vs 2.35 removals, 705 vs 325 shop gold (d1 §4). In the zero-search study 72% of final-boss losses could not be won by re-deciding all of Act 3 (w1 §1.2).
4. **Weak tactics plus strong strategy beats us on the real target.** Language models without search kill the A20 Heart in 18–33% of runs (w1 §2.1); this agent in 0.3% (d1 §1). Experts convert about 90% of Act-3 clears into Heart kills (w1 §3); this agent wins 50% of its Heart fights at A0, 14% at A20 (d1 §2).
5. **In-house, strategy models carried the gains and objective matching worked.** Learned models are 50.2% of all kept gain, 70% at A20; a value trained on the Heart outcome moved Heart wins from about 16.5% to 25.3% (r1 §2, §6.3). No model was ever trained or selected on A20 wins or the A20 Heart.
6. **The code.** No plan state; removal unconditional; Neow random; potions banned from hallway search while 7.4% of A20 games die in a hallway holding one; the value sees a bag of counts with one label per run, RMSE 0.455 on a spread of 0.715 (c2 §1–3, §6; d1 §4). The hand rules (rest below 50–70% HP; elites only above 60% HP in the map DP) are not the expert profile: 9% of campfires rested, 2.6 Act-1 elites (w1 §2.2).
7. **The one decision with a decision-specific learned correction paid more in play than offline:** paths, +0.0133 per decision became +0.030 per run (h2, F35).

**Do the "no headroom" studies bind this plan?** (h2 F33–F34, h3 F26–F32.)
- *What they measured:* the advantage of an 8-playout oracle over the agent's pick, one decision changed, the same policy afterwards, scored on floor/51 + win. H has a standard error of 0.004–0.008 per decision; the halves agree on the best card in 35% of decisions (h2 §1). A new policy's gain is the sum of one-step advantages along its ≈100 decisions, so an undetectable +0.003 to +0.01 per decision leaves room for +0.3 to +1.0 score [estimate]. The source agrees: "too noisy to rank A20 card options", "not a strict ceiling" (h2 §3).
- *Never measured:* shops, removal, upgrade target, events, potions, Neow, keys, and every surface on the A0 and Heart tracks (h2 §4.13–14).
- *Objective:* the A20 score is about 0.58 from floors and 0.05 from wins; the kept decision-trained value "helps the agent go further, not win more" (h3 §3, §6.1).
- *Where they do bind:* single-surface, small-data corrections are exhausted (19,531 decisions bought +0.0019 per decision, h3 F28). Effects per decision are small, so only 10^6 runs crediting all surfaces at once can resolve them, and a card-reward-only pilot (s4 §6) would most likely return nothing; this plan does not start there.
- *The strongest counter-evidence* is h4 §6.4: twelve phases of value work moved A20 wins by 1–2 points. My reading: the form saturated (value differences, floor score, exploration only at card and relic picks), not the layer [inferred]. P1 tests that in two weeks.

## 2. Target design

**One policy π(a | s, g, z)** for every out-of-combat screen: Neow, card reward and skip, shop (one purchase per call, including leave, removal, potions), rest site (all six actions), upgrade / remove / transform targets, events, map node, boss relic, chests and keys, potion pick-up and discard. One decision is added before every fight: a *posture* among 4–6 presets of existing search parameters (`hall_potions`, `potion_v`, the low-HP terms; per-fight overrides exist, c1 §5), never the simulation cap. g is the goal (Act-3 win or Heart), z a deck plan.

**Imitation without cloning error.** logit(a) = β·ℓ(s,a) + f_θ(s,a,g,z). ℓ is the score the current hook already computes for each option (knowledge-base value plus value bonus, path-model output, V(heal) against V(upgrade)), or a one-hot of the rule's choice where only a rule exists; the nearest track's parameter file supplies it. f_θ has a zero-initialised output layer, so iteration 0 plays the current best move for move. In P2 a stand-alone network is cloned from β·ℓ + f_θ on policy-visited states so the legacy hooks can be retired.

**State encoding** (P2 onward; P1 reuses the 666 run features and 20 path features). Public information only.
- Deck: a learned embedding per card id (d = 48) with upgrade flag and per-card counters, sum- and mean-pooled; relics likewise; potions by identity.
- Scalars: HP, max HP, gold, floor, act, ascension, keys, act boss.
- Map: a learned backward pass over the act's graph, one vector per legal next node.
- Option token: type, embedding of the entity it points to, price.
- Head: MLP(trunk ⊕ option), masked softmax over legal options (s4 §5); 0.3–1M parameters. No attention over cards: no published ablation shows it beats pooling for a known pool (s4 §3).

**Critics.** V_pub(s,g) has heads for P(goal reached), P(clear act k), floors remaining and HP share at the next boss, trained on realised outcomes of the acting policy, γ = 1. V_priv(s,g,h) adds hidden inputs h: the act's upcoming encounter and elite lists, the A20 second boss, the order of the relic pools and boss-relic offers [inferred: seed-determined simulator state]. V_priv is only the policy-gradient baseline (AlphaStar 22% → 82% with a privileged value, s2 §2; PerfectDou 0.486 → 0.524, s3 §2) and is never exported to the C++ agent.

**Hidden information, training only.** (1) V_priv. (2) Restarts: a stored state is a seed plus an action prefix (the harness is deterministic, h6 §5); after the restart all hidden streams are re-seeded, as `branchEval` does (c2 §4), and K = 4 continuations give a low-noise value label and within-state advantage estimates. The bank holds act starts, elite and boss starts and rest sites, is refreshed every iteration, and is sampled by p(1−p) of V_pub and by where fresh runs die (+3.6 and +0.4 points for two characters in the zero-search paper, s4 §5); at least half of all episodes stay fresh. (3) Hindsight relabelling of z. The actor never sees h. Every candidate passes an out-of-combat leak test: re-seed the hidden streams, require the same argmax (the test that caught the relic-pool leak, h4 §1).

**Return.** 3·goal + 0.1 per act cleared + 0.02 per floor (spire-engine's weights, s4 §1); shaping terms sit on their own value heads and are annealed to zero. Reason: +6 points of Heart wins moved the present score by +0.006 (r1 §5).

**Loop.** PPO, GAE λ 0.95 → 1, clip 0.1, a KL anchor to the iteration-0 policy and later to the previous snapshot (the anchor was worth +380 Elo in AlphaStar, s2 §2), entropy kept low after the copy (a copied entropy schedule cost Mentor 13 points, s4 §6). One iteration is 25,000 runs, about 3M decisions; a campaign is 40 iterations, 1M runs. Fights use the honest search at the full play budget: a fight surrogate predicted survival of 0.64–0.68 against a true 0.29 (s4 §1), and a cheaper pilot would teach the wrong price of risk [inferred]. Ascension is sampled uniformly A0–A20 and is an input (up-weighting hard levels cost Mentor 2 points, w1 §1.1), ablated once against A20-only.

**Exploration over deck plans.** z ∈ {none, 12–16 plan tokens}: clusters of winning final decks in the 520k-run store, seeded with the nine Ironclad archetypes of the knowledge base (`/home/user/sts/ironclad_archetypes.md`). The policy is conditioned on z, dropped 25% of the time; a small pseudo-reward on its own head pays for offered picks that move toward the plan; a bandit per (act boss, Neow) context samples the best and the least-tried plans (z was worth +140 Elo in AlphaStar; plan win rates spread 35–65% in SCC, s2 §2–3). At play time z is "none" or the argmax of a plan head on public state. This is the plan state the code lacks (c2 §3); the only exploration ever run here, ε = 0.2 on cards and boss relics, was a keep (h5 X1).

**Play-time cost.** Combat is unchanged: about 1,000 simulations, 3–4 ms. Strategy: one trunk pass and at most 30 option heads; at 1M parameters at most 0.2 ms + 30 × 19 µs ≈ 0.8 ms per decision (s5 §2: 196–222 µs at batch 1, 19 µs batched), 0.3 ms typical [estimate]. With ≈100 strategic decisions and 16–26 postures that is ≈ 40 ms per run [estimate], 2–5% of a 0.8–1.8 s game, removed by the existing wall-clock calibration. No fight is simulated online.

**Reused.** The simulator; FairSearch untouched; every FairAgent hook (enumerate options, legacy score, add f_θ, argmax); run features; `branchEval` forking; the harness and fresh-slice protocol; the data store. **Replaced over time:** the four per-track parameter files by one policy with ascension and goal inputs; knowledge-base thresholds and the event table as decision makers (they remain as prior logits); the `hall_potions` ban; random Neow; the fixed shop order.

## 3. Roadmap

One fleet-day is 960 core-hours: about $43 at list price for 4-vCPU boxes (s5 §3), about $360 at the rate this project recorded ("10 worker sessions ≈ 2 h, ≈ $30", h1 §2) [computed]. A run costs 0.8–1.8 core-seconds (d1 §4). All gates are pre-registered, on fresh paired seeds, with compute ratio ≤ 1.01 and a clean leak test. "Confirmed" = a second fresh 2,000-seed slice, pooled P ≥ 0.95.

| Phase | Deliverable | Eng. days | Compute | Gate | Kill |
|---|---|---|---|---|---|
| **P0**, week 1 | Trace logger, residual hook, restart tool, leak test; three diagnostics (§4) | 5 | 150 core-h, $7–56 | None; readings set P1's priors | All three diagnostics null |
| **P1**, week 2 | Mini-campaign on five surfaces at A20, legacy features | 5 | 2M runs ≈ 700 core-h, $31–260 | A20 wins ≥ +1.5 points over the best, 2,000 seeds, confirmed | Pooled gain < +0.5 points after 2M runs and a flat training return |
| **P2**, weeks 3–5 | Full policy: all surfaces, entity encoder, V_priv, restarts, uniform ascension, stand-alone clone | 12 | 10M runs ≈ 3,600 core-h, $160–1,350; learner on one box (36.6k rows/s at 1M parameters, s5 §2) | One policy: A20 ≥ 15%, A10 ≥ 62%, A0 ≥ 85%, 2,000 seeds each, confirmed | A20 < 11% |
| **P3**, weeks 6–8 | Plan conditioning with bandit; single-boss specialists distilled back; optional run-aware fight utility (§6) | 10 | 15M runs ≈ 5,800 core-h, $260–2,200; optional GPU, two weeks, $115–300 | **T1:** A20 ≥ 25% at 1× on the milestone test split | A20 < 18%: freeze the policy, hand the fleet to pilot work |
| **P4**, weeks 9–12 | Heart goal: key and act-4 decisions, restarts from act-3 and act-4 states, Heart return from A0 upward | 10 | 15M runs ≈ 7,500 core-h, $330–2,800 | A20 Heart ≥ 8% and A0 Heart fights won ≥ 60% (50% now, d1 §2) | A20 Heart < 3%, or A0 Heart fights won ≤ 52% |

Total: 42 engineering days, about 17,800 core-hours (18.5 fleet-days), $800–6,700; a GPU is optional.

## 4. First two weeks

**Build (days 1–4).**
- `STRAT_TRACE`: at every out-of-combat hook, log the entity-level state, the legal options, ℓ for each, and the choice.
- Residual hook: runtime weights, sampling in training, argmax in evaluation; with zero weights the repro gates must be bit-identical.
- Restart tool (replay seed and action prefix, re-seed, continue K times); out-of-combat leak test on 200 games.

**D-a, expert-profile audit (day 3; 50,000 traced runs over the four tracks, about 20 core-hours).** Elites per act, share of campfires rested, skip rate by act, deck size and HP at the act-1 boss, relics, removals, upgrades and gold spent at each boss, against the expert column: 2.6 Act-1 elites, 9% rests, 21 / 39 / 53% skips (w1 §2.2). An audit, never a reward; repeated at every gate.

**D-b, replay attribution (day 4; 2,000 lost A20 runs, about 15 core-hours).** Per run, 16 fresh-dice replays of the fatal fight and 16 fresh-dice continuations from the start of the act it died in. Reading for act-2-boss deaths: the mean re-fight win rate among runs that died equals μ(1−ρ), where μ = 0.604 is the entrants' win rate (d1 §2) and ρ is the share of outcome variance fixed by the state on arrival [my derivation]. A mean ≤ 0.42 (ρ ≥ 0.3) says what the run brings to the boss decides it; ≥ 0.54 (ρ ≤ 0.1) says the boss is dice and D1's first hypothesis is wrong as stated. A regression of the per-state win rate on HP against deck and relic features then splits attrition from deck strength.

**D-c, joint bias probe (days 4–6; 288,000 A20 runs ≈ 80 core-hours).** f_θ reduced to 24 constants: one bias per (surface, option class, act) for elite nodes, rest against smith, card skip, shop removal / relic / card / save, and posture by fight type. Cross-entropy method, 6 generations × 48 candidates × 1,000 runs; objective = A20 win + 0.1 per act. The best mean vector is tested on 2,000 fresh paired seeds and confirmed. Difference from the knob tuning the ledger calls saturated (r1 §3): those knobs moved one at a time, on 300-seed slices and the floor score, and several are inert at A20 because a learned hook returns first (c2 §0.9); these biases sit on the learned hooks, move jointly, and are judged on wins. A gain ≥ +1 point says a profile shift alone pays, and it initialises P1. "Null" for P0's kill rule: profile already inside the expert band, ρ ≤ 0.1, and a probe gain < +0.5 points.

**P1 mini-campaign (days 6–12).**
- Surfaces: map node, rest site, card take or skip, shop purchase, pre-fight posture.
- f_θ: MLP 706 → 128 → 64 → 1 on (666 run features, 20 path features, 20 option features), about 100k parameters; critic initialised from vr_251.
- 40 iterations × 25,000 A20 runs; return as in §2; temperature set so about 10% of decisions deviate from the argmax [estimate]. A second campaign that adds restarts is pre-declared.
- Power: an alternative taken in 10% of 1M visits has 100,000 samples; with a residual outcome SD near 0.22 after the baseline (value correlation 0.55–0.6 at A20, h3 §2), a win-probability difference has a standard error of 0.0007 [estimate]. F33 had about 700 decisions × 16 playouts.

**Test (days 12–14).** Argmax policy against `f43a20_sf2` on 2,000 fresh paired seeds, then a confirmation slice. The paired standard error of the win difference is about 0.6 points per slice, 0.45 pooled [estimate].
- **Go:** ≥ +1.5 points confirmed, what a search doubling buys on the current best (7.65 → 9.10, d1 §3). Start P2.
- **Grey (+0.5 to +1.5):** run the pre-declared restart campaign, then decide by the same rule.
- **Stop:** < +0.5 points with a flat training return. The strategic layer is then near its optimum for this pilot; the plan ends and its tools go to the pilot-first plans.

## 5. Expected outcome

- **T1, A20 Act-3 ≥ 25%: 35%.** For: a no-search agent reports 21.9–27.5% on this game (s4 §1), and Mentor's legs gave +7.5 to +15.5 points (s4 §6). Against: +17 points is a tripling; the outside engines are unaudited, and this project's own fidelity fix halved A20 from 6.35% to 3.0% (h4 §1); an independent re-run of Mentor scored 16.8% against 27–30% self-reported (w1 §1.1). Intermediate: A20 ≥ 15% at 60% [estimate].
- **T2, A20 Heart ≥ 20%: 10%.** It needs Act-3 clears with three keys near 35% and Heart fights won near 60%; today 2.5% (50 of 2,000 [computed from d1 §2]) and 14%. The Heart fight does not respond to search, so conversion must come from the deck, this plan's lever; but the reward is a desert at 0.3% and the Heart ladder from A0 to A20 is untried.
- **T3, A20 Heart ≥ 45%: 2%.** Expert level needs a pilot that plays engine decks through long boss fights; this plan leaves the pilot alone.

**Most likely failure.** The policy gains 2–5 points at A20 by repricing HP, elites and potions, then plateaus near 10–13% because the drafter learns the pilot's taste: random playouts value setup and scaling as if followed by random play (c1 §6.2), so the decks that beat the act-2 boss and the Heart are never rewarded. The layer-crossing result predicts this: each run policy suits its own combat player (w1 §1.2). Second: gains that do not survive fresh seeds, as with expert iteration on V at A20 (h3 F20–F21).

## 6. Conflicts and owner decisions

**Closed lines touched.**
- *Learned leaf value: not touched.* V_pub and V_priv are run-level critics over out-of-combat states; they are never evaluated inside the combat tree.
- *Optional run-aware fight utility (P3).* Before a fight, V_pub on a grid of post-fight HP and potion counts becomes a lookup table that replaces "35 + HP + potion credit" at terminal states only. It is not a leaf evaluator and differs from the closed line in target (run outcome under the search-played policy) and representation (run-level entities), but it is adjacent to the closed value-shape line (h7 §1: `ldw35`, `ltw1`, `brt8`). It runs only if the owner reopens that line; the plan does not depend on it.
- *Lines closed by the loop that this plan reopens:* card-decision work (F33), A20 value iteration (F21), path iterations (F36–F38), key rules, Neow, elite veto. Each was closed as a one-surface change judged on the floor score with 10^4 decisions or 300–4,000 seeds. Here all surfaces move together, on wins, over 10^6 runs, starting from the current best.
- *"Models that replace the rules lose"* (r1 §3: −0.22 to −0.62): those were value argmaxes without exploration data. Here nothing is replaced before the residual passes a paired gate.

**Rule changes requested.**
1. Primary keep metric: win rate on the target objective, the present score as a guard (r1 §5).
2. Out-of-combat inference, about 40 ms per game, is charged through the usual wall-clock calibration.
3. Posture presets are allowed, because the simulation cap does not change.
4. Standing approval for fleet-days per phase (today no multi-hour worker phase runs without the owner, h1 §5).

**Hardware and money.** The ten-box fleet for 18.5 fleet-days over twelve weeks, $800–6,700 by rate; a consumer GPU for two weeks ($115–300) only if the encoder becomes a token transformer.

**Before the start, the owner decides:** P0 + P1 (two weeks, about 850 core-hours); the metric change; T1 before the Heart or not; one policy across ascensions or A20 only; whether the fight-utility line may be reopened.

## 7. What this plan deliberately does not do, and what it would adopt later

**Not done.**
- No change inside the combat search (tree across the turn boundary, chance nodes, learned playouts, sparse leaf network). The pilot stays fixed so the strategic signal is clean.
- No learned fight-outcome model in place of fights (s4 §1); no online simulated fights (about 100× a game's search, h2 §2).
- No language model, no imitation of mass human logs, no end-to-end network for combat (w1, dead ends).
- No reallocation of simulations between fights (failed repeatedly: h1, h3, h5).

**Adopted later from the other directions.**
- *A stronger pilot*, whichever plan delivers it: swap it in and run one more campaign, since a run policy must be re-fitted to its combat player (w1 §1.2). This is the main route from T1 to T2 and T3.
- *The perfect-information ceiling* and the search curve beyond 4× (d1 §6.1–2): they tell P3 whether its plateau is the pilot.
- *A lookup-cost playout policy* (w3 item 2) and *sequential paired testing* (w3 item 3): more runs per day and cheaper gates.
- *Expert winning decks as boss-practice restart states* (s4 §5), once the restart tool exists.
