# W3 — Engineering practice and automated discovery (stage 1, 2026-11-16)

Tools: WebSearch and WebFetch were available. Every URL below was opened in this session; claims without one are marked `[snippet]` (search result only), `[from memory, unverified]` or `[inferred]`. Project facts come from the sibling files `c1_combat_search.md` (c1) and `h1`-`h6_history.md` (h1-h6).

## 1. NNUE: efficiently updatable evaluation (chess, shogi)

**What.** A shallow network whose large first layer reads sparse binary features and is updated incrementally as moves are made; introduced for shogi by Yu Nasu in 2018, merged into Stockfish in August 2020 (https://www.chessprogramming.org/NNUE).

**Result.** Fishtest at merge: +92.77 ± 2.1 Elo (1 thread, 60,000 games) and +89.47 ± 2.0 (8 threads) (https://stockfishchess.org/blog/2020/introducing-nnue-evaluation/). The progression table shows the merge commit alone moving master from +25.5 to +83.4 Elo over Stockfish 11, where ordinary steps are 1-5 Elo (https://official-stockfish.github.io/docs/stockfish-wiki/Regression-Tests.html). This came "despite the approximately halved search speed" (https://www.chessprogramming.org/Stockfish_NNUE).

**Mechanism.** Three parts, all needed:
- *Sparse entity features.* HalfKP has 40,960 features per side (king square × piece square × piece type × colour), about 30 active; an ordinary move changes a handful, a king move forces a refresh. The first-layer output ("accumulator") is kept on the search stack and patched by adding or subtracting a few weight columns; only the tiny dense tail (256×2→32→32→1) is computed per evaluation (https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md).
- *Integer inference.* int8/int16 weights and SIMD, aimed at "million(s) of evaluations per second per thread" (same document). Stockfish's own data: disabling the incremental update roughly halves speed and costs −67.6 ± 9.5 Elo (https://official-stockfish.github.io/docs/stockfish-wiki/Useful-data.html).
- *Targets from search, not from the old evaluation.* Trained on "evaluations of millions of positions at moderate search depth" (blog above); from Stockfish 14, on billions of positions evaluated by Leela Chess Zero, a far stronger evaluator (https://stockfishchess.org/blog/2021/stockfish-14/).

**Needs.** CPU only at play time; hundreds of millions to billions of labelled positions; a trainer (Stockfish uses a GPU trainer, but the net is small).

**Transfer.** High relevance. The failed leaf network here is the opposite on all three parts: 204 dense aggregate features with no card identities (c1 §3), about 45% of a simulation per evaluation (c1 §4), and a target equal to the mean of 8 random rollouts, so its ceiling was the rollout's own judgment (h1). An NNUE-shaped network (one feature per card × pile × upgrade, relic, monster × intent; accumulator patched when a card moves; integer tail) should cost a few percent of a rollout [inferred]. Cheapness: high confidence. Net strength gain: medium; it depends on the target (item 9).

## 2. Cheap learned playout policies and mixed leaf evaluation (Go, before and in AlphaGo)

**What.** Replace near-uniform playouts with a very cheap learned move distribution; later, mix playout results with a value network.

**Result.** Coulom's Crazy Stone, one CPU, 15,500 simulations/s on 9×9: win rate against GNU Go rose from 38.2% to 68.2% by using learned patterns in simulations, and to 90.6% with patterns also pruning the tree (19×19: 0% → 37.5% with both) (https://www.remi-coulom.fr/Amsterdam2007/MMGoPatterns.pdf). AlphaGo's fast rollout policy was a linear softmax reaching 24.2% move-prediction accuracy at 2 µs per move; mixed evaluation (λ = 0.5) won ≥95% of games against variants using only the value network or only rollouts; tournament Elo 2890 mixed, 2416 rollouts only, 2177 value only (https://www.cs.princeton.edu/courses/archive/spring16/cos598F/Google-go-nature16.pdf, text and Extended Data Table 7).

**Mechanism.** Playouts stay cheap because "only light-weight features are kept" (Coulom §4.1); weights are learned from game records (652 games sufficed). Value network and rollouts were complementary because the value net was trained on outcomes of strong self-play, not on rollout results.

**Needs.** Logged decisions of the strong agent; a per-action weight that is a table lookup.

**Transfer.** High relevance. Playouts are 87% of a simulation at roughly 0.4 µs per step (c1 §4), and random play cannot win boss fights (c1 §6.2). The one attempt at guided rollouts used the MLP prior, cost 2.43× per simulation and lost (h5): a failure of cost, not concept. The mixed leaf here gained nothing because its value was trained on rollout means (h1). Caveat: a playout policy that predicts moves better does not always make search stronger [from memory, unverified], so weights must be tuned on playing strength. Confidence: medium-high.

## 3. Fishtest: sequential testing and the accumulation of small patches

**What.** A distributed queue where every Stockfish patch plays tens of thousands of games against master and is accepted or rejected by a sequential test (https://github.com/official-stockfish/fishtest).

**Result.** Roughly 80 Elo per year for five years (Stockfish 14 post above). The Stockfish 9 cycle gained about +52 Elo in 13 months through steps of mostly 2-8 Elo, each measured over 40,000 games (regression-tests page).

**Mechanism.** A generalised sequential probability ratio test stops as soon as the evidence suffices; results are scored on game *pairs* with a five-outcome model that "leads to a substantial saving of testing resources"; bounds are in normalised Elo (https://official-stockfish.github.io/docs/fishtest-wiki/Fishtest-Mathematics.html). Discipline: a short-time-control test, then a long one; one idea per test; never stop a test early (https://official-stockfish.github.io/docs/fishtest-wiki/Creating-my-first-test.html). Recent logs show bounds of 0-2 Elo `[snippet]`. Speed is priced: about 2.1 Elo per 1% speed-up (useful-data page).

**Needs.** A cheap fixed test and many games.

**Transfer.** Largely in place: paired seeds, pre-registration, P-values over 1,000-8,000 seeds (h2, h3). A sequential stop would cut test cost [inferred]; it is hygiene, not a breakthrough. The lesson is resolution: Stockfish resolves 1-2 Elo routinely, while here +0.004-0.008 needs 4,000-8,000 paired seeds (h3). Confidence: high for efficiency, low for a step change.

## 4. Parameter tuning: SPSA, Texel, CLOP, Bayesian optimisation

**What and mechanism.**
- *SPSA*: perturb all parameters at once by ±c, play one game pair θ+ against θ−, move every parameter in the winning direction. Cost per step does not grow with the number of parameters; Stockfish moved from CLOP to SPSA in 2014 (https://www.chessprogramming.org/SPSA).
- *Texel tuning*: fit the evaluation's parameters by logistic regression so that a sigmoid of the score predicts the game result; no games are played during fitting (https://www.chessprogramming.org/Texel%27s_Tuning_Method).
- *CLOP*: local quadratic regression on noisy win/loss outcomes (https://www.remi-coulom.fr/CLOP/). *Bayesian optimisation* was used repeatedly on AlphaGo's search hyper-parameters (https://arxiv.org/abs/1812.06855).

**Result.** Texel: about 400 parameters fitted on 8.8 million positions from 64,000 games, a local minimum in up to 6 hours on 16 cores, +99.6 Elo cumulative; the author estimates CLOP would need "at least ten years" for the same job. AlphaGo: self-play win rate 50% → 66.5% from tuning before the Lee Sedol match.

**Needs.** SPSA: thousands of game pairs. Texel: logged positions with outcomes. CLOP and Bayesian optimisation: few parameters (about ten or fewer) [from memory, unverified].

**Transfer.** Good fit and cheap. The terminal value, PUCT constant, early-stop fraction, potion values and knowledge-base weights are dozens of numbers (c1 §5), tuned one at a time [inferred]. SPSA with paired seeds can move them jointly; a Texel-style fit of the terminal value on logged fight states against real outcomes costs nothing at play time. Wins at 8% are a weak signal, so the objective must be the continuous score. Expect a few points, not a step change. Confidence: medium.

## 5. Paired seeds and noisy black-box optimisation

**What.** Compare candidates on identical random scenarios. PEGASUS fixes start states and random sequences so a noisy objective becomes deterministic; paired tests improve optimisers, but fixed scenarios cause "overfitting", and adapting the number of trials per comparison speeds learning (https://www.jmlr.org/papers/v3/strens02a.html). Evolution strategies scale to over a thousand CPU workers that exchange only scalars via shared random seeds (https://arxiv.org/abs/1703.03864). CMA-ES targets non-linear, non-convex continuous problems (https://arxiv.org/abs/1604.00772).

**Transfer.** Paired seeds are already standard here. Two additions: rotate seed sets to avoid overfitting, and allocate seeds adaptively. CMA-ES or evolution strategies suit the playout-policy table of item 2 (hundreds of weights, 40 cores). Confidence: medium.

## 6. Program discovery with language models and evolutionary search

**What.** A language model proposes code edits, an automatic evaluator scores them, and an evolutionary database keeps diverse good programs.

**Result and cost.**
- *FunSearch*: evolves one function inside a human-written skeleton; record cap sets and better online bin-packing heuristics (https://deepmind.google/discover/blog/funsearch-making-new-discoveries-in-mathematical-sciences-using-large-language-models/).
- *AlphaEvolve*: evolves whole files of hundreds of lines; 4×4 complex matrix multiplication in 48 multiplications; a data-centre scheduling heuristic recovering 0.7% of fleet compute; 23% kernel speed-up. Its own comparison: FunSearch used "millions of LLM samples" on 10-20-line functions with evaluations of at most 20 minutes on one CPU; AlphaEvolve needs "thousands" with stronger models, rich feedback and an *evaluation cascade* (cheap tests first) (https://arxiv.org/html/2506.13131).
- *Eureka*: reward code for robot learning; beats human rewards on 83% of 29 tasks, +52% on average, with 16 candidates per iteration, each scored by a GPU-simulated training run; removing the feedback summary costs 28.6% (https://arxiv.org/html/2310.12931).
- *Sample efficiency*: ShinkaEvolve reports a state-of-the-art circle packing in 150 samples (https://arxiv.org/abs/2509.19349).
- *Games*: evidence is thin. A model-rewritten value function for a limited tree search in 2048 improved by 473 points per cycle over 30 cycles of 10 games, from a weak start (https://arxiv.org/html/2510.20205v1). In 3v3 capture-the-flag, evolved strategy code won the hardware round of a 2026 competition; the paper credits deep evaluation replacing noisy few-game scores and adding champions to the opponent pool (https://arxiv.org/abs/2606.10389).

**Mechanism.** The evaluator removes wrong ideas; the skeleton confines search to where ideas matter; diversity avoids early convergence.

**Needs.** A fast, automatic, low-noise score; thousands of model calls; no GPU.

**Transfer.** The pieces fit: a simulator at 1.6-2.6 million actions per second per core (c1 §4) and code-shaped targets (playout policy, terminal value, card-pick and path heuristics). The obstacle is noise: a full-run score needs thousands of seeds, so a cascade is required (fixed fight suites with paired seeds first, full runs for survivors). This automates the patch stream of item 3. Offline use respects the compute rule; generated code must be checked to read only observable state. Confidence: medium for a steady stream of gains, low-medium for a step change by itself.

## 7. Population-based training

**What.** A population trains in parallel; weak members copy weights and hyper-parameters from strong ones and perturb them, discovering schedules; gains reported in deep reinforcement learning, translation and GANs (https://arxiv.org/abs/1711.09846).

**Transfer.** Low. It optimises long neural training runs; this project's networks train in minutes and are not the bottleneck. Confidence: high that it does not transfer.

## 8. Fast simulators and batched environments

**What and result.** EnvPool runs environments in a C++ thread pool: 1 million Atari frames/s on a large server, 2.8× Python subprocesses on a laptop (https://arxiv.org/abs/2206.10558). PufferLib's C environments each exceed 1 million steps/s on one core (https://rlj.cs.umass.edu/2025/papers/RLJ_RLC_2025_151.pdf).

**Mechanism.** Native code, no allocation in the step, shared memory, batching sized to feed a GPU learner.

**Transfer.** Already achieved: the simulator is in that speed class, and profiling found no hotspot (c1 §4). Batching pays only when a GPU consumes the batch. Low; confidence high.

## 9. Distilling big-budget search into small-budget play

**What.** Use search as the teacher of the cheap parts. Expert Iteration: tree search plans, a network generalises the plans and then guides the search (https://arxiv.org/abs/1705.08439).

**Result and mechanism.**
- *KataGo* reached top strength with about 50× less compute than comparable systems. Measured factors: playout-cap randomisation 1.37× (a full search of 600 nodes on 25% of turns, recorded for training; 100 nodes otherwise, to finish more games), auxiliary targets such as final ownership and score 1.65×, policy-target pruning 1.25×; product about 9.1× (https://arxiv.org/pdf/1902.10565).
- *Searchless chess*: a 270-million-parameter transformer trained on 15.3 billion action values (about 8,864 Stockfish-days at 50 ms each) reached tournament Elo 2299 against 2711 for its teacher; a 9-million model reached 2025. With equal label counts, action-value and state-value targets tied (https://arxiv.org/html/2402.04494v2).
- *Reanalyse*: rerun search on stored positions to refresh targets (https://arxiv.org/abs/2104.06294).
- *Gumbel AlphaZero*: sampling root actions without replacement guarantees policy improvement and "significantly improve[s] prior performance when planning with few simulations" (https://iclr.cc/virtual/2022/poster/6418; https://github.com/google-deepmind/mctx).

**Needs.** Offline search compute (unlimited here), storage, a student with enough capacity.

**Transfer.** The principle is the unexploited core: no leaf target from real fight outcomes or deeper search was tried (h1), and the prior is distilled at the root only (c1 §3). Useful specifics: label states with the outcome of the 4×-budget agent, add auxiliary targets (HP lost, turns to kill, potions used), record all root action values, refresh old states by reanalysis. Not transferable: a searchless policy. Small students lose hundreds of Elo to their teacher, and 50-200-simulation agents here collapsed (h5). Confidence: medium-high for value and playout targets, low for replacing search.

## 10. Where engines spend a fixed thinking budget

**What and result.**
- *Chess*: extend when the best move keeps changing or the score drops; stop early on forced or obvious moves (https://www.chessprogramming.org/Time_Management). Stockfish 16.1 added a small second network for easily decided positions (https://stockfishchess.org/blog/2024/stockfish-16-1/).
- *Tree search with rollouts*: Baier and Winands tested early stop (STOP), extend-when-close (CLOSE) and extend-when-unstable (UNST) in five games. In 13×13 Go, STOP raised the win rate against GNU Go from 25.5% to 39.1% at equal total time and scored 63.7% against the previous best scheme; fixed time per move needed 63% more time for equal strength (https://dke.maastrichtuniversity.nl/m.winands/documents/time_management_for_monte_carlo_tree_search.pdf).
- *Learned stopping*: 2.5× speed-up at equal strength, or 61% wins at equal average simulations (https://arxiv.org/abs/2012.07910); a virtual-expansion rule used under 50% of the search time in 9×9 Go at 71% against 75% wins (https://ar5iv.labs.arxiv.org/html/2210.12628).

**Mechanism.** Stop decided searches, spend the saving on undecided ones.

**Transfer.** Mostly harvested. The soft early stop with reinvestment is the STOP strategy and gave +0.015 to +0.05 (h1); moving search to boss fights failed repeatedly (h1, h3, h5). What remains is CLOSE/UNST-style extension and a learned stop; in the Go tables CLOSE and UNST alone gave about half of STOP's gain (+7-8 against +13.6 points) and were not shown to add to it. Confidence: medium that little is left.

## Top transfers

1. **A learned playout policy that is a table lookup** (item 2). Per-card, per-context weights distilled from the search's own decisions, then tuned on strength with SPSA or CMA-ES under paired seeds. It attacks the stated worst weakness at zero added cost per step; the precedent is 38% → 68% on one CPU.
2. **The full NNUE recipe for the leaf** (item 1): sparse entity features, incremental accumulator, integer inference, targets from strong search. It then mixes with rollouts as in AlphaGo, where the mix beat either part by 470-710 Elo.
3. **Strong-search targets as the data engine** (item 9): outcomes and action values from a 4×-16× budget agent, auxiliary targets, reanalysis, playout-cap randomisation. Items 1 and 2 depend on it.
4. **Language-model evolutionary search over code-shaped parts with a cascaded, paired-seed evaluator** (item 6): automates the patch stream; best aimed at the playout policy and terminal value.
5. **Joint SPSA and Texel-style tuning of all numeric parameters under sequential paired tests** (items 3-5): reliable small gains and cheaper testing.

## Dead ends

- **More time management or fight-aware reallocation**: the best published strategy is already in use, and reallocation failed repeatedly.
- **Population-based training, vectorised environments, GPU-style batching**: they solve bottlenecks this project lacks.
- **Searchless policies or large transformers**: hundreds of millions of parameters, billions of labels, still below the teacher, and too slow for 3-4 ms on one core.
- **A leaf value trained on rollout results, alone or mixed with rollouts**: no information beyond the rollouts; measured as a tie.
- **A network prior inside rollouts**: 2.43× cost per simulation was fatal; only lookup-cost policies qualify.
- **CLOP or Bayesian optimisation over many parameters**: poor scaling past about ten dimensions [from memory, unverified]; SPSA scales.
- **Language-model rewriting judged on a handful of games**: selection follows noise; the capture-the-flag work needed deep evaluation to progress.
- **Sequential testing as a breakthrough**: it saves test compute and finds nothing new.
