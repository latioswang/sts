# S2 — AlphaStar / OpenAI Five lineage: what transfers to a Slay the Spire agent

Web tools were available. Sources actually opened (tag = URL):

- **[AS]** AlphaStar Nature paper, full text + Methods: https://storage.googleapis.com/deepmind-media/research/alphastar/AlphaStar_unformatted.pdf
- **[ASb]** DeepMind blog: https://deepmind.google/discover/blog/alphastar-grandmaster-level-in-starcraft-ii-using-multi-agent-reinforcement-learning/
- **[O5]** OpenAI Five paper, full text + appendices: https://ar5iv.labs.arxiv.org/html/1912.06680
- **[ASU]** AlphaStar Unplugged: https://arxiv.org/pdf/2308.03526
- **[ROA]** ROA-Star (NeurIPS 2023): https://papers.neurips.cc/paper_files/paper/2023/file/94796017d01c5a171bdac520c199d9ed-Paper-Conference.pdf
- **[TX]** TStarBot-X: https://ar5iv.labs.arxiv.org/html/2011.13729
- **[SCC]** StarCraft Commander: https://ar5iv.labs.arxiv.org/html/2012.13169
- **[mAS]** mini-AlphaStar: https://ar5iv.labs.arxiv.org/html/2104.06890
- **[DI]** DI-star README: https://github.com/opendilab/DI-star
- **[D3]** DreamerV3 abstract: https://arxiv.org/abs/2301.04104 · **[IMP]** IMPALA abstract: https://arxiv.org/abs/1802.01561 · **[JW]** Honor of Kings full-MOBA abstract: https://arxiv.org/abs/2011.12692

Reading quality: [AS], [ASU], [ROA] were read directly as extracted PDF text. [O5], [TX], [SCC], [mAS] were read through a page-summarising fetch of the full text, so their numbers are one step removed and worth a spot check before being quoted onward. R2D2 could not be opened (OpenReview blocked the fetch). `[estimate]` = my arithmetic, not measured. `[project]` = from the task brief.

---

## 1. How each system was built

### AlphaStar (StarCraft II, 2019) [AS]

- **Inputs.** A list of up to 512 entities (unit type, owner, health, position, cooldowns, orders, buffs), a 128×128 map with 7 layers, scalar player data, game time. Fog of war applies; a camera restricts detail.
- **Network.** Scalar MLP + entity **transformer** + spatial ResNet, joined by **scatter connections** (entity embeddings written onto the map at their positions), then a **deep LSTM core**. 139M weights, of which only 55M are needed at inference; the rest are value networks.
- **Actions.** About 10^26 choices per step, factorised **autoregressively**: action type (residual MLP) → delay → queued → selected units (**pointer network**) → target unit (attention) → target point (deconvolution).
- **Strategy statistic z.** Extracted from each human replay: the build order (first 20 constructed buildings and units) and cumulative statistics (units, buildings, effects, upgrades present in the game). The policy is π(a | s, z). In supervised learning z is zeroed 10% of the time, which yields one network that is both conditional and unconditional.
- **Stage 1, imitation.** 971,000 replays from players above 3,500 MMR (top 22%); KL loss per action argument; then fine-tuning on 16,000 winning games above 6,200 MMR (win rate against the Elite bot 87% → 96%).
- **Stage 2, RL.** Reward ±1 at the end, no discount. Policy: V-trace plus **UPGO** (follow the return only while the behaviour action was at least as good as the value estimate, else bootstrap). Value: TD(λ) with no off-policy correction. Value networks also receive the **opponent's observations** (training only). Pseudo-rewards for following a sampled z (edit distance on build order, Hamming distance on cumulative statistics), each active with probability 25%, each with its own value head. A KL penalty towards the supervised policy stays on throughout.
- **League.** Per race: 1 main agent (35% self-play, 50% PFSP against all past players, 15% against forgotten players and past exploiters; never reset), 1 main exploiter (plays only current main agents; always reset to the supervised weights after it is frozen), 2 league exploiters (PFSP against everyone; 25% reset chance). PFSP samples opponent B with weight f(P[A beats B]); f_hard = (1−x)^p for robustness, f_var = x(1−x) as a curriculum.
- **Search at play time.** None. One consumer GPU per agent.
- **Compute.** Per learning agent: 16,000 concurrent matches, 16 actor TPU-v3 devices, a 128-core TPU learner at about 50,000 agent steps/s, about 4,200 CPU cores; 12 such agents for 44 days; almost 900 frozen players created. The main agent consumed about 1.9×10^11 steps [ROA].
- **Result.** 6,275 / 6,048 / 5,835 MMR (Protoss / Terran / Zerg), above 99.8% of ranked players.

### OpenAI Five (Dota 2, 2019) [O5]

- **Inputs.** About 16,000 values per hero per step from the game API (189 units × 43 features, plus items, abilities, hero stats); fog respected, no camera.
- **Network.** Per-set encoders ("Process Set") into a **single-layer 4,096-unit LSTM** that holds 84% of 159M parameters, shared across the five heroes. Value is a linear head on the LSTM with no privileged information; the paper calls AlphaStar's privileged value "a promising direction".
- **Actions.** Factorised heads: primary action (≤30) × delay (4) × unit (189) × offset (81); 8,000–80,000 valid actions per step. Acts every 4th frame: about 20,000 decisions per game.
- **Training.** PPO with GAE(λ=0.95), 80% self-play against the latest policy and 20% against past ones. **No human data.** A hand-built **shaped reward** (win +5, towers, hero deaths, gold, XP, …), made zero-sum by subtracting the enemy average and **decayed by 0.6^(T/10 min)**. Team spirit raised from 0.3 to 1.0. GAE horizon annealed from 60 s to 840 s.
- **Scripted.** Item purchase order, ability order and courier control were hand-scripted and randomised during training. The analog of a build order was never learned.
- **Compute.** Batches up to about 2.9M timesteps, 480–1,536 optimiser GPUs, 80,000–172,800 CPU cores, sample reuse about 1, ten months, 770±50 PFlops/s-days; more than twenty "surgeries" to carry weights across model and game changes.
- **Search.** None. **Result.** Beat the world champions 2–0; 99.4% of 7,257 public games.

### Dreamer / IMPALA / R2D2 (training-algorithm context only)

IMPALA decouples actors from a learner and corrects the lag with V-trace [IMP]; AlphaStar is IMPALA plus replay, UPGO and centralised batched inference [AS]. R2D2 adds recurrent replay with burn-in for value-based agents [from memory, unverified]. DreamerV3 learns a world model and trains in imagination, collecting Minecraft diamonds "without human data or curricula" [D3]; that pays when the real environment is slow, the opposite of this project.

---

## 2. What actually mattered

**AlphaStar ablations** ([AS] Fig. 3; one matchup, 10^10 steps):

| Component | Result |
|---|---|
| Human data | none **149** Elo · supervised only 936 · human init + RL 1,020 · + KL to supervised 1,400 · + z **1,540** |
| Reference bots | Very Easy 418 · Elite 603 |
| Architecture (supervised win rate vs Elite) | baseline 0% · + action delays 7% · + pointer network 36% · + transformer 71% · + scatter 87% |
| Off-policy learning (avg win rate) | V-trace 49% · + TD(λ) 73% · + UPGO 82% |
| Value sees opponent | with **82%** · without **22%** |
| League | main only 1,540 · + main exploiters 1,693 · + league exploiters 1,824 |
| Matchmaking (Elo / min win rate vs own past) | PFSP+SP 1,540 / 71% · SP 1,519 / 46% · PFSP 1,273 / 70% · FSP 1,143 / 69% |

Readings:

1. **RL from scratch failed**: at 10^10 steps it was weaker than the easiest built-in bot. RL from a human initialisation alone gained only 84 Elo over imitation; anchoring (KL, +380) and strategy conditioning (z, +140) carried the RL stage.
2. **Imitation alone reached the 84th percentile** (3,699 MMR) [AS]; DI-star's supervised model is "equal to diamond players" [DI].
3. **Representation carried imitation**: 0% → 87% from architecture alone.
4. **The privileged critic is the largest single RL ablation** (22% → 82%).
5. Plain self-play was nearly as strong as PFSP+SP but forgot (46% vs 71%); the league bought robustness more than raw strength.

**Post-mortems on AlphaStar.**

- [ROA]: the final agent "was easily defeated by Grandmaster players using some uncommon strategies, such as Cannon Rush", and late in training the unconditioned exploiters "tend to lose the ability to identify the weaknesses". ROA-Star conditions exploiters on z (top-10% win-rate z to exploit, under-executed z to explore) and adds opponent prediction; against a replicated AlphaStar league its 10-day main agent's worst-case win rate was 44% versus 1%.
- [TX]: with less compute the main agent collapsed to one or two rush strategies within days, and exploiters reset to a weak supervised model never caught up.
- [ASU]: **no memory beat the LSTM** for imitation (84% vs 70% against the very-hard bot); halving width hurt (83% → 76%), halving depth did not (82%); training only on elite data hurt (65%), full data then elite fine-tuning was best (89%).
- [ASU], search: the best offline agent was a behaviour-cloned policy + value + latent model with **MCTS at inference only** (Elo 1,578, robustness 50%, versus 1,425 / 30% for the same network without search); 20 sampled actions per node sufficed. Using MCTS outputs as *training* targets collapsed offline; offline Q-learning scored 0%.

**OpenAI Five** [O5].

- A clean **Rerun** in the final environment reached 98% against the final agent with about 20% of the compute (150 vs 770 PFlops/s-days): most of the cost was the exploratory path.
- Surgery saved an estimated 30 months (10 instead of 40).
- Sample reuse of 2–3 halved learning speed; stale data hurt severely.
- A sparse win/loss reward did learn (TrueSkill 155; the scripted bot is 100) but with "a large penalty to sample efficiency". The shaped agent ended above 250, though that is not a like-for-like run.
- Longer horizons kept helping up to 6–12 minutes; the lane-assignment randomisation "provided little benefit".

**Across both.** Neither system learned long-horizon builds by undirected RL: AlphaStar imitated them and conditioned on them, OpenAI Five scripted and randomised them, TStarBot-X added rule guidance for decisions that occur "once in thousands of steps" [TX].

---

## 3. Cheaper replications

| System | Compute per learning agent | Net | Result | What bought the efficiency |
|---|---|---|---|---|
| SCC [SCC] | ~1,000 envs, ~800 steps/s, ~30 game-years (AlphaStar: 200), 58 days | 49M | Beat two professionals 2:0 each; 3:2 and 5:0 vs Grandmasters | One matchup, one map; 64×64 minimap; fine-tune on top replays; **agent branching** (new agents start from the main agent with the z that wins most; z spread was 35–65% win rate) |
| TStarBot-X [TX] | whole league: 144 V100 + 13,440 cores, 57 days | 20M (value net ~30× smaller) | 35 wins, 1 loss vs Master/Grandmaster (per its table) | One matchup; exploiters seeded from six strategy-specific supervised models; rule-guided KL on critical states; KL to the previous snapshot once the supervised anchor became the limit |
| ROA-Star [ROA] | 64 V100, 4,600 cores, 2,400 games, 11,000 steps/s, 50 days, 4.4×10^10 steps | — | 50–60% over repeated games vs four professionals | One matchup; goal-conditioned exploiters; opponent prediction and scouting reward |
| AlphaStar Unplugged [ASU] | offline, 10^10 frames, 2.8M episodes | memory-less | 90% vs original AlphaStar Supervised | No environment in training; search only at inference |
| DI-star [DI] | 4-GPU example | — | SL ≈ diamond, RL ≈ master/grandmaster (one matchup) | Open code |
| mini-AlphaStar [mAS] | "common commercial machine" | LSTM 128, 1 layer | "final performance … not ideal" | Uniform shrinking did not work |

Pattern: the successful cheap systems narrowed the task and kept the imitation prior. Only the offline one added play-time search.

---

## 4. Design lessons for a Slay the Spire agent

**First, what does not carry over.** Neither landmark used search: they had no fast exact model and had to act in real time. This project has a model and a per-decision budget. [ASU] is the only case I found in the lineage that combines a learned policy/value with search, and search won. So this lineage should supply the representation, the training loop and the strategy layer, and the search stays.

**State encoder.** Replace the 204 hand features with entities, as in [AS]: one token per card in hand, draw pile (an unordered set, since order is hidden), discard and exhaust; per enemy (HP, block, intent, powers); per relic; per potion; plus scalars (HP, block, energy, turn, floor, ascension, gold). Shared card and relic embedding tables serve combat and out-of-combat alike. A 2–3 layer transformer; **no LSTM** (the public state is almost Markov, and [ASU] found memory-less better); no scatter connections (nothing is spatial). Per [ASU], keep width and cut depth.

**Action head.** Autoregressive pointer: action type (play / potion / end turn) → pointer over hand tokens → pointer over enemy tokens. Out of combat, a pointer over the offered options with a learned "skip" token. This is where [AS] gained most (0% → 36% → 71%).

**Value target.** Every system here trains V on returns of the *acting policy* (TD(λ) in [AS], GAE in [O5]); none regresses on random-rollout outcomes. That is a likely reason the leaf MLP lost: its target was the value of random play [my inference]. Use separate heads, as [AS] did per pseudo-reward: P(win run), P(clear each act), HP after the current fight. Train two critics: **V_priv**, which sees the draw order, the RNG stream and upcoming rewards and serves only as the policy-gradient baseline; and **V_pub**, on public information, the only one play-time search may call. This respects the leak rule and is the analog of the 22% → 82% ablation; with this game's draw-order variance it should matter at least as much [my inference].

**Training loop.**

1. *Imitation.* The "human replays" for combat are the 1,000-simulation search agent's own decisions; for deck building, the agent's winning runs plus public human run logs, which record picks and paths but not combat [from memory, unverified]. Train on everything, then fine-tune on wins ([AS], [ASU]).
2. *RL, anchored.* PPO or V-trace with a KL term to the imitation policy; when the anchor becomes the ceiling, switch it to the previous snapshot ([TX]).
3. *Reward.* Win/loss alone at Ascension 20 Heart is a desert at 0.3% [project]. Put shaped pseudo-rewards (act cleared, floor reached) on their own value heads and anneal them, as [O5] decayed shaping. A run is roughly 1–2 thousand decisions [estimate], close to AlphaStar's episodes, so no discount is needed.
4. *Throughput.* The network, not the simulator, is the cost: batch inference across environments ([AS]: "large efficiency gains") and reuse samples at most twice ([AS] replayed twice; [O5] lost half its speed at 2–3).
5. *Surgery.* Zero-initialise new inputs so weights survive simulator fixes and feature additions ([O5]).

**Build-order conditioning.** z = (order of the first N adds, removals and upgrades; final deck multiset and relic set), taken from winning runs. Condition the out-of-combat policy on z, drop z 10% of the time, pay a pseudo-reward for moving towards z. Unlike StarCraft, card offers are random, so an exact sequence is not executable: z must be coarse (an archetype cluster or target deck vector) and the pseudo-reward must count only picks that were offered. Keep a moving win rate per z and sample the best and the least-executed: [ROA]'s exploit/explore rule and [SCC]'s branching, as a bandit over deck plans. Its second job matters more: it forces the combat policy to pilot decks the current drafter would never build. [AS]'s example is a policy good with ground units that never discovers air units because every first attempt loses; here, a drafter never values an engine deck because the pilot cannot play it.

**League: what a single-player game can and cannot borrow.**

| League piece | Single-player analog | Verdict |
|---|---|---|
| PFSP matchmaking | Sample training scenarios (saved fight-start states × ascension) by x(1−x), later (1−x)^p, of the current win probability | Borrow |
| Playing forgotten opponents | Re-test scenario clusters already mastered | Borrow |
| Exploiters reset to the supervised weights | Specialists restarted from the imitation policy on one deck plan or one boss; where a specialist beats the main agent on the same states, distil it in ([TX] specific exploiters; [JW] lists curriculum self-play and distillation) | Borrow, as specialists |
| Self-play as automatic curriculum | Ascension 0 → 20 and the act structure | Replace |
| Zero-sum reward symmetry | V_priv and common random numbers | Replace |
| Nash, payoff matrix, opponent modelling, scouting | — | Drop: there is no adversary and no cycle |

**Fast rollout policy.** The lineage has none; the closest device is the small student ([AS] ships 55M of 139M weights; [TX] ran 20M). Here: distil the entity policy into a per-card scorer cheap enough to replace uniform rollouts [my inference].

**Play-time search.** Keep determinised MCTS; put the learned prior at every node if a forward pass fits; replace the hand terminal value with V_pub at truncated leaves. A fast reactive policy also allows run-level lookahead for strategic decisions, by playing the next fights policy-only under freshly sampled randomness [my inference].

**What breaks here.**

- The environment does not get harder as the agent improves; the curriculum must be built.
- Luck is large. UPGO reinforces better-than-expected outcomes, and with a shuffled deck that is often just a good draw. Use it only with the V_priv baseline, or not at all [my inference].
- Deck and pilot co-adapt, so judging a pick under the current pilot understates it.
- The play-time network must fit a fixed CPU budget; AlphaStar's ran on a GPU.

---

## 5. Cost to try it here

AlphaStar got about 3 agent steps per second per match (50,000 steps/s from 16,000 matches) [AS]. This simulator is orders of magnitude faster, so network inference is the whole cost. For a 0.3–2M-parameter entity network on 40 vCPUs, expect roughly 10^4–10^5 decisions per second fleet-wide [estimate, to be measured], the order of ROA-Star's 11,000 steps/s per agent.

| Step | Work | Compute | Signal |
|---|---|---|---|
| 0. z spread | Bias the existing drafting heuristics towards each of ~12 deck-plan clusters; 4,000 Ascension-20 runs per cluster with the existing search | ~35 CPU-h, about 1 h on the fleet [estimate, assuming ~2.6 CPU-s per searched run] | Standard error ≈ 0.43 points at 8%. Go if the best cluster beats unconditional play by ≥ 2 points and holds on fresh seeds. Flat means deck plan is not the bottleneck under the current pilot |
| 1. Imitation | Log raw entity states from ~50,000 search-agent runs; train the entity/pointer policy and both critics | Data ~40 CPU-h; training 6–16 h on one 4-vCPU box, under 1 h on one GPU [estimate] | Policy-only Ascension-0 win rate (imitation alone gave the 84th percentile in [AS]); gain at equal wall-clock when it replaces the current root prior |
| 2. Anchored RL on a fight bank | ~10^9 decisions from saved fight-start states, Act 1 elites to the Heart; PFSP-style sampling; KL anchor; V_priv | 2–5 days on the fleet; the learner is the bottleneck on CPU, and one mid-range GPU removes it [estimate] | Policy-only HP lost and win rate on held-out fights versus the imitation policy and versus the 1,000-simulation search on shared seeds; ablate V_priv and the KL term |

Stop if step 2 does not clearly beat its own initialisation by 10^9 decisions; [AS]'s ablations separated at 10^10 steps on a far harder problem. No GPU is needed for the signal. A league-scale effort would need many, and this document does not recommend one.

---

## 6. Top transfers and dead ends

**Top transfers**

1. **Imitation first, then KL-anchored RL**, with the search agent as the "human" (149 → 936 → 1,400 Elo in [AS]). Confidence: **high** that imitation gives a strong prior; **medium** that RL then exceeds the search teacher.
2. **Privileged-information critic for training only**, public critic for search (22% → 82%). **High** for variance reduction, **medium** for win rate.
3. **Entity encoder + pointer heads, no memory**, replacing hand features (0% → 87%; no memory 84% vs LSTM 70%). **High**.
4. **Coarse strategy statistic z with a bandit over deck plans**, used to diversify what the pilot trains on (+140 Elo; z spread 35–65% in [SCC]). **Medium**; weakened by random offers.
5. **Prioritised scenario sampling, ascension curriculum and specialists distilled into the main agent** (PFSP 1,273 vs FSP 1,143 Elo). **Medium**.

**Dead ends**

- **The league as game theory** (exploiters as adversaries, Nash, opponent modelling): it exists to stop cycles between opponents, and there are none.
- **RL from scratch, or scale first**: below the easiest bot at 10^10 steps [AS]; OpenAI Five needed 770 PFlops/s-days and still scripted its builds [O5].
- **A pure policy as the final agent**: the lineage had no alternative; [ASU] shows search adds on top.
- **Learned world models**: the simulator is exact.
- **LSTM cores and recurrent-replay machinery**: unnecessary in a near-Markov state, and worse in [ASU].
- **Offline value-based learning on logged games**: 0% in [ASU].
- **Exact build-order imitation**: offers are random.
- **Shrinking AlphaStar uniformly**: [mAS] reports it did not perform.
