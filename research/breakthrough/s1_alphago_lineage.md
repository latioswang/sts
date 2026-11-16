# S1 — The AlphaGo lineage, and what it implies for a Slay the Spire agent

**How this was researched.** WebSearch and WebFetch were available; papers were read as full text (methods, tables, appendices). OpenReview blocked automated access: the Gumbel paper was read through a text mirror, Stochastic MuZero only at abstract/summary level. Anything not opened is marked `[from memory, unverified]`. `[project log]` = this project's notes (`research/breakthrough/h1_history.md`, `h2_history.md`, `research/program.md`). `[my estimate]` = my arithmetic. Source keys resolve to URLs at the end.

## 1. How each system was built

**AlphaGo Fan [AG].**
- Input: 19×19 hand-designed feature planes (stone colour, liberties, captures, legality, turns since played, ladder features).
- Networks: separate 13-layer conv policy and value nets, plus a linear-softmax rollout policy on small patterns: 24.2% move accuracy at 2 µs, against 57.0% at 3 ms for the policy net.
- Pipeline: (1) supervised policy on ~30M human positions (50 GPUs, 3 weeks); (2) policy-gradient self-play against a pool of earlier versions (50 GPUs, 1 day): >80% wins against the supervised policy and 85% against Pachi at 100k simulations/move, with no search; (3) value net regressed on 30M positions, one per self-play game, each labelled with the result of the RL policy playing the game out (50 GPUs, 1 week). Training on whole human games overfitted (MSE 0.19 train, 0.37 test); one position per game fixed it (0.226 / 0.234).
- Search: asynchronous PUCT MCTS; supervised policy as prior at every expanded node; leaf value = (1−λ)·value net + λ·rollout result, λ = 0.5.
- Play hardware: 48 CPUs + 8 GPUs (single machine); 1,202 CPUs + 176 GPUs (distributed). Result: 5–0 against Fan Hui.

**AlphaGo Lee and Master [AGZ, Methods].** Lee: value net retrained on outcomes of AlphaGo's own search self-play, "iterated several times"; larger nets; 48 TPUs; 4–1 against Lee Sedol. Master: the AlphaGo Zero network and algorithm, still with human initialisation, handcrafted features and rollouts.

**AlphaGo Zero [AGZ].**
- Input: 17 binary planes (8 steps of stone history per colour, colour to move). One residual tower (20 blocks × 256 filters; 40 blocks in the long run) with a policy head (362-way softmax) and a value head.
- Loop: self-play with 1,600-simulation MCTS (~0.4 s); train (p, v) towards (π = root visit distribution, z = game winner); loss = squared error + cross-entropy + L2. Window: last 500k games; new nets gated at >55% over 400 games; no rollouts.
- Compute: 20 blocks: 4.9M games, 700k batches of 2,048, ~3 days, training on 64 GPU workers; 40 blocks: 29M games, 40 days. Play: one machine, 4 TPUs.
- Result: 100–0 against AlphaGo Lee after 72 h. Elo 5,185 against Master 4,858, Lee 3,739, Fan 3,144.

**AlphaZero [AZ].** The same algorithm for chess, shogi and Go. Differences from AlphaGo Zero: value = expected outcome (draws exist), no symmetry augmentation, no gating (one continually updated net generates the games), one hyperparameter set. 800 simulations per move in training; 700k steps × 4,096; 5,000 first-generation TPUs for self-play and 64 for training; chess 9 h / 44M games, shogi 12 h / 24M, Go 34 h / 21M. Play on 4 TPUs at 80k positions/s against Stockfish's 70M. Chess: 28 wins, 72 draws, 0 losses against Stockfish; Go: 60–40 against 3-day AlphaGo Zero.

**MuZero [MZ].** Learns the model: representation, dynamics (next latent state, reward) and prediction (policy, value), unrolled K = 5 steps. Value target: final outcome in board games, n-step bootstrapped return in Atari. MCTS in latent space: 800 simulations (board), 50 (Atari). 16 TPUs training + 1,000 self-play per board game; 8 + 32 per Atari game. Matches AlphaZero on board games; Atari median 2,041% at 20B frames; "Reanalyze" (re-search stored trajectories with the newest net) 731% at 200M frames.

**Stochastic MuZero [SMZ].** Adds afterstates (state after the agent's action, before chance), an afterstate prediction that outputs a value and a distribution over chance outcomes, and chance nodes whose outcome is drawn from that prior. Abstract: matches or exceeds the state of the art in 2048 and backgammon, equals MuZero in Go. `[from memory, unverified]`: chance outcomes are discrete codes from a VQ-VAE-style encoder (codebook 32), and in 2048 it matched an AlphaZero given the true stochastic simulator.

**Gumbel AlphaZero/MuZero [GMZ].** Root: sample m actions without replacement (Gumbel-top-k) from the prior, spend the n simulations by Sequential Halving, act by argmax(g + logits + σ(q)). Policy target: softmax(logits + σ(completedQ)), where unvisited actions receive the value estimate. This is a policy improvement whenever the Q estimates are right. In 9×9 Go plain MuZero "fails to learn from 16 or less simulations"; Gumbel MuZero "learns reliably even with 2"; m = min(n, 16). Agents were evaluated at 800 simulations, so this is evidence about training cost, not about playing at n = 2.

**EfficientZero [EZ].** MuZero + self-supervised latent consistency loss + end-to-end value prefix + off-policy correction. Atari-100k (2 h of play): 194.3% mean, 109.0% median; 4 GPUs × 7 h per game. Ablation (mean): 1.943 full; 0.881 without the consistency loss, the largest drop.

**KataGo [KG, KGm].** The AlphaZero loop plus: playout-cap randomisation; forced playouts with policy-target pruning; global pooling; an auxiliary policy target (the opponent's reply); auxiliary ownership and score-distribution targets; some game-specific input features; a net grown from 6×96 to 20×256. 19 days on ≤28 V100 GPUs (1.4 GPU-years), 4.2M games, 241M samples; it passed ELF OpenGo's final model: "about a factor of 50". Later [KGm]: one net for boards 7×7 to 19×19, short-term value targets (horizons ≈ 6, 16, 50 turns), policy-surprise weighting.

**ELF OpenGo, Leela Zero, Leela Chess Zero.** [ELF]: faithful replication; 2,000 self-play GPUs + 8 training GPUs, 1,600 simulations/move, ~20M games, superhuman in 9 days. [LZ]: community replica; its README puts recomputing the AlphaGo Zero weights at "about 1700 years on commodity hardware". [LC0]: community AlphaZero for chess, now with transformer nets.

## 2. What actually mattered

**(a) The value target was always the outcome of strong play.** In [AG] the value net approximates the value function of the strongest available policy, not of the rollout policy; building it on the RL policy beat building it on the supervised policy. The paper calls the two leaf evaluators "complementary: the value network approximates the outcome of games played by the strong but impractically slow pρ, while the rollouts can precisely score and evaluate the outcome of games played by the weaker but faster rollout policy". One value evaluation approached the accuracy of rollouts played by the RL policy at 15,000 times less computation. Each later step raised the strength of the policy behind the label: Lee used search self-play; AlphaGo Zero used 1,600-simulation self-play and deleted rollouts. No system in the lineage trained a value net to imitate rollouts of a weak policy: that caps the net at the rollout policy's judgment and adds approximation error.

**(b) Components of AlphaGo Fan** (single machine, 5 s/move) [AG, Ext. Data Table 7]:

| Prior in the tree | Leaf evaluation | Elo |
|---|---|---|
| policy net | value net + rollouts | 2890 |
| policy net | rollouts only | 2416 |
| policy net | value net only | 2177 |
| none | value net + rollouts | 2077 |
| none | value net only | 1655 |
| none | rollouts only | 1457 |
| raw policy net, no search | n/a | 1517 |

The prior at every node is the largest single piece (+520 to +960 Elo, depending on the leaf evaluator). With first-generation nets, value-only lost to rollouts-only at equal time; only the mix won.

**(c) AlphaGo Zero ablations [AGZ, Fig. 4].** Residual instead of plain conv: >600 Elo. One dual-headed net instead of two nets: ~600 more. The human-trained net predicted human moves better, yet the self-taught net beat it within 24 h. Dropped without loss: human data, handcrafted features, rollouts, the separate RL-policy stage; AlphaZero then dropped gating and symmetry tricks.

**(d) Raw network against network + search.**

| System | Raw net | With search | Source |
|---|---|---|---|
| AlphaGo Fan | 1517 | 2890 | [AG] |
| AlphaGo Zero (40 blocks) | 3055 | 5185 | [AGZ] |
| AlphaZero chess | 1777 (policy), 1992 (one-ply value) | 2470 at 400 sims | [CWS, Table 1] |
| Leela Chess Zero | 2292, 2418 | 2858 at 400 sims | [CWS, Table 1] |
| 270M-parameter transformer distilled from Stockfish | 2299 | n/a | [CWS, Table 1] |

Search adds roughly 550 to 2,100 Elo. Still, the AlphaGo Zero raw net (3,055) is about as strong as the whole distributed AlphaGo Fan (3,144), and in Atari MuZero with a single simulation "performed well" [MZ].

**(e) Scaling with search.** [AG, Ext. Data Table 8]: 1 → 40 search threads = 2203 → 2890 Elo. [ELF]: even for the final model, doubling rollouts gives ≈ +200 Elo. [MZ, Fig. 3]: Go keeps improving up to 100× the training search time; Atari plateaus near 100 simulations.

**(f) Search is the teacher.** [AGZ] calls MCTS "a powerful policy improvement operator" and the self-play outcome a "policy evaluation operator". [EXIT] is the same loop, and its first teacher is plain rollout MCTS: a net imitating visit distributions is 50 Elo stronger than one imitating the chosen move; MCTS guided by that net wins 97% against unguided MCTS, while doubling the unguided iterations wins 56%. [GRILL]: visit counts approximate a regularised policy optimisation, poorly at low budgets. [HAM]: in MuZero, planning matters most during learning, and shallow trees suffice except in the hardest reasoning tasks.

**(g) Efficiency ablations [KG, Table 2]** (training-time factor when a technique is removed): ownership + score targets 1.65×, global pooling 1.60×, game-specific features 1.55×, playout-cap randomisation 1.37×, auxiliary policy 1.30×, forced playouts and pruning 1.25×; product ≈ 9.1×. Motive for playout caps: the outcome target is "highly data-limited, with only one noisy binary result per entire game", so value learning wants many cheap games and policy learning wants deep searches.

## 3. Cheaper replications

| Work | Compute | What bought the saving |
|---|---|---|
| AlphaZero Go | ≈ 41 TPU-years (estimate in [KG]) | baseline |
| ELF OpenGo [ELF, KG] | 2,000 GPUs, ~2 weeks ≈ 74 GPU-years | faithful replication |
| KataGo [KG, KGr] | 1.4 GPU-years; README: "a few strong GPUs for a few days" reach high amateur dan | §2(g), net grown during training |
| Gumbel MuZero [GMZ] | trains at 2–16 simulations instead of 800 | a correct improvement operator |
| MuZero Reanalyse [REAN] | 126.6% median at 20M frames with 99.5% of targets re-searched | data reuse |
| EfficientZero [EZ] | 4 GPUs × 7 h per Atari game | auxiliary consistency loss |
| Searchless chess [CWS] | supervised only, 15.3B Stockfish-labelled action values | distilling a strong searcher |

A cross-check from chess: the strongest engine, Stockfish, pairs alpha-beta search with "efficiently updatable" neural networks [CWS, intro; AZ], and Leela Chess Zero lost TCEC superfinals 23–25 to it [LC0]. Where an exact, fast simulator makes brute-force search strong, the cost of one network evaluation decides the design.

## 4. Design lessons for a Slay the Spire agent

**Why the learned leaf failed.** `[project log]` The MLP regressed on the mean of 8 random rollouts; the pure leaf and the λ = 0.5 mix both lost. The lineage predicts both. The label was the rollout policy's own evaluation, so the net could only remove variance, which 1,000 simulations already average out. The mix combined two estimates of the same quantity, not AlphaGo's two complementary ones. The table in §2(b) also shows that a first-generation value net losing to rollouts at equal time is normal.

**The budget decides the shape.** `[project log]` A 10k-parameter MLP evaluation costs about half a rollout. So a simulation costs microseconds, and one decision is worth roughly 10–50M multiply-adds `[my estimate]`. Three regimes fit: (A) ~1,000 evaluations of a net of ≤ 20k multiply-adds; (B) 32–64 evaluations of a net of 0.3–1M; (C) one evaluation of a net of ≥ 10M. §2(d) puts C far below search, [GMZ] shows B is learnable, and the chess cross-check in §3 points to A.

**State encoder.** Entities, not 204 summary features: every card with its zone (hand, draw, discard, exhaust), upgrade and current cost; every enemy (identity, HP, block, intent, powers); the player (HP, block, energy, powers); relics, potions; act, floor, ascension, boss. The draw pile enters as an unordered multiset: exactly the legal information, so the encoder cannot leak. Learned embeddings, sum-pooled per zone (KataGo's global pooling), then a small trunk with two heads. A sum of embeddings can be updated incrementally when a card moves, which makes regime A affordable (`[from memory, unverified]`: the accumulator idea behind Stockfish's nets). One net for all ascensions, as KataGo uses one net for all board sizes [KGm].

**Action head.** Score each legal action from (card embedding, target embedding, trunk), plus end-turn and potion actions; softmax over legal actions only, as AlphaZero masks illegal moves [AZ]. Identical cards are one action.

**Value target.** Combat: the result of the fight as actually played by the strongest available agent from that state: death flag and HP lost in bins (KataGo's score distribution), with auxiliary heads for fight length, HP lost over the next 1 and 3 turns, and potions used. The run-level value turns that distribution into the search utility, so the combat net survives changes upstream. Run level: the final result plus dense auxiliaries (floor reached, HP at each boss), because a 0.3% win rate makes the binary label almost constant. Ranked Reward [R2], which scores a result against the agent's own recent percentile, is the single-player substitute for an opponent.

**Training loop.** Expert iteration. Round 0: fights played by the present rollout MCTS at 4–8× budget; this is the analog of human replays, as in [EXIT]. Then train on (root visit distribution or completed-Q policy, realised outcome), replace the searcher with net-guided search, regenerate, retrain; one net, no gating [AZ]. Diversify states as AlphaGo did (one uniformly random action, then strong play, record the next state [AG]) and by perturbing start states (deck, HP, potions): the analog of league opponents is the distribution of decks and encounters. Playout-cap randomisation: full search on ~25% of decisions for policy targets, cheap search elsewhere [KG]. Reanalyse is cheap because the simulator restores any state [REAN].

**Play-time search.** PUCT with the prior at every node (the largest piece in §2(b); today the prior is used at the root only). Value head at leaves; rollouts or a λ-mix only if they still add strength once the value is trained on strong play. Gumbel root with Sequential Halving over ≤ 16 actions when n ≤ 64 [GMZ]. Explicit chance nodes at afterstates (end of turn before the draw, random enemy moves, random card effects), sampled from the true simulator, in place of today's determinisation: Stochastic MuZero's tree without its learned model [SMZ]. An afterstate value is an expectation over the next draw; a rollout samples one draw.

**Other analogs.** Fast rollout policy → the raw policy head, used offline as a cheap whole-run playout engine for strategic decisions; AlphaGo's first value net needed only policy-only self-play [KG, §3.1]. Build-order conditioning → none in this lineage; the nearest is ascension, act boss and floor as inputs.

**What breaks.**
1. Single player: no opponent, hence no automatic curriculum and no balanced outcome. Ascension is the curriculum; dense targets carry the signal.
2. Chance: a deterministic AlphaZero tree is wrong, and outcome labels are noisier.
3. Horizon: one net and one search over a whole run is the pure design, but one run-level simulation contains whole fights. That forces two levels: combat net + search, and a strategic net trained on offline Monte-Carlo with fast policy-only fights.
4. A CPU-only budget of milliseconds where exact brute-force search is already strong: by §3, the regime in which the lineage's large nets did not win.

## 5. Cost to try it here

Smallest decisive experiment: **two rounds of expert iteration on boss fights**, where most A10/A20 deaths occur `[project log]` and where a paired, low-variance metric exists.

1. Freeze 5,000 boss-entry states from A20 runs as the test set; keep a disjoint pool for training.
2. Data: 200k boss fights by the current agent at 4× simulations, logging entity state, root visits and the realised outcome: about 80 CPU-hours, 2 h on ten machines `[my estimate]`.
3. Train two nets on the same ~16M rows: A (sum-pooled embeddings + two small layers), a few CPU-hours; B (0.3–1M multiply-adds), about a day on CPU or under an hour on one GPU `[my estimate]`.
4. Play the test states at equal CPU time per decision: current agent, A-search (~1,000 evaluations), B-search (Gumbel, 32–64 evaluations), and the raw policy.
5. Round 2: regenerate with the better searcher, retrain, test again.

Signals, in order:
- Offline gate: on sibling states scored by 16 continuations at 4× budget, the outcome-trained net must rank better than the mean of 8 rollouts. If not, stop.
- Boss-fight win rate and HP left at equal time, round 1 against the baseline.
- Round 2 above round 1. This is the signature of the lineage; a flat second round means plain distillation.
- Raw-policy strength: if it is within a few points of the current agent, policy-only playouts make run-level planning affordable.

Total: 150–250 CPU-hours and 3–5 days of engineering (entity-state export, in-tree priors, chance nodes); no GPU needed. A full loop over whole runs (10^8–10^9 positions, several rounds, teacher nets of 1–5M parameters) realistically wants 1–4 GPUs for training for one to two weeks, with self-play staying on CPU `[my estimate]`.

Honest prior: `[project log]` a prior at depth 1, prior-guided rollouts and a "large net at 500 sims" have already lost at equal wall-clock. This test differs in the label (strong-play outcome), the representation (entities) and the round-2 check. I put the chance of a keep at equal time at 35–45%.

## 6. Top transfers and dead ends

**Top transfers**
1. **Labels from the strongest available play, iterated**: value = realised outcome, policy = search statistics, one dual-headed net. Confidence: high that this is the core of the lineage, medium that it wins here at the fixed budget.
2. **KataGo's efficiency kit**: outcome-decomposition auxiliary targets, playout-cap randomisation, policy-target pruning, domain input features, one net across ascensions. Confidence: medium-high: each bought 1.25–1.65× in Go, and the first two attack the "one noisy result per game" problem, which is worse here.
3. **Prior at every node, Gumbel root, explicit chance nodes**, sized for 32–64 evaluations. Confidence: medium.
4. **Entity encoder with sum pooling, evaluated incrementally**, in place of summary features. Confidence: medium.
5. **Raw policy as a fast playout engine for the strategic layer**, with dense run-level targets. Confidence: medium-low; it depends on the raw-policy signal in §5.

**Dead ends**
- **Learned dynamics (MuZero, EfficientZero).** The simulator is exact and costs microseconds; a latent model is slower and less accurate. Only Reanalyse carries over.
- **Value nets trained on rollout means, alone or mixed.** Failed here; absent from the lineage for the reason in §2(a).
- **AlphaZero-sized nets at play time.** A 20-block × 256 tower on one CPU core is under one evaluation per decision `[my estimate]`.
- **Raw policy as the player.** 550–2,100 Elo below search in every system measured.
- **A learned fast rollout policy.** AlphaGo Zero deleted rollouts; the project's version cost 2.43× per simulation and lost `[project log]`.
- **Human-data initialisation for combat, gating, symmetry augmentation, leagues.** Each was removed by a successor or has no counterpart in a single-player game.

## Sources (all opened unless noted)

- [AG] Silver et al. 2016, Nature 529 — https://storage.googleapis.com/deepmind-media/alphago/AlphaGoNaturePaper.pdf
- [AGZ] Silver et al. 2017, Nature 550 (author version) — https://discovery.ucl.ac.uk/id/eprint/10045895/1/agz_unformatted_nature.pdf
- [AZ] Silver et al. 2017 — https://arxiv.org/abs/1712.01815
- [MZ] Schrittwieser et al. 2019 — https://arxiv.org/abs/1911.08265
- [SMZ] Antonoglou et al., ICLR 2022 — https://openreview.net/forum?id=X6D9bAHhBQ1 (blocked); read via https://papers.cool/venue/X6D9bAHhBQ1@OpenReview and https://syncedreview.com/2022/07/21/deepmind-ucls-stochastic-muzero-achieves-sota-results-in-complex-stochastic-environments/
- [GMZ] Danihelka et al., ICLR 2022 — https://openreview.net/forum?id=bERaNdoegnO (blocked); full text read at https://object.cloud.sdsc.edu/v1/AUTH_da4962d3368042ac8337e2dfdd3e7bf3/ml-papers-txt/ICLR/2022/policy_improvement_by_planning_with_gumbel__15d5fc26.txt ; code: https://github.com/google-deepmind/mctx
- [EZ] Ye et al. 2021 — https://arxiv.org/abs/2111.00210
- [KG] Wu 2019 — https://arxiv.org/abs/1902.10565 ; [KGm] https://github.com/lightvector/KataGo/blob/master/docs/KataGoMethods.md ; [KGr] https://github.com/lightvector/KataGo
- [ELF] Tian et al. 2019 — https://arxiv.org/abs/1902.04522
- [LZ] https://github.com/leela-zero/leela-zero ; [LC0] https://lczero.org/blog/2024/02/transformer-progress/
- [CWS] Ruoss et al. 2024 — https://arxiv.org/abs/2402.04494
- [EXIT] Anthony et al. 2017 — https://arxiv.org/abs/1705.08439
- [GRILL] Grill et al. 2020 — https://arxiv.org/abs/2007.12509
- [HAM] Hamrick et al. 2021 — https://arxiv.org/abs/2011.04021
- [REAN] Schrittwieser et al. 2021 — https://arxiv.org/abs/2104.06294
- [R2] Laterre et al. 2018 — https://arxiv.org/abs/1807.01672
