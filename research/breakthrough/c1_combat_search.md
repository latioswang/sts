# C1 — The combat search as it is in the code (stage 1, 2026-11-16)

Abbreviations: `FS.h` / `FS.cpp` = research/cpp/FairSearch.h / .cpp; `FA.cpp` = research/cpp/FairAgent.cpp; `vf/` = research/vf; `sim/` = /home/user/sts_lightspeed. program.md is cited by entry timestamp (its line numbers move). "Bests" = the four best-parameter files. Nothing was built or run; numbers marked [computed] come from stored run rows in research/runs.

## 0. Where the code differs from the project description

- **"Determinised MCTS"**: there is one tree per decision, not one per determinisation. Hidden state is re-sampled at the root of every simulation, and tree nodes are keyed by an observation hash, so different sampled futures share nodes (FS.h:5-11).
- **"Random rollouts"**: not uniform. End-turn has weight 0.05 against 1 for every other legal action in all bests (FS.cpp:397-414; `end_turn_w=0.05`).
- **"Learned root move prior"**: true, but switching it on also switches selection at *every* node from UCB1 to PUCT; below the root the prior is uniform (FS.cpp:444, 300-311).
- **"Fixed compute per decision"**: the code caps simulations, not time (FS.cpp:563). The soft early stop makes the count variable, and forced moves get no search (FS.cpp:557). Equal compute is enforced outside the code, by calibrating mean search actions per game (tools/f40_calib.sh).
- Present in code but off in all four bests: tree reuse, bilinear prior at all nodes, prior-guided rollouts, both learned leaf values, the mixed leaf, action budgets, robust-child selection, rollout cut-off.

## 1. The algorithm exactly

**Tree.** A `Node` is a player decision point: `visits`, `wsum`, a vector of edges (FS.h:102-106). An `Edge` is one legal action with `n`, `w`, `prior` and a map `observation hash → child node` (FS.h:95-101). Chance outcomes get no node of their own: an edge fans out into one child per distinct observation, and the edge statistics average over outcomes. Children are found only through their parent edge (FS.cpp:493-498), so the structure is a tree, not a graph.

**Observation hash** (FS.cpp:58-96): input state, outcome, turn, player HP/block/energy, the hand *in order* (id, upgrade, cost for turn, free-to-play, special data), sizes of the three piles, each monster's id/HP/block/targetable/current move (move omitted under Runic Dome), potion slots, card-select contents. Not hashed: player and monster statuses, pile contents, relic counters.

**Hidden information** (`determinise`, FS.cpp:98-133, called once per simulation at FS.cpp:470 on a copy of the real state): the draw pile below the `knownTop` cards is sorted canonically, then shuffled with the searcher's RNG; six simulator RNG streams (enemy AI, card random, misc, monster HP, potion, shuffle) are reseeded. Nothing is re-sampled later in the simulation: later reshuffles, enemy move rolls and random card effects follow the reseeded streams, so one simulation is one sampled future. The agent may know: pile contents but not order; the top cards it placed itself with Headbutt/Warcry (FA.cpp:204-233); current enemy intents and HP. Under Runic Dome the pending RNG-rolled move is re-rolled (FS.cpp:124-132). `bc.seed` is never read. The leak test re-randomises hidden state and requires the same move (eval/eval_main.cpp:78-108).

**Selection** (bests; FS.cpp:444-455): PUCT, `u = q + 1.5 · P · sqrt(N+1) / (1+n)`. `q = (w/n − vmin)/(vmax − vmin)`, where vmin/vmax are the extremes of all simulation values in this search (FS.cpp:527-528). Unvisited edges take the node's own mean as `q` (0.5 if the node has no visits). `P` is the MLP prior at the root only (`prior_mlp_depth=0`) and `1/|A|` elsewhere. Ties go to the lowest edge index, i.e. lightspeed's hand-written play order (FS.cpp:195-197; sim/src/sim/search/ExpertKnowledge.cpp:228). The UCB1 branch with c = 0.7 (FS.cpp:456-464) is not reached in any best.
Quirk: a new node gets `visits += 1` but no value (FS.cpp:521), while later passes add both (FS.cpp:535). A non-root node's mean is therefore k/(k+1) of its true mean, so its unvisited edges look worse than its visited ones.

**Expansion.** One node per simulation. When the executed action yields an observation unseen under that edge, `newNode` enumerates the legal actions, sets priors, and a rollout starts there (FS.cpp:495-523). An action that is no longer legal (hash collision) ends the descent with a rollout (FS.cpp:483-486).

**Rollout** (FS.cpp:376-438). Each step enumerates legal actions and samples one by weight: END_TURN 0.05, potion 1, every (card, target) pair 1. The block-aware weights, play-order bias and prior-guided sampling are off. Depth limit: `rolloutTurns = 99` turns beyond the start turn, i.e. to the end of the fight; the simulator itself declares a loss after turn 500 (sim/src/combat/BattleContext.cpp:941-946).

**Value** (`terminalValue`, FS.cpp:345-374), defaults unless a best overrides:
- Victory: `35 + 1·HP + potionValue·potionsKept + 0·maxHP − 0.01·turn`, minus 30 after a Smoke Bomb escape. (`lowHpPen` is 0.)
- Loss: `10·(1 − enemyHP/enemyMaxHP over non-minions) + 0.2·turn + potionValue·potionsKept/2`.
- Cut-off (unreached with 99 turns): `35·(1 − ratio) + HP + potionValue·potions/2`, or a learned leaf if enabled.

**Backup** (FS.cpp:527-536): the same scalar is added to every edge and node on the path; plain means, no max, no discount.

**Final move**: the most-visited root edge, first index on ties (FS.cpp:578-582).

**Early stop** (FS.cpp:562-576): after each simulation, stop if `n1 − n2 > es_frac × simulations left` (es_frac = 0.1 in all bests; 1.0 is the provably move-preserving rule).

**Tree reuse**: `advance()` exists (FS.cpp:278-290) but `tree_reuse` is off; a new `FairSearcher` is built for every decision (FA.cpp:154, 171). Tested once at A10: tie (program.md, 2026-11-09 20:40 "F41 (a)").

## 2. The action space

One decision is one `search::Action` (sim/include/sim/search/Action.h:24-30): play one card (hand index + target), use one potion (slot + target), one card-select choice (single or multi), or end turn. `Action::execute` runs the simulator until the next player input (sim/src/sim/search/Action.cpp:482-525), so END_TURN contains the enemy turn, the next draw and the next intent roll.

Enumeration (FS.cpp:159-231): playable cards, deduplicated only against the *adjacent* identical hand card (FS.cpp:188-194), sorted by play order, one action per targetable monster for targeted cards; then potions (Fairy in a Bottle and target-less targeted potions appear only as a discard action, FS.cpp:221-223 with Action.cpp:500-501); END_TURN last. Smoke Bomb is barred in boss fights (FS.cpp:135-139). In hallway fights potions are removed from tree and rollouts unless HP is below 25% of max (`hall_potions=0`, FA.cpp:135).

Whole-turn plans are never enumerated. Every card play is a separate search from an empty tree.

Typical counts [computed from the latest stored 2,000-game slice of each best]:

| track | battles/game | decisions/game | decisions/battle | search actions/decision |
|---|---|---|---|---|
| A0 | 22.7 | 462 | 20.3 | 7,838 |
| A10 | 22.1 | 443 | 20.1 | 6,584 |
| A20 | 15.9 | 286 | 17.9 | 5,948 |
| Heart | 22.2 | 479 | 21.6 | 7,800 |

Decisions include forced single-option moves. Decisions per turn are not logged; about 4-6 including END_TURN [inferred].

## 3. Learned parts in combat

**Root move prior** (`pfm`, vf/pf_mlp.h:17-39; weights cpp/pfm_weights.inc, `PFM_TRAINED = 1`). Input per legal action: 204 state features + 16-d embedding of the action token (card id, potion id, END_TURN, or one shared SELECT token; plus an "upgraded" embedding) + 4 target features (has target, target HP fraction, target attacking, target is lowest-HP; vf/policy_features.h:20-56). One hidden layer of 64 ReLU units, one logit per action, softmax over legal actions. Trained by cross-entropy on the search's own root visit distributions (vf/train_policy_mlp.py:2-5; logger FA.cpp:178-198).
Used: once per decision, at the root, inside PUCT. Not used: below the root, in rollouts, in the final move choice, or to tell card-select options apart (they share a token and have zero target features, so their logits are equal [inferred]). The bilinear prior (`pf`, K = 16) is compiled in and trained but disabled.

**Leaf value options** (all off in the bests). `leaf_vf`: compiled 204→64→1 MLP (vf/mlp.h). `leaf_sv=<id>`: runtime MLP of any shape from research/models/sv_<id>.txt (vf/sv_mlp.h:24-63). Both are read only when a rollout is cut off with the fight undecided (FS.cpp:363-365), so replacing rollouts needs `rollout_turns=0`. **Mixed leaf** (`mix_sv`, `mix_lambda`): a new leaf's value is `λ·sv(leaf) + (1−λ)·one rollout` (FS.cpp:515-519). Recorded results at equal wall-clock on A0: pure leaf −0.29 score; mixed leaf λ = 0.5 −0.02, no better than the plain agent at the same simulation count (program.md, 2026-11-11 09:30 and 12:45).

**The 204 features** (vf/vf_features.h:24-30, 72-190):
- Player, 44: HP (three forms), block, energy, energy per turn, 38 statuses and powers.
- Hand, 12: size, counts by type, total cost, zero-cost count, sum of KB tier values, crude attack and skill counts, Pain and Normality counts.
- Piles, 12: sizes of draw/discard/exhaust, type counts in draw and discard, Strike count.
- Key cards, 26: 0.5 per copy of 26 named Ironclad cards anywhere in hand + draw + discard.
- Monsters, 36: sums over living monsters (count, HP, largest HP, HP fraction, block, total and largest incoming attack, hits, attackers, minions, Dome flag) and 24 statuses summed over monsters.
- Global, 8: turn, potion count, boss flag, elite flag, floor, ascension, exhaust-pile-non-empty, input state.
- Encounter one-hot, 66.

Not represented: identities, upgrades and costs of individual cards beyond the 26 counts (and not which pile they are in; exhaust contents not at all); relics (none, except Dome and the Pen Nib status); potion identities; per-monster structure (which monster has which HP, status or intent; non-attack intents); enemy move history; card counters such as Rampage; the known top of the draw pile.

## 4. Cost model

- **Throughput** [computed from run rows; time is whole-game wall-clock, so it includes out-of-combat work]: 1.6-2.6 million simulator actions per second per core; 3.0-3.9 ms per combat decision; 1.1-1.9 s per game.
- **Simulations per decision** are counted (FA.cpp:177) but not written to run rows. The caps were calibrated to match the action count of 500 simulations without early stop (program.md, 2026-11-09 19:22 and 23:17), so about 15 simulator actions and about 6 µs per simulation [inferred].
- **Split inside a simulation** [inferred from the F52 calibrations, program.md 2026-11-11 09:19: learned leaf without rollouts 0.589× time at equal simulations, leaf plus rollout 1.455×]: rollout ≈ 87%, tree descent and expansion ≈ 13%, one `sv` evaluation (feature extraction + small MLP, with heap allocation in `sv::predict`, vf/sv_mlp.h:49-62) ≈ 45% of a simulation.
- **State copy**: one `BattleContext` copy per simulation (FS.cpp:468). The simulator already keeps the action queue as 50 fixed 40-byte slots, piles inline up to 48 cards and a flat status map (sim/include/combat/ActionQueue.h:23-55, CardPile.h:17-24, PlayerStatusMap.h:14-17), so the copy is a few kilobytes without heap traffic [inferred]. It does not appear in any profile.
- **Profiles**. perf, 4 A10 games (program.md, 2026-11-10 19:20): `executeActions` 14.5%, `enumerateActionsInto` 14.4% plus its sort 4.2%, `rollout` 7.5%, `addToBot` 4.4%, `afterMonsterTurns` 4.0%, `drawCards` 3.4%; "no single hotspot". Earlier gprof (program.md, section "Speed at equal wall-clock"): nothing above 9%; micro-optimisation gave 1.029×; `-march=native` gives 7-8%.
- **What limits simulations per second**: the ≈ 15 engine actions of a rollout, each needing a legal-action enumeration and an action-queue run; single thread; no batching.
- **Scale**: doubling simulations gave +0.05 to +0.08 score per track, quadrupling +0.08 to +0.14 (program.md, 2026-11-09 16:16 "F39 closed").

## 5. Parameters

Set in the bests (`sp` is built at FA.cpp:63-104; per-fight overrides at FA.cpp:119-138):

| key | meaning | default | A0 | A10 | A20 | Heart |
|---|---|---|---|---|---|---|
| `sims` | simulation cap per decision | 500 | 1000 | 950 | 1050 | 775 |
| `early_stop`, `es_frac` | soft early stop | 0, 1.0 | 1, 0.1 | 1, 0.1 | 1, 0.1 | 1, 0.1 |
| `prior_mlp`, `prior_mlp_depth` | PUCT; MLP prior at depth ≤ d | 0, 1 | 1, 0 | 1, 0 | 1, 0 | 1, 0 |
| `end_turn_w` | rollout weight of END_TURN | 1 | 0.05 | 0.05 | 0.05 | 0.05 |
| `hall_potions` | potions searchable in hallway fights | 1 | 0 | 0 | 0 | 0 |
| `potion_v` | value per potion kept | 4 | 8 | 4 | 4 | 4 |
| `potion_v_boss` | same, in boss fights | = `potion_v` | 4 | 4 | 2 | 4 |

At their defaults in all four bests:
- Selection: `c` 0.7 (UCB1, unused), `prior` 0 (bilinear prior), `prior_c` 1.5, `prior_temp` 1, `select_q` 0 (robust child), `budget_actions` 0.
- Rollout: `rollout_turns` 99, `boss_rollout_turns` (= `rollout_turns`), `potion_roll_w` 1, `roll_order_k` 0, `roll_block_w` 1, `roll_atk_w` 1, `roll_prior` 0, `roll_prior_eps` 0.25.
- Value: `win_bonus` 35, `hp_w` 1, `maxhp_w` 0, `escape_pen` 30, `lowhp_pen` 0, `lowhp_thresh` 20, `loss_dmg_w` 10, `loss_turn_w` 0.2, `cut_win_frac` 1, `cut_hp_frac` 1, `potion_v_elite` and `potion_v_heart` (= `potion_v`).
- Learned leaf: `leaf_vf` 0, `leaf_sv` 0, `mix_sv` 0, `mix_lambda` 0; data logging `leaf_log`, `leaf_log_p` 0.002, `leaf_log_k` 8.
- Budget and loop: `boss_mult` 1, `heart_mult` 1, `tree_reuse` 0, `known_top` 1.
- Hypothetical fights for out-of-combat models use the same searcher with `cf_sims` 60 / `bf_sims` 100 and potions off (FA.cpp:394-446).

## 6. Structural weaknesses (all [inferred], ranked)

1. **The tree ends at the turn boundary.** The hash contains the ordered hand and each simulation reshuffles the draw pile, so after END_TURN almost every simulation opens a new child (FS.cpp:66-74, 110, 495-498). Statistics never build up past the current turn; everything later is one random playout. Change: sampled-hand chance nodes with widening, an afterstate value, or a nested turn-level search.
2. **The playout is the wrong estimator in hard fights.** Play is random to the end (FS.cpp:376-438). The notes record that in boss fights random rollouts cannot win, leaving only `10·damage fraction + 0.2·turns`, where the player's HP does not appear, so attacking beats blocking (program.md, section "Batch 48"; audit 2026-11-09 20:56: racing instead of blocking, four potions on turn 1). Setup, scaling and potion timing are valued as if followed by random play. Cheap fixes failed: prior-guided rollouts −0.062 at 2.43× cost, learned leaf −0.29. Change: a much stronger playout policy or value at similar cost.
3. **The objective is not the run.** The value is 35 + HP + potions per fight (FS.cpp:345-374), blind to whether this is the last fight, a rest site follows, or the run value `vr_*`; a loss even pays for unused potions (FS.cpp:373). Change: score fight ends with the run-value model, in win-probability units.
4. **Root comparison is unpaired and noisy.** Each simulation draws a fresh future, so root actions are compared on different draws (FS.cpp:470); about 500 simulations spread over the legal actions, with rollout sd ≈ 9 from one leaf and 32 across leaves (program.md, Phase S2 smoke test). Q is squeezed by a global min-max that one loss stretches (FS.cpp:441). Change: common random numbers across root candidates with sequential halving.
5. **Learned parts see aggregates only.** No card identities, relics or per-monster structure (section 3). The leaf-value line stopped at RMSE 11.8 with 10× data changing nothing (program.md, 2026-11-11 12:45). Change: an entity-level encoder.
6. **No transpositions, no turn plan.** Order permutations of the same cards are separate subtrees; duplicates merge only when adjacent; each card play restarts from nothing (FS.h:100; FS.cpp:188-194; FA.cpp:171). Change: canonical turn plans on a full-state table. Caution: plain tree reuse was a tie.
7. **Selection below the root is crude.** Uniform priors, the under-counted node mean (FS.cpp:521 against 535) and play-order tie-breaks make deep nodes stick to the first edge tried. Change: fix the mean; use a cheap prior or completed-Q there.
