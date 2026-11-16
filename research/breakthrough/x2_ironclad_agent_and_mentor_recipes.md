# X2 — sts-ironclad-agent (A) and Spire Mentor (B), read at the source

Read 2026-11-16 from raw repository files (tags → URLs at the end). "[computed]" = my arithmetic on their published per-game logs; "[inferred]" = my reasoning; "[unverified]" = not checked. Could not open (404): Mentor's `2026-10-31-ironclad-a10-cont300.md` and its `it700`/`it850` provenance files.

# Part A — sts-ironclad-agent (destire-mio)

## 1. What it is and what it claims

- **Setting.** Slay the Spire 1, Ironclad, Ascension 20, natural start; a win = three keys, both Act-3 bosses, Shield & Spear and the Heart. Prismatic Shard is excluded from the simulator [A1][A9].
- **Agent.** One frozen network makes every out-of-combat choice; combat is simulator search [A1].
- **Results [A1].**

| Game | Policy | Seeds | Games | Heart wins |
|---|---|---|---:|---:|
| Simulator | teacher `p300_play_v21` | dev block 3900012000+ | 2,000 | 50.1% |
| Simulator | teacher | fresh block 3900018000+ | 1,000 | 49.5% |
| Simulator | distilled student v2 | 3900040000+ | 1,000 | 50.2% (teacher 54.7% on the same seeds; paired −4.6 points) [A7] |
| Real game | distilled student v3 | 3900020000+ | 501 | 50.1% [45.7, 54.5] |

- The real-game figure is 473 finished games (231 wins, 48.8%) plus a later rerun of the 34 bridge-faulted seeds on a repaired importer (28 finished, 6 still faulted) [A1][A8]; 251 wins in total, so 20 of the 28 reruns won [inferred].
- Further teacher blocks in the logs: 3,326/6,232 = 53.4% [A11, computed]. The planned 1,024-unseen-seed acceptance "has not been run" [A1].
- **Hidden information.** The out-of-combat network reads public state only [A7]. Combat does not: the search is rooted on a copy of the true `BattleContext`, RNG streams and draw-pile order included [A5]. In the real game a companion mod exports the RNG state and each fight is planned in the simulator, by design once per fight [A13]. The teacher code says so itself: "Frozen Eye has no value when the combat search already sees the draw pile" [A4]; the upstream README: "the teacher effectively sees the future" [A9].
- **Compute.** 40,000 simulations per search call; ×12 at bosses (480,000); 80,000 base in Act 4, so the Heart gets 960,000 [inferred from A4 and the boss list in A5]; 320,000 for Shield & Spear [A4]. Median 7.0 M simulations and 97 s on one core per game [A11, computed]; real game 382 s median [A8, computed].
- **Date.** Commits to 2026-10-01; 255 commits, MIT [A18].

## 2. Architecture

- **Observation.** A flat vector: sts_lightspeed's `NNInterface` observation (6,820 values, each divided by its maximum) plus 23 values for the burning-elite position = 6,843 [A19][A7]. Deck, relic and potion blocks sit in its tail; the exact layout was not read [unverified]. No combat piles or hand: the network never acts in combat.
- **Options.** Each legal action has an 807-value descriptor: action kind, screen, card one-hot (371) with upgrade/bottled flags, relic (181), potion (44), map room with look-ahead, rest option, event option, Neow bonus/drawback, price, amount [A7 schema].
- **Network.** One scorer applied per candidate, argmax, no overrides. Parent: 5,529 → 192 → 1, 1,061,953 parameters [A10]. Student: 11,195 → 384 → 1 ReLU, 17 MB; it adds 20 public scalars, candidate order, mean/max over all candidates' descriptors, and 26 route features (room counts on reachable paths, nearest room of each type, burning-elite reachability) [A7].
- **Combat.** `BattleScumSearcher2` from sts_lightspeed, modified [A5][A6]:
  - UCT with exploration 3√2; a new leaf takes a uniform random child, then a playout to the end of the fight. Playouts are random except END_TURN gets one-tenth weight while a card is playable, three quarters of card picks follow the built-in expert play order, and attacks retarget the enemy with least HP+block.
  - **Max backup**: a node keeps the best playout seen ("for deterministic planning").
  - Terminal-only evaluation, no learned value. Win = 100·(35 + HP + 4·potions − 0.01·turn), plus post-victory relic healing, max-HP gained in the fight (Feed), minus a stolen-gold penalty. A loss gets a small enemy-HP term.
  - Once a winning line exists the agent executes 15 of its actions before replanning, otherwise 5 by visit count; the subtree is reused.

## 3. Training recipe

There is no end-to-end RL in the final system. Three layers:

1. **Parent network.** Success-weighted imitation of whole trajectories from a heuristic collector, continued from an earlier checkpoint: 2,659,555 decisions from 37,423 seeds, with only 120 distinct winning seeds; AdamW, lr 3e-5, two full passes; fights at 8,000 simulations (boss ×3) [A10]. It wins 7.6% of 512 fresh seeds [A3].
2. **Teacher = parent + measured tables + rules** [A4]:
   - *Stage value tables* [A12][A3]: take real decks at the entry of a stage (Heart, each boss, elites, Act-1 hallways), apply one change (add / upgrade / remove a card), replay the fight on several battle seeds with the production search, and record the change in survival and HP kept. Combined as Σ Δ_stage / p_stage over the stages still ahead, plus 0.3 × HP change at elite stages. Used for card rewards and single-card selections.
   - *Counterfactual forks*: at a boss relic, Neow, map or card decision, play every option to the end of the run with the same policy. They produced a per-act boss-relic table, an elite-seeking rule (at ≥60% HP an offered elite beats a hallway, event or rest) and card adjustments (Feed, Immolate).
   - *Human knowledge*: `guide_rules.py` encodes an 84-page Ironclad guide; `baixi_rules.py` comes from a 300-run audit of two expert players [A12].
   - *Loss-trace fixes* (`fix`–`fix4`): pre-boss rest below 75% HP, never enter the portal without the ruby key, no lethal event options, low-HP routing, Act-4 potion buying.
3. **Student.** Behaviour cloning on the teacher, cross-entropy weighted ×3 for shop and card-select and ×2 for card rewards, warm-started from the parent, then two DAgger rounds of 200 games; AdamW 3e-4 / 2e-4, batch 128, early stopping on held-out seeds [A7]. v2 used 3,000 teacher games. v3 used about 8,200 teacher seeds, 1,415,765 decision groups, weight 8, batch 512 [A16].

Compute: the published logs of this phase hold 67,412 games, about 2,590 single-core hours and 8.9·10¹¹ simulations [A11, computed]; the networks train on CPU [A7].

## 4. What mattered, what failed

**The ladder** [computed from A11 unless cited; seed blocks differ unless "paired" or "same seeds"]:

| Step | Heart wins |
|---|---:|
| Parent, 8,000 sims | 7.6% of 512 [A3] |
| + 32,000 sims, boss ×12, pre-boss rest (paired) | 16.8% [A3] |
| + stage tables, tree reuse (with 15% Act-3 exploration) | 20.2% of 3,000 |
| + portal/event safety, playout target focus | 27.1% of 2,000 |
| + boss-relic table (paired) | 26.1 → 29.4% |
| + human-guide rules (same seeds) | 29.4 → 38.0% |
| + no potion discards in search (same seeds) | 38.5 → 41.1% |
| New block: + that, Heart budget ×2, fix2, expert-audit rules | 38.2 → 40.7% |
| + elite seeking (same seeds) | 45.5% |
| + fix3; + post-victory-HP score, 320k at Shield & Spear, card adjustments, fix4 | 48.2% → 50.1% |

- **Search saturates.** ×8 instead of ×4 budget: 19.5% vs 18.0%, "within error". The Heart is insensitive to budget (15/40, 17/40, 16/40 at boss ×3/×12/×24) [A3]; "the limit is deck and entry HP" [A2].
- **Where it still loses** (death rate among games entering each stage, 3,000 games [A1]): Act-1 path 9.1%, Act-1 boss 1.9%, Act-2 path 10.3%, Act-2 boss 5.1%, Act-3 path 4.3%, Act-3 bosses 9.3%, Shield & Spear 3.7%, Heart 21.3%. So about 66% clear Act 3 with keys, and about 76% of those kill the Heart [computed]. Heart win by entry HP: ≥75% → 90%, 50–75% → 62%, <50% → 31% [A2]. Real game: 302/473 reached Act 4, 231 won [A8, computed].
- **Failed** [A3][A9][A15][A2]:
  - many policy-gradient and preference-learning rounds on the binary Heart outcome (e.g. −3 and −12 wins on 128 seed families);
  - a learned combat value in PUCT (16 vs 20 wins); six early attempts to distil combat into a network;
  - fight-outcome models used directly for card picks ("noise 2–6× the option differences");
  - cloning one streamer's picks (+6 and +1 on 128, not significant);
  - adaptive extra search, a second guide rule set, shop potion buying.
- **Student gap.** It agrees with the teacher on 92–94% of map, card-reward and rest choices but only 72–75% of shop and card-select choices [A7].

## 5. Simulator and game interface

sts_lightspeed (C++, MIT) at commit 7476a81 plus nine patches, Python 3.12 bindings [A17]. The author labels parity with the Java game **INCOMPLETE**. It was checked by an original-vs-simulator comparer on captured scenes (several hundred regression entries) and by the 501-game real run, whose interval covers the simulator estimate [A1][A9][A17]. The real run used the modded game (BaseMod, CommunicationMod, a state-export mod) [A8]. Speed: about 72,000 simulations per second per core [A11, computed]. Code, parent weights and student v2 weights are in the repository; I did not find the v3 weights used in the real game [unverified].

## 6. How to use it for this project

**(a) Replication on another exact simulator.** The recipe is tables and forks, not training. Their bill was about 2,600 core-hours at 97 s per game. With a fair search at our 1–2 s per game: stage tables are 100–200 entry decks per stage [A3] × ~100 deck changes × 4–8 seeds, about 10⁶ fights over ~12 stages; forks, A/B blocks, teacher games and DAgger are about 10⁵ game-equivalents. Together roughly 100 CPU-hours at 1,000 simulations per decision and 1,000 at 10× that budget [inferred]. GPU-hours: 0.

**(b) Evaluating the released agent.** Our project appears to share the sts_lightspeed lineage [inferred from the internal map]. If so, `student/` plus `agent/armG_train.py` can run on our engine once the 6,843-value observation and 807-value descriptor match bit for bit; `student/INTERFACE.md` describes that check [A7]. The valuable run is a 2×2 on common A20 seeds: {their student, our strategy} × {their exact-state search, our fair search}. 1,000 games cost about 30 CPU-hours in cells using their search at their budget. Their win needs keys and the Heart.

**(c) Copy, ranked.**
1. Stage value tables: causal deck-change effects measured per future fight with the production pilot.
2. Counterfactual full-run forks on the same seed, for rare high-leverage decisions (boss relic, Neow, elite routing).
3. The per-stage death table and boss win rate by entry HP as the steering instrument; paired fixed seed blocks with a fresh-block confirmation.
4. Teacher (network + tables + rules) → one network by cloning and DAgger, with a paired check.
5. Information-neutral combat details: score the HP after victory healing and max-HP gains, no potion waste, extra budget only where the death table points.

**Do not copy.** Exact-state search with max backup; open-loop plan execution; rules that presuppose it (Frozen Eye exclusion; elite seeking at 60% HP should be re-measured); excluding faults from the denominator.

**(d) Deck building versus piloting.** With the clairvoyant pilot held fixed, strategy changes moved the Heart rate from 16.8% to 50%. Pilot budget moved it from 7.6% to 16.8% and then saturated. So out-of-combat decisions are a first-order lever even under near-perfect piloting; a weak strategy with that pilot still won under 10%. It does **not** show that this strategy reaches 50% with a fair pilot. No fair ablation exists in the files I opened. Mentor's measured rent for clairvoyance, −23 to −33 points (Part B), and the layer-crossing table in [B9] say the pilot is first-order too, and that strategy and pilot are co-adapted. Our 8% / 0.3% is therefore a product of both gaps; the 2×2 above is the cheapest way to split it.

## 7. Doubts

- Clairvoyant combat in simulator and real game: not comparable with a leak-free agent.
- Real-game faults (6.7%) were first excluded, then rerun after a repair; either treatment can bias the number [inferred]; 6 seeds remain out.
- The 50.1% development block was reused to select rules; other blocks give 49.5–53.5%.
- The real-game student is v3, the published weights are v2.
- "A fresh CMake build does not reproduce the cloud games move for move… treat its win rates as unmeasured" [A1].
- The teacher embeds human-guide knowledge; this is not learning from scratch.

# Part B — Spire Mentor (tfriedel), Slay the Spire 2

## 1. Claims

Rust port of StS2 v0.107.1 for all five characters, MCTS in fights, PPO run policy; "non-clairvoyant / using chance nodes" [B1]. A win is beating the final-act boss or bosses [inferred].

- Ironclad `cont300_it1150`: A10 **30.0%** of 400 [25.7, 34.7] at 4,000 simulations per decision, 16,000 in boss fights; on the same seeds the previous champion scored 27.0% (p = 0.34) [B1][B6].
- Earlier champion `a10_it700` at 8,000 simulations: A0 96.0%, A8 84.5%, A9 56.4%, A10 27.2% (200–500 runs each), on an engine with a since-fixed reward bug "in A10's favour" [B1][B7].
- Independent host [B9]: Ironclad A10 **67/400 = 16.8%** [13.4, 20.7] at Mentor's budgets, with the tree rebuilt each decision, the Ironclad search settings off, in-combat card choices by another policy and one event skipped.
- Compute: 1,000 simulations ≈ 14 ms on one thread [B1]; an 8,000-simulation run averaged 166 s [B7].

## 2. Architecture

- **Run policy** [B3]: token transformer, dim 192, 3 layers, 4 heads, pre-norm RMSNorm, SwiGLU (about 2 M parameters [inferred]). Observation tokens: one state token, one per deck card, relic, potion and map node. One token per candidate action follows; a shared linear head scores the action tokens (variable action sets); mean-pooled value head; auxiliary head predicting the destination room type.
- **Token** = section embedding + sum of 14 categorical embeddings (card, upgrade, relic, potion, room, event, option index, decision surface, boss, deck operation…) + sinusoidal coding of 26 continuous values (HP, gold, floor, prices…).
- **Combat search** [B4]: UCT with a transposition table and progressive widening at chance nodes.
  - *Draws* ("BeliefDraw"): the unknown part of the draw pile is a sorted multiset; each uncertain draw samples from a reseeded stream, so every draw at every depth is a chance node; outcomes that look identical to the player are merged. Widening cap ⌈C·(visits+1)^α⌉, with separate constants for draws (2.0; 4.0 for Ironclad) and end-of-turn (5.787; 23.1 for Ironclad) [B2][B7].
  - *Leaf*: playouts to the end of the fight or a depth cap, by a heuristic or learned playout net; graded terminal scores (wins +1.0…+1.5 by HP, potions and turns; losses −1.45…−1.05).
  - Ironclad adds a behaviour-cloned move prior in elite and boss fights [B7].

## 3. Training recipe [B2][B3]

- PPO over whole runs. The MCTS plays every fight during collection; every multi-option non-combat decision is sampled from the net.
- Loss: clip 0.2, 0.5·value MSE, entropy bonus, 0.1·auxiliary cross-entropy; GAE γ = 1, λ = 0.97; EWMA advantage normalisation; AdamW 3e-5 policy / 1e-4 value, weight decay 1e-4, gradient clip 5, minibatch 192, 2 epochs; learning rate annealed linearly to 0.3×.
- Entropy bonus 0.05 held flat, then geometric decay to 0.0125; warm starts inherit the parent's schedule.
- Reward: 0.5·floors/total + 0.5·win, as a potential difference with shaping (0.1·HP fraction, 0.035·upgraded cards).
- Curriculum: uniform A0–A10 per game; the last two Ironclad legs A10 only [B6]. Always warm-started from the previous champion.
- **Budget.** Collection at 250 simulations (1,000 at bosses) × 960 games per iteration; promotion gates at 1,000/4,000 on paired seeds with McNemar's test; strength evaluation at 4,000/16,000 (16× added only +1.5 points over 4×).
- **Cost.** 46 collection cores and one RTX 3090. ≈208 s per iteration, 150 iterations ≈ 9 h [B1]; the last 300-iteration A10 leg ran ≈410 s per iteration, about 38 h [B6]. Older legs (256 games at 1,000/4,000) took 31–82 h [B8]; collection is 87–97% of wall-clock; the update is ~33 s on GPU vs ~497 s on CPU.

Ironclad legs: event pilot 31.6 → 47.2% pooled A0–A10; +30 iterations with LR anneal 43.1 → 49.1%; `a10_it700` +6.9 points at A10; new card features `cardmods_it850` 21.7 → 28.1% (1,000 paired seeds); +300 iterations 21.7 → 25.4% (p = 0.021, promoted although it missed its bar) [B2][B6][B1].

## 4. Mattered / failed [B2][B5][B8]

- **Clairvoyance rent**, same binary, 2,000 paired A0 runs: PPO policy 81.25% with clairvoyant search vs 55.75% honest; older policy 38.35% vs 15.55%. Search-side fixes recovered only ~14% of it; the stated next lever was retraining under honest search.
- An environment bug hid every event decision from the policy; fixing it and retraining gave +15.7 points pooled.
- Cheap search with more games beat the default at equal wall-clock (+2.5 points); the same volume at high search quality gained nothing — "the binding constraint is data volume".
- Omitting the learning-rate anneal cost ~6 points.
- Failed: a campaign with four schedule deviations (−13.3 points, 44 h); Prioritized Level Replay (null); a combat-outcome surrogate replacing search in training; a cheap screening harness (cannot resolve effects under ~8 points).

## 5. Simulator

Rust, MIT. Lockstep parity against the game's own C# after every action: Ironclad 6,104/6,104 fuzzed fights clean, whole runs 1,232/1,250. About 59,000 random-play fights per second per core. Code and ONNX weights released [B1].

## 6. Use for this project

- **(a)** One 300-iteration leg is 288,000 runs: about 1,600 core-hours and 3 GPU-hours at their cost [inferred from B6]; the whole Ironclad lineage (~1,150 iterations) is on the order of 10⁴ core-hours [inferred]. At our 1–2 s per game a leg is 80–160 CPU-hours plus the update; CPU-only updates are feasible for a 2 M-parameter net.
- **(b)** Not evaluable against us: a different game.
- **(c)** Copy: search in the training loop at a reduced budget with a full-budget gate; candidate-token transformer with a value head; γ = 1 graded-progress reward; chance nodes with outcome merging and widening; paired pre-registered gates; warm-start discipline. Do not copy: the surrogate, level replay, unpaired comparisons.
- **(d)** In [B9], Mentor's decks win 67/400 with its search but 22/400 with a no-search policy in combat. A run policy trained with the pilot in the loop is specialised to it, which argues for training our strategy against our own fair search, not importing A's.

## 7. Doubts

Self-reported; the table mixes budgets, dates and engines; the last champion missed its gate; the independent figure is 16.8%, not 30%, under a non-identical driver; StS2 A10 is not StS1 A20 Heart.

## Sources (opened)

RA = `https://raw.githubusercontent.com/destire-mio/sts-ironclad-agent/main/`; RB = `https://raw.githubusercontent.com/tfriedel/spire-mentor/main/`

- [A1] RA`README.md` · [A2] RA`docs/status-2026-09-30.md` · [A3] RA`docs/p300-fight-decomposition.md` · [A4] RA`agent/p300_play_v21.py`
- [A5] RA`combat_engine/combat4r/engine-source/src/sim/search/{BattleScumSearcher2,ScumSearchAgent2}.cpp`, `…/include/sim/search/ScumSearchAgent2.h`, `…/include/constants/MonsterEncounters.h` · [A6] RA`combat_engine/combat4r/agent/{combat_search_reuse.h,fightsim.cpp}`
- [A7] RA`student/README.md`, `student/distill2_{model,train,features,pipeline}.py`, `student/models/{schema,freeze}.json` · [A8] RA`docs/live-original-distill3-student-20261001.{md,json}`
- [A9] RA`docs/history.md` · [A10] RA`docs/ironclad-experiments.md`, `runtime/config.json` · [A11] RA`experiments-archive/cloud/c5…c52-*.jsonl.gz` (42 files)
- [A12] RA`agent/p300_stage_values.py`, `agent/guide_rules.py`, `agent/baixi_rules.py` · [A13] RA`steam/README.md`, `docs/original-importer-{repairs,followup}-2026-10-01.md`
- [A15] RA`docs/first-principles-plan.md`, `docs/ironclad-research-direction-review.md` · [A16] RA`experiments-archive/cloud/distill3-v21-20260930/{seed-declaration.json,bc-d2-lr0.0002-w8.log,extract.done.json}`
- [A17] RA`sim_patch/README.md`, `sim_patch/parity/README.md` · [A18] `https://github.com/destire-mio/sts-ironclad-agent` and `/commits/main` · [A19] RA`agent/armG_train.py`
- [B1] RB`README.md` · [B2] RB`docs/ppo-champion-training-playbook.md` · [B3] RB`sts2-pycli/python/ppo/{trainer,rewards,model,schema}.py` · [B4] RB`sts2-rs/crates/sts2-mcts/src/lib.rs`
- [B5] RB`sts2-rs/docs/experiments/2026-07-12-clairvoyance-rent-reanchor.md` · [B6] RB`sts2-pycli/data/ppo/ppo_fullrun_ironclad_cont300_it1150.provenance.md`, `champions.toml` · [B7] RB`docs/experiments/2026-10-26-ironclad-release-winrates.md`
- [B8] RB`docs/experiments/2026-08-19-training-sims-isocompute.md`, `docs/ppo-screen-playbook.md`, `docs/combat-surrogate-investigation.md`, `docs/experiments/2026-08-25-ironclad-underdocks-campaign.md` · [B9] `https://arxiv.org/html/2611.12088`
