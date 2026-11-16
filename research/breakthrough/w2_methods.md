# W2 — Methods: planning and learning for single-player stochastic card games

Web search and fetch worked. Every source [n] was opened by me; "(abstract only)" marks those where only the abstract was readable; other claims are marked [from memory].

Project facts used (from `c1_combat_search.md`): the search is already one shared tree keyed by observations, with hidden state re-sampled every simulation; almost no node below the current turn is revisited; a simulation costs about 6 µs, 87% of it the playout; one dense value-net evaluation costs about 45% of a simulation; prior-guided rollouts lost at 2.43× cost; a decision takes 3–4 ms.

## A. Search with chance and hidden draw order

**A1. Determinisation, strategy fusion, information-set search.**
- What: sample the hidden state, search each sample as perfect information, average. Its errors are strategy fusion (acting differently in worlds one cannot tell apart) and non-locality [1][2].
- Result: on synthetic trees determinisation is worst when sibling leaves are uncorrelated and improves sharply when hidden information is revealed quickly [1]. A single information-set tree (ISMCTS) beat determinised UCT clearly in two of three games, and made no real difference in the card game Dou Di Zhu [2].
- Mechanism and needs: shared statistics, no budget split; code only.
- Transfer: already in place, and StS combat (no adversary, five cards revealed per turn) is determinisation's best case. No step change left. High.

**A2. Chance nodes at scale: sparse sampling, widening, bucketing, open loop.**
- What: sparse sampling plans near-optimally at a cost independent of state count but exponential in horizon [3]; progressive widening caps the children of a chance node [4]; open-loop search keeps statistics per action sequence [5, abstract only; mechanism from memory].
- Result: when chance outcomes are effectively unique, tree search with widening collapses to one particle per node and converges to a policy "suboptimal regardless of the computation time" [4]. In Hearthstone, grouping similar chance outcomes into buckets raised win rate against the strongest bot from 60.1% to 72.3% at 5 s per move [6].
- Mechanism and needs: abstraction makes chance children revisitable; needs a hand abstraction and a value at the turn boundary.
- Transfer: [4] explains this project's dead tree below the turn boundary; bucketed draw nodes are the only variant with a measured card-game gain. Medium. Open loop cannot work across turns because legal actions depend on the draw. Low; high confidence.

**A3. Gumbel root selection with Sequential Halving.**
- What: sample root actions without replacement by prior plus Gumbel noise, allocate simulations by Sequential Halving, train the policy toward completed-Q targets [7].
- Result: MuZero "fails to learn from 16 or less simulations" in 9×9 Go and from 4 or fewer in Ms Pac-Man; the Gumbel version learns reliably with 2 and matches at large budgets [7].
- Mechanism and needs: optimises simple regret at the root and guarantees policy improvement when not every action is visited; needs a prior.
- Transfer: small at play time with 1,000 simulations (medium); large as a data engine: improved-policy labels from 8–32 simulations instead of 1,000, roughly 30–100× more labelled decisions per CPU-hour [my estimate]. KataGo's playout-cap randomisation follows the same economy and is part of its reported 50× compute reduction [8]. High as an enabler.

## B. What to evaluate at the leaf

**B1. AlphaGo: fast rollout policy plus value network.**
- What: a linear-softmax rollout policy on small pattern features and a value network, mixed at the leaf [9].
- Result: rollout policy 24.2% move accuracy at 2 µs per move against 3 ms for the policy network. The value net was trained on 30 million positions, each from a separate self-play game of the strong policy, because using every position of a game overfitted. One evaluation approached the accuracy of strong-policy rollouts "using 15,000 times less computation"; mixing both (λ = 0.5) won ≥95% against either alone [9].
- Mechanism and needs: the value net amortises rollouts of a strong, slow policy; needs outcome labels from strong play, one per game.
- Transfer: this explains the failed leaf value here: its target was the random-rollout mean, so it could only reproduce the playout at similar cost. High confidence in the diagnosis; medium that a retargeted net wins unless it is also far cheaper (B4). At AlphaGo's 2 µs per move, a 15-action playout would cost five times a whole simulation here.

**B2. Simulation balancing.**
- What: fit rollout-policy weights to minimise the error of the Monte-Carlo value against a deep-search value, not to maximise playing strength [10].
- Result (5×5 Go, 100 simulations per move, 107 weights): the policy trained for strength was the best stand-alone player (947 Elo) but gave weaker search (1159) than the balanced one (719 alone, 1367 in search); uniform random scored 1031 [10].
- Mechanism and needs: playouts must be unbiased evaluators, not good players; needs value labels from high-simulation search and a per-card weight table.
- Transfer: a weight table adds no run-time cost (prior-guided rollouts failed on cost), and this objective was not tried. Medium.

**B3. Rollouts as one step of policy improvement over a non-random base.**
- What: score each action by simulating a base policy, optionally truncated and bootstrapped with a value.
- Result: backgammon: Monte-Carlo over a base player cut equity loss 2.5–3.9× (random base 0.330 → 0.131); truncated rollouts scored by the network cut it 3.5–6.6× and were "at least an order of magnitude" faster than full ones [11]. Solitaire with known cards: heuristic 13.05% → 31.20% → 47.60% → 56.83% → 70.20% wins over one to five nesting levels, the last at 1 h 45 min per game [12]. Hanabi: search over a "blueprint" policy lifted every blueprint tested (22.99 → 24.21 of 25) at about 2 core-hours per game [13].
- Mechanism and needs: the gain compounds with base-policy quality; truncation removes steps and variance; needs a base policy or value far cheaper than the playout.
- Transfer: near-random play to the end of the fight is the weakest base in this literature; truncation at the turn boundary with a cheap value is the supported form. High.

**B4. Evaluators built for one CPU core: n-tuple networks and NNUE.**
- What: value functions whose first layer is a sparse table lookup, updated incrementally as the state changes.
- Result: 2048, a single-player stochastic game: a TD-trained n-tuple afterstate value plays greedily at 258,371 moves per second scoring 324,710; with expectimax it scores 527,099 at 1 ms per move and 609,104 at 1 s, the best program of its time, single-threaded [14]. Chess: NNUE has about 0.1% of inputs active, updates its accumulator per move in int8/int16 arithmetic, and targets "million(s) of evaluations per second per thread" [15]; it added +92.77 ± 2.1 Elo over a hand-written evaluation after training on search evaluations of millions of positions [16].
- Mechanism and needs: cost scales with what changed, so capacity is nearly free; needs sparse features (card × pile, enemy × intent, bucketed HP and block) and tens of millions of search-labelled states.
- Transfer: the 2048 result is at this project's time scale; the design removes the cost that killed the dense net and supplies the card identities it lacked. Medium-high.

## C. Learning loops

**C1. Expert iteration and AlphaZero targets.**
- What: search generates targets, a network learns them, the network then guides search [17].
- Result: in Hex, learning the tree's visit distribution rather than the chosen move gave +50 ± 13 Elo at equal accuracy, and search with the policy net beat plain MCTS 97% [17]. KataGo reports that auxiliary targets "noticeably improve" learning [8].
- Value target: the realised outcome of search-guided play (AlphaGo Zero) or of the network policy's own play (this paper) [17]. A rollout mean caps the value at the rollout policy's judgement (B1). Root search values as targets trade variance for bias [from memory, unverified].
- Mechanism and needs: each round raises the quality of data and search; needs self-play volume, hence cheap search (A3). MuZero ablations agree that planning pays mostly during learning [21].
- Transfer: use the prior below the root, train the value on realised results of search-guided fights, add auxiliary heads (HP lost, turns). Medium-high.

**C2. Stochastic and Sampled MuZero.**
- What and result: a learned model with afterstates and chance codes matched or exceeded prior results in 2048 and backgammon [18, abstract only]. Sampled MuZero plans over K sampled actions; K = 50 closely approaches full search over 362 actions in Go [19]. Both need accelerator-scale training.
- Transfer: the simulator is exact and faster than any learned model. Low; high confidence. Action sampling matters only for turn-level search.

**C3. Search-free policies distilled from search.**
- What: supervised transformers predicting search action-values, played greedily [20].
- Result: trained on 15.3 billion action-values: 9M parameters reach 2054 Lichess Elo against bots; 270M reach 2299 against bots and 2895 blitz against humans; the 50 ms search teacher has 2713 against bots. Action-value targets beat state-value targets and behavioural cloning [20].
- Mechanism and needs: scale; the student stays below the teacher; needs accelerators.
- Transfer: not as a replacement for search. The design rule — label every legal action with a value — does transfer. High.

## D. Card games and roguelikes

**D1. Hearthstone.**
- Result: MCTS with a 750-input value network for early termination and a heuristic simulation policy raised second-player win rate against plain MCTS from 26.5% to 56.1% at 0.5 s per move [22]. Bucketing plus a learned high-level rollout policy won 53.4% and 62.6% (two decks) against the search without them [6]. End-to-end self-play RL beat a top-10-ranked human in best-of-five, using 8 GPUs per training run [23].
- Mechanism: a value cut-off and informed playouts in the first two; scale in the third.
- Transfer: the first two are in this project's compute class and agree with B1–B3 (medium-high); the third is out of reach on CPU.

**D2. Legends of Code and Magic.**
- What and result: one network plays both draft and battle; it won both 2022 competition tracks (84.41% average win rate) after 72 hours on 24 units of one V100 plus 600 cores. MCTS in training at equal resources scored 36–56% against the no-search baseline and was dropped [24].
- Mechanism: deck choices receive the final-outcome gradient directly.
- Transfer: low at CPU scale; medium confidence.

**D3. Dominion, Magic drafting, Balatro.**
- Result: Provincial evolves "buy menus" over a few million games and sweeps heuristic bots; a two-layer DQN that only chooses purchases (tactics by heuristics), about 7,000 self-play games in 45 GPU-minutes, beats or ties it about two-thirds of the time [25]. A network imitating 100,000 human drafts reaches 64.7% top-one accuracy [26]. For Balatro I found only language-model leaderboards [27].
- Mechanism and needs: the build layer learns against the true objective because tactics are cheap; needs a low-cost combat proxy.
- Transfer: medium.

**D4. NetHack.**
- Result: in the NeurIPS 2021 challenge the best symbolic bot's median score was nearly three times the best neural agent's; the winner, a behaviour tree over a hand-parsed state, had a median of 5,300. Stated reasons: explicit strategy subroutines, encoded game knowledge and long-range dependencies, while hierarchical RL and credit assignment remain open [28]. Imitating that bot with hierarchy labels and large transformers raised neural results by 127%, yet "mere scaling is insufficient" [29].
- Mechanism and needs: hand-written knowledge and hierarchy; expert effort, not compute.
- Transfer: on long roguelikes under small compute, structured knowledge beats end-to-end learning; keep the knowledge-based strategic layer and improve what it knows. High.

**D5. Slay the Spire itself** (see `w1_sts_prior_work.md`). Language-model agents with no search reach 17% and 20% at Ascension 20 Heart, 15 runs per character [30]; a public project reports six failed attempts to distil combat search into a network [31]. Both locate the gap in the strategic layer. Medium.

## E. Long-horizon credit assignment

**E1. Suphx (Mahjong).**
- What: (i) a learned predictor spreads the final game reward over rounds; (ii) oracle guiding trains with perfect-information features, then removes them; (iii) run-time policy adaptation on 100,000 simulated trajectories per deal [32].
- Result: (i) and (ii) each added rank; (iii) won 66% against the unadapted agent but was too slow for live play; 44 GPUs for two days per agent [32].
- Transfer: the run-value network already plays role (i). A critic that sees the draw order in training only is legitimate: medium-low. Per-fight adaptation: low.

**E2. Run-level rollouts as labels** (my synthesis). At a card, path or shop decision, clone the run, take each option, re-sample all hidden randomness, finish the run many times with a cheap agent, and regress an action-value. This is [11] applied to the investment layer, producing the label type [20] found best. Offline only; needs a combat proxy about 100× cheaper than the current search. Medium-high.

## Top transfers

1. **A microsecond value at the turn boundary** (B4, B1, B3): a sparse incremental evaluator trained on realised outcomes of search-guided fights (one sample per fight, auxiliary heads), with the playout truncated there.
2. **Run-level action-value labels for the strategic layer** (E2, D3, D4, D5): the Ascension 20 Heart gap appears to be here; item 1 supplies the cheap combat proxy.
3. **Cheap-search expert iteration** (A3, C1): Sequential-Halving root, completed-Q targets, playout-cap randomisation; prior used below the root.
4. **Balance-trained playout weights** (B2): a per-card weight table fitted to high-simulation values; no run-time cost.
5. **Bucketed draw nodes** (A2): revisitable chance children plus the value from item 1 give a real two-turn expectimax.

## Dead ends

- **Learned world models** (C2): the simulator is exact and faster.
- **Search-free combat policy** (C3): needs accelerator scale and still trails its teacher.
- **End-to-end model-free RL** at the scale of [23], [24], [32] or DouZero [33]: tens of GPU-days or more; out of reach on CPU.
- **Information-set search or open loop as the breakthrough** (A1, A2): already in place or inapplicable.
- **A bigger dense leaf network at the current cost and target** (B1): cost and the random-play target are the limits, not data volume.
- **Deep nested rollouts or per-fight policy adaptation at decision time** (B3, E1): exponential cost per level [12] or about 100,000 trajectories [32]; both break fixed compute.

## Sources

[1] Long 2010: https://cdn.aaai.org/ojs/7562/7562-13-11092-1-2-20201228.pdf
[2] Cowling 2012: https://eprints.whiterose.ac.uk/75048/1/CowlingPowleyWhitehouse2012.pdf
[3] Kearns 2002: https://www.cis.upenn.edu/~mkearns/papers/sparsesampling-journal.pdf
[4] Sunberg 2018: https://arxiv.org/pdf/1709.06196
[5] Perez Liebana 2015: https://repository.essex.ac.uk/14734/
[6] Zhang 2017: https://skatgame.net/mburo/ps/cig17-hsai.pdf
[7] Danihelka 2022 (text copy): https://object.cloud.sdsc.edu/v1/AUTH_da4962d3368042ac8337e2dfdd3e7bf3/ml-papers-txt/ICLR/2022/policy_improvement_by_planning_with_gumbel__15d5fc26.txt
[8] Wu 2020: https://arxiv.org/pdf/1902.10565
[9] Silver 2016: https://storage.googleapis.com/deepmind-media/alphago/AlphaGoNaturePaper.pdf
[10] Silver 2009: https://icml.cc/Conferences/2009/papers/500.pdf
[11] Tesauro 1996: https://arxiv.org/pdf/2501.05407
[12] Yan 2004: https://papers.nips.cc/paper_files/paper/2004/file/48c3ec5c3a93a9e294a8a6392ccedeb4-Paper.pdf
[13] Lerer 2020: https://arxiv.org/pdf/1912.02318
[14] Jaśkowski 2016: https://arxiv.org/pdf/1604.05085
[15] NNUE documentation: https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md
[16] Stockfish blog 2020: https://stockfishchess.org/blog/2020/introducing-nnue-evaluation/
[17] Anthony 2017: https://arxiv.org/pdf/1705.08439
[18] Antonoglou 2022: https://iclr.cc/virtual/2022/poster/6832
[19] Hubert 2021: https://arxiv.org/pdf/2104.06303
[20] Ruoss 2024: https://arxiv.org/pdf/2402.04494
[21] Hamrick 2021: https://arxiv.org/pdf/2011.04021
[22] Świechowski 2018: https://arxiv.org/pdf/1808.04794
[23] Xiao 2023: https://arxiv.org/pdf/2303.05197
[24] Xi 2023: https://arxiv.org/pdf/2303.04096
[25] Halawi 2024: https://arxiv.org/pdf/2405.06846
[26] Ward 2021: https://arxiv.org/pdf/2009.00655
[27] BalatroBench report: https://gigazine.net/gsc_news/en/20260213-balatrobench
[28] Hambro 2022: https://arxiv.org/pdf/2203.11889
[29] Piterbarg 2023: https://arxiv.org/pdf/2305.19240
[30] Epoch AI 2026: https://epoch.ai/publications/ai-systems-are-improving-rapidly-at-slay-the-spire-and-its-sequel
[31] Public agent write-up: https://huggingface.co/Jialeiv/sts-rl-agent
[32] Li 2020: https://arxiv.org/pdf/2003.13590
[33] Zha 2021: https://arxiv.org/pdf/2106.06135
