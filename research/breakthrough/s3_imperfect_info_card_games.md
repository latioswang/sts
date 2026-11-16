# S3 — Imperfect-information card and tile games: what transfers to a Slay the Spire agent

Written 2026-11-16. Web search and page fetch were available and used. Marking: every external claim carries the URL I opened. **[snippet]** = seen only in a search-result snippet, page not opened. **[from memory, unverified]** = not checked. **[project]** = this repo's notes (`c1_combat_search.md`, `s5_small_compute_rl.md`, `h*_history.md`). **[estimate]** = my arithmetic.

**Bottom line.** The strongest card-game agents built on modest hardware (DouZero and its successors) use no search at play time. They learn Q(state, action) from the final outcome of their own play, with raw card-count inputs and the action fed in as features. Hidden information used only in training buys sample efficiency and a few points of win rate, not a new ceiling. Where search is added on top of a learned policy (Hanabi, bridge, Skat), it adds a further, smaller gain. The game-theoretic machinery of poker and Stratego does not apply to a single-player game.

## 1. How each system was built

**DouZero** (DouDizhu) — https://ar5iv.labs.arxiv.org/html/2106.06135
- Input: card multisets as 4×15 count matrices flattened to 54 values (own hand, union of unseen cards, last move, each player's played cards, cards left, bombs), plus the last 15 moves through an LSTM. No hand-made strategy features.
- Action: the move's own 54-value card matrix is concatenated to the state; the net outputs one Q(s,a) and is run once per legal move (27,472 possible combinations, so no fixed output head).
- Net: LSTM + 6-layer MLP, width 512, one per seat.
- Training: Deep Monte-Carlo (DMC): every-visit Monte-Carlo return, γ=1, reward only at the end, MSE, ε-greedy 0.01, self-play from scratch, no human data.
- Compute: one server, 48 cores + 4 GTX 1080 Ti, 45 actors, 30 days. Passed the supervised agent at ~5×10⁸ steps (2 days) and the search-based DeltaDou at ~5×10⁹ (10 days).
- Result: 0.586 win rate against DeltaDou; first of 344 bots on Botzone; one forward pass per move.
- Its predecessor DeltaDou was AlphaZero-like: tree search over sampled hidden hands with a learned inference step, about two months of training [snippet: https://www.ijcai.org/proceedings/2019/176].

**PerfectDou** — https://ar5iv.labs.arxiv.org/html/2203.16406
- "Perfect-training-imperfect-execution": PPO with GAE. The policy sees 23×12×15 card planes + 6 scalars. The value net also sees both opponents' hands and their minimum number of moves to empty the hand (25×12×15 + 8).
- Oracle shaping reward: the change in the "moves to go out" gap between the sides, computed from all hands; removed in a final fine-tune on the real score.
- Action: 27,472 moves grouped into 621 classes; each legal action gets a feature vector (12×15 card matrix + 7 scalars) concatenated to the LSTM state code, then an MLP [256,256,256,512] and a softmax over legal actions.
- Compute: 880 CPU cores + 8 GPUs, 2.5×10⁹ steps.
- Result: 0.543 win rate against DouZero; at 10⁹ steps it edges a DouZero trained for 10¹⁰ (0.524).

**Suphx** (Riichi Mahjong) — https://ar5iv.labs.arxiv.org/html/2003.13590, https://arxiv.org/pdf/2003.13590
- Input: 34-wide tile planes, scalars as constant planes, plus 100+ hand-made look-ahead features. Deep residual CNNs.
- Action: five separate models (discard with 34 outputs; riichi, chow, pong, kong binary).
- Training: supervised on professional logs (4–15M samples per model), then distributed self-play policy gradient with adaptive entropy regularisation, plus three additions.
  - Global reward prediction: a 2-layer GRU, trained on human logs, predicts the final game reward from round summaries; round k's reward is Φ(k) − Φ(k−1).
  - Oracle guiding: the agent starts with perfect-information features (other hands, the wall) multiplied by a random mask whose keep-probability decays from 1 to 0. Afterwards the learning rate drops to a tenth and samples with large importance weights are rejected; without that, further training was unstable. Plain distillation from the oracle "does not work well".
  - Run-time policy adaptation: at the start of a round, sample the hidden tiles, roll out with the offline policy, fine-tune on those rollouts, play the round with the adapted policy. The adapted agent won 66% against the unadapted one, but it was not used on the ladder because of time limits.
- Compute: 44 GPUs for 2 days and 1.5M games per agent (2.5M for the final one).
- Result: 10 dan, stable rank 8.74 over 5,760 games, above 99.99% of ranked players. No search.

**Poker**
- DeepStack — https://ar5iv.labs.arxiv.org/html/1701.01724. Re-solves at every decision with a lookahead four actions deep; leaf values come from a 7×500 MLP that takes pot size and both players' hand distributions (1,000 buckets) and returns a value per hand. Training data: 10M random turn situations solved offline (175 core-years) and 1M flop situations (half a GPU-year). Play: one GTX 1080, ~3 s per action. +486 mbb/game (thousandths of a big blind per hand) against 33 professionals.
- Libratus — https://noambrown.com/papers/17-Science-Superhuman.pdf. Abstraction, a self-play blueprint, nested subgame solving, a self-improver. +147 mbb/game over 120,000 hands.
- Pluribus — https://noambrown.com/papers/19-Science-Superhuman.pdf. No neural network. Blueprint in 8 days on 64 cores (12,400 core-hours, about $144); play on two CPUs at 20 s per hand. At the leaves of its depth-limited search each player picks among k=4 continuation strategies (the blueprint and versions biased to fold, call, raise). 48 mbb/game against elite professionals.
- ReBeL — https://ar5iv.labs.arxiv.org/html/2007.13544. AlphaZero-style self-play on public belief states. A 6×1536 MLP maps (public state, two belief vectors of 1,326) to a value per private hand; subgames run to the end of the betting round; the solved root values are the training targets. Data generation used 90 eight-GPU machines. 165±69 mbb/game against a top professional.

**Student of Games** — https://ar5iv.labs.arxiv.org/html/2112.03178. One algorithm (tree-growing regret minimisation with a value-and-policy net over belief states) for chess, Go, poker and Scotland Yard. It is weaker than each specialist: +420 relative Elo against AlphaZero's +592 in chess; 7±3 mbb against the benchmark bot Slumbot, where ReBeL has 45±5.

**DeepNash** (Stratego) — https://ar5iv.labs.arxiv.org/html/2206.15378. No search and no belief model. 10×10×82 observation including the last 40 moves; factored heads (deployment, piece to move, destination) and one value head. Trained by R-NaD, a regularised self-play dynamic; low-probability actions are cut (threshold 0.03 in fine-tuning). 768 learner and 256 actor TPU nodes. 84% against humans on Gravon, at least 97% against bots.

**Hanabi**
- SPARTA — https://ar5iv.labs.arxiv.org/html/1912.02318. Any fixed "blueprint" policy plus one-ply search: keep an exact belief over hidden cards, sample states from it, and for each legal action roll the game out to the end with the blueprint. At least 100 rollouts per action; drop actions more than 2 standard deviations below the best; deviate from the blueprint only when the gain exceeds 0.05. Scores out of 25: 22.99 → 24.21 on a rule bot, 24.08 → 24.53 on the best learned policy. About 2 core-hours per game.
- Learned Belief Search — https://ar5iv.labs.arxiv.org/html/2106.09086. Rollouts stop after N steps and bootstrap from the blueprint's own Q-values. 16 steps keep about 91% of SPARTA's gain at 4.6× less time; 1 step keeps 55% at 36× less. Full rollouts scored slightly lower than 16-step ones.

**Bridge and Skat**
- Bridge bidding — https://ar5iv.labs.arxiv.org/html/2011.14124. An imitation MLP (4×1024) on 1M deals, then policy iteration: search samples deals consistent with the auction, weights them by the policy's likelihood of the observed bids, rolls out with the policy, and the net imitates the result. Against the bot WBridge5: −0.12 → +0.48 IMPs (match points) per deal when partnering that bot, +0.85 as its own partnership.
- NooK beat eight champions in 67 of 80 sets at declarer play only; its internals are undisclosed [snippet: https://www.deeplearning.ai/the-batch/bridge-to-explainable-ai].
- Skat — https://ar5iv.labs.arxiv.org/html/1905.10911. Determinised search whose sampled worlds are weighted by their probability under policies learned from human games: +2.4 to +5.1 tournament points per game over the older table-based inference.

**Hearts, Gin Rummy.** I found no peer-reviewed superhuman result.
- Hearts: a hobby agent trained by neural regret minimisation (540 binary inputs, 4×1080 MLP, one desktop with an RTX 3070, days) wins 58.0% to 43.7% against a strong heuristic program — https://brianberns.github.io/Hearts.
- Gin Rummy: a 243K-parameter policy trained on CPU in hours reaches 34% against an exact-meld heuristic expert — https://www.alphaxiv.org/abs/2607.06854.md.

## 2. What actually mattered

**1. Action-as-input and Monte-Carlo targets carried DouZero.** Its own comparisons: TD learning gave no benefit, DQN "often diverges", actor-critic with action features failed (DouZero URL above). PerfectDou's "vanilla PPO" row wins only 0.346 against DouZero, so the feature and action design is worth more than the algorithm (PerfectDou URL above).

**2. A learned policy without search beat the search-based predecessor in DouDizhu.** Elsewhere search adds on top of the policy:
- Hanabi: +0.45 to +1.2 points, largest on weak blueprints.
- Skat: pure imitation card play (2.5 ms per move) is 2.61 points per game weaker than search at ~650 ms per move — https://ar5iv.labs.arxiv.org/html/1905.10907.
- Gin Rummy: trained policies beat a fair determinised search with 60 rollouts 66–69% of the time (Gin Rummy URL above).

**3. Hidden information at training time: measured gains are moderate.**

| Source | Comparison | Result |
|---|---|---|
| PerfectDou ablation, 10⁹ steps, vs DouZero-10¹⁰ | critic sees hidden hands vs does not | win rate 0.524 vs 0.486 |
| same | no oracle shaping reward | win rate 0.540, but points per game −0.201 vs +0.014 |
| OADMCDou, 30 days on one small box — https://ijcai.org/proceedings/2024/0660.pdf | oracle-guided DMC vs same-budget DouZero | better than the same-budget replica, still below the public DouZero |
| DouZero+ — https://ar5iv.labs.arxiv.org/html/2204.02558 | true next-player hand as input ("maphack") | "boosted performance", no number |
| Gin Rummy study | learner given the opponent's hand | no gain (24.7 vs 25.5%) |
| Suphx | + global reward prediction, + oracle guiding | each adds a step in stable rank; numbers only in a figure |

A general variational form of oracle guiding exists (VLOG — https://mlanthology.org/iclr/2022/han2022iclr-variational, abstract only). The pattern: oracle information speeds learning (PerfectDou claims about 10× fewer samples, mixed with other changes), needs a gradual hand-over, and does not replace good reward design.

**4. Reward decomposition mattered as much as the oracle.** Suphx's round reward from a learned game-outcome predictor and PerfectDou's shaping reward both replace a sparse, noisy final signal with a dense one that is later removed or kept consistent with the final objective.

**5. Value nets must be trained on the states that play reaches.** In ReBeL's ablation a value net trained on random beliefs "fails to learn anything valuable" (ReBeL URL above). DeepStack's 175 core-years of solved random situations were replaced by self-play.

**6. Search details that paid.** SPARTA's pruning and deviation threshold cut rollouts ~10× at no loss. Truncated rollouts with a Q bootstrap beat full rollouts.

**7. When determinisation is sound.** "Strategy fusion" is the error of a search that plans a different action for each sampled world although the player cannot tell the worlds apart.
- Long et al. measured that in trick-taking games sibling outcomes mostly agree (leaf correlation 0.8–1.0) and hidden cards are revealed quickly (disambiguation ~0.6). In that region determinised search loses little against an equilibrium player; poker is the opposite — https://cdn.aaai.org/ojs/7562/7562-13-11092-1-2-20201228.pdf.
- Cowling et al.: a single tree over information sets, re-sampled each iteration, beats one tree per determinisation where deep search or strategy fusion matter. The two are on a par in Dou Di Zhu, where seeing hidden cards helps only in a minority of deals — https://eprints.whiterose.ac.uk/75048/1/CowlingPowleyWhitehouse2012.pdf.
- In single-agent planning, solving each sampled future separately is "hindsight optimisation"; the paper gives examples where it fails — https://mlanthology.org/aaai/2008/yoon2008aaai-probabilistic. Its value estimate is an upper bound on the true value [snippet: https://arxiv.org/pdf/2406.17863].
- A 2026 result: aggregating evaluations of several sampled states in one AlphaZero tree gains 136–291 Elo over the per-determinisation baseline — https://arxiv.org/abs/2605.24139.

**8. Later found unnecessary.** DeltaDou's inference and search (DouZero); neural nets and supercomputers in poker (Pluribus); exact beliefs and full rollouts (Learned Belief Search); generality (Student of Games); Suphx's run-time adaptation (never deployed); encoder choice (MLP, convolution, Deep Sets, attention and LSTM fall in one band in the Gin Rummy study).

## 3. Cheaper replications

| System | Hardware and time | Result | Trick |
|---|---|---|---|
| OADMCDou — https://ijcai.org/proceedings/2024/0660.pdf | 4 cores + 1 GTX 1080, 7 actors, ~10⁸ state-action pairs a day, 30 days | +0.104 ± 0.041 points per game over public DouZero | oracle guiding (hidden-feature weight 1→0 linearly over 20 days) + clipped update size |
| DouZero+ — https://ar5iv.labs.arxiv.org/html/2204.02558 | 4 Xeon CPUs + 1 RTX 2080 Ti | top of Botzone (DouZero ~20th) | hidden-hand prediction head; "coach" net that only starts games from balanced deals |
| DanZero (Guandan) — https://ar5iv.labs.arxiv.org/html/2210.17087 | 160 CPUs + 1 GPU, 30 days | ≥82% against eight rule bots, 60% against humans | DMC unchanged; 513-value state + 54-value action |
| AlphaHoldem — https://pi.cs.tsinghua.edu.cn/lab/people/jlxing/zh/publication/featured02-aaai-2022/ | one PC, 3 days | beats Slumbot and DeepStack over 100,000 hands, 2.9 ms per decision | end-to-end actor-critic self-play, no regret minimisation, no search |

Pluribus (12,400 core-hours) and Learned Belief Search (47 s against 215 s per game) from §1 belong here too. Common efficiency levers: Monte-Carlo targets, action features, train-time oracle information, start-state selection, and truncating rollouts with the policy's own value.

## 4. Design lessons for a Slay the Spire agent

**What is different here, and what follows.**
- Hidden information is only chance (draw order, enemy rolls, future rewards). There is no opponent with private cards, so the belief is exact and uniform; no inference network or belief model is needed.
- The value of an information state is well defined. That is not true in poker, and it is why ReBeL needs belief vectors.
- The project's search already uses one tree with hidden state re-sampled per simulation and nodes keyed by observation [project c1], which is the sound form (§2.7). Its known weakness is that statistics stop at the turn boundary [project c1].

**State encoder (DouZero pattern).**
- Per-pile card-count vectors over the card vocabulary (hand, draw, discard, exhaust; upgraded cards as separate entries), with a learned embedding per card summed per pile.
- Enemy slots (HP, block, intent, powers), player powers, relic bitmap, potions, energy, turn number.
- Plain MLP. Do not spend effort on architecture (§2.8).
- The failed leaf value used 204 summary features [project c1]; this lineage never summarised cards away.

**Action head.** Score each legal action with the action as input: card embedding, upgrade, current cost, target enemy features. Compute the state trunk once and a small head per action, as PerfectDou does. The same head covers card rewards, shops and events, whose option sets vary. Keep single card plays as the unit: a turn is not a set of cards as in DouDizhu, because draws arrive mid-turn. PerfectDou's 621 action classes and poker's bet abstraction answer a problem this game does not have.

**Value target.**
- Use the outcome of the agent's own play (DMC, γ=1), not the mean of random rollouts. A net fitted to random-rollout means can only reproduce rollouts more cheaply; DMC nets improve because the policy that generates the returns is greedy on the net.
- For a fight, the reward is Suphx's construction: Φ(run state after) − Φ(run state before), with Φ the existing run-value net retrained on the current agent's runs. This replaces the hand-written terminal value.

**Training loop.**
1. Warm-start by imitating the present search agent. It plays the role of human replays (Suphx, bridge).
2. DMC with parallel CPU actors, ε-greedy.
3. Train one level at a time (combat, then deck decisions) with the other frozen. Suphx trained only its discard model by reinforcement learning.
4. Choose start states with a coach: fights and run states whose outcome is uncertain, not the already-won ones. Ascension levels are the curriculum; they replace league opponents.
5. Use train-time hidden information in three ways, all legal here:
   - Oracle guiding: feed the next draws and enemy rolls as inputs with a keep-probability decayed to 0.
   - For actor-critic training, a critic or baseline that sees the future random inputs. This stays unbiased because the action is independent of future inputs given the observation [snippet: https://arxiv.org/abs/1807.02264]. A critic that sees state but not history can bias learning [snippet: https://arxiv.org/abs/2105.11674]; here the observation determines the belief, so this risk is small.
   - For run-level choices, compare options under the same future seeds. The project found per-option playout means too noisy to rank Ascension 20 card choices [project h2].

**Play-time search.** The budget is about 3–4 ms per combat decision; a 1M-parameter MLP costs ~19 µs per batched evaluation [project s5, c1], so about 150–200 evaluations per decision.
- First choice: enumerate or beam-search the current turn (mostly deterministic), sample draws where a card draws mid-turn, and evaluate each end-of-turn state with the net. This is DeepStack's and ReBeL's "solve to the end of the round, learned value at the boundary", placed exactly where the current tree stops being useful.
- Second: keep the present tree and use the net only at the turn boundary.
- SPARTA-style rollouts with a neural policy do not fit the budget at play time. They fit offline, as the improvement step that generates training targets (the bridge recipe), and at play time only with a distilled, microsecond-scale rollout policy.
- Adopt SPARTA's rule in any case: deviate from the policy's choice only when the search margin is statistically clear.
- Pluribus's cheap alternative to a learned leaf: let each leaf take the best of a few fixed continuation policies (aggressive, defensive, default). In a single-player game each policy's value is a lower bound on the optimum, so the best of them is a tighter bound, up to sampling noise (my reasoning, not a published result).

**Analogues.**

| Landmark component | Here |
|---|---|
| Human replays | the current search agent's games |
| League opponents, self-play curriculum | ascension ladder + coach-selected start states |
| Build-order conditioning | a deck-plan or archetype input to the policy (not from this lineage) |
| Fast rollout policy | a distilled tiny policy, or Pluribus-style fixed continuation policies |
| Oracle critic, oracle guiding | critic or inputs that see draw order, enemy rolls, future offers during training only |
| Global reward predictor | run-value net; fight reward = its change |

**What breaks.**
- Single player: no opponent supplies diversity or a rising difficulty. Exploration and start-state coverage must be engineered, and a fixed environment makes over-fitting to seeds a risk.
- Variance: these games give millions of short independent deals. A run is ~50 fights and one outcome; only Suphx addresses this, and for about ten rounds.
- Deck building: every system here plays a fixed deck. Nothing in this lineage learned to construct one, and the net must generalise across decks it has rarely seen, so coverage of (deck, enemy) pairs is the data problem.
- Card meaning: DouDizhu cards differ only by rank. Here each card needs a learned embedding, and rare cards get few samples.

## 5. Cost to try it here

**E0 — value of clairvoyance (diagnostic).** Run the current agent with the true draw order and enemy rolls in every simulation, against the fair agent, on Ascension 20 and Heart seeds.
- Cost: 2 × 2,000 runs at ~1.4 core-seconds [project s5], under 2 core-hours.
- Signal: the win-rate gap bounds what better belief handling could recover at play time. A gap under about 5 points (my threshold) closes that line and points at evaluation quality. The earlier attempt left no usable number [project h6].

**E1 — DMC value at the turn boundary, hard fights only.**
- Data: a bank of pre-fight states (Ascension 20 act bosses, elites, Heart) from the agent's own runs.
- Model: card-count encoder and action-as-input Q net of 0.2–1M parameters, warm-started by imitation.
- Player: greedy turn enumeration with ε = 0.01.
- Target: fight outcome valued by the run-value net.
- Cost: about 1,000 core-hours (ten boxes for a day) for ~10⁹ decision samples [estimate: ~2 ms per decision, ~50 decisions per fight]. One box trains a 1M-parameter MLP on 3.2B rows a day [project s5]. No GPU is needed.
- Signal: fight win rate and HP lost on held-out (deck, fight) pairs against the present search at equal wall-clock, plotted against samples. DouZero crossed its supervised baseline at 5×10⁸ steps and the search agent at 5×10⁹.
- Stop rule: flat over the last decade of data and still below the search agent.

**E2 — oracle guiding ablation on E1.** The same run with next draws and enemy rolls as decaying inputs. About 500 core-hours. Signal: samples needed to reach E1's level; expect a speed-up, not a higher ceiling.

**E3 — offline improvement.** If E1 works, use SPARTA-style rollouts with the E1 policy to label states, then retrain. This is CPU-only and scales with boxes.

**GPUs.** Not needed for E0–E3. A GPU helps only if the net must exceed a few million parameters, and the CPU play-time budget excludes that anyway. The landmark systems used GPUs mainly for actor inference of larger nets.

## 6. Top transfers and dead ends

**Top transfers**
1. **DMC on own-play outcomes, card-level encoding, action-as-input, used as a turn-boundary value with exact within-turn search.** Confidence: medium. DouZero beat a search agent this way, but two learned-leaf attempts failed here with a different target and representation.
2. **Fight reward = change in a learned run value (Suphx's global reward prediction).** Confidence: medium-high. Cheap, and it removes the hand-written terminal value.
3. **Train-time hidden information: oracle guiding for sample efficiency, future-aware baselines and shared seeds for noisy run-level decisions.** Confidence: medium. Measured gains are a few points or a speed-up; one null result.
4. **Blueprint plus belief-sampled rollouts as the offline improvement operator, with pruning and a deviation threshold.** Confidence: medium offline, low at play time under the CPU budget.
5. **Coach-selected start states and the ascension ladder as curriculum.** Confidence: low-medium.

**Dead ends**
- **Regret minimisation, public-belief value nets, safe subgame solving, R-NaD.** They exist to resist an adversary who exploits the agent; there is none.
- **Opponent-hand inference and learned belief models (Skat, DouZero+, Learned Belief Search).** The belief here is exact and free.
- **Further determinisation refinements.** The search is already the single-tree form, and E0 will show how little is left.
- **Suphx's run-time policy adaptation.** It needs per-fight fine-tuning compute, which the fixed per-decision budget excludes; Suphx itself did not deploy it.
- **A unified search-and-learn system such as Student of Games.** Weaker than specialists and far more compute.
- **Large residual or attention encoders.** No evidence of gain at this scale, and too slow for the CPU budget.
