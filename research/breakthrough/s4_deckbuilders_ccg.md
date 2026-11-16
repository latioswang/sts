# S4 — Deck-builders, card games and roguelikes: what the strong agents did (2026-11-16)

Web search and page fetch were available. Each source key resolves to a URL in the list at the end; I opened every page today unless it is tagged **[snippet]** (seen only in a search-result summary) or **[blocked]**. Numbers from GitHub READMEs are the authors' own and unreviewed.

**Headline.** A published recipe already beats this project on the same game. `spire-engine` (Slay the Spire 1, own Rust engine, no search at play time) reports Ironclad wins of 93.1% / 75.2% / 21.9% at A0 / A10 / A20 with a 1.07M-parameter transformer trained by PPO on whole runs on one consumer GPU [SE-IC]. This project stands at about 86% / 52% / 8%. Engines, seeds and leak audits differ, so the gap is indicative, not measured. Three other sources point the same way: the strategic policy was learned by reinforcement learning on the run outcome, over 10^5–10^6 runs, and was not built from one-step value differences.

## 1. Systems

### Slay the Spire 1 and 2

| System | Representation | Method | Compute | Result |
|---|---|---|---|---|
| spire-engine, StS1 [SE] [SE-IN] [SE-AR] [SE-IC] | Tokens through a transformer (d=128, 6 layers, 4 heads). Hand: 10 tokens × 15 features. Draw / discard / exhaust piles and relics pooled. 6 monster tokens with intent and last two moves. Per (card, target) damage, kill flag and HP share removed are computed inputs. 171 masked action slots plus a value | One network per character plays every decision. PPO in stages: fights on random decks, whole runs, block and exhaust practice, boss practice on winning human A20 decks, then "habit" rewards. Reward 0.02 per floor, 0.1 per act, 3 for a win, gamma 0.9995. 30% of games are practice fights. Small cross-entropy pull toward human A20 winners | One RTX 5070 Ti, 14–19 Oct 2026. Final stage 300 updates of 2,048 games × 128 steps. Inference a few ms | Ironclad 93.1 / 75.2 / 21.9% (1,024 seeds each, Act 3 boss, no Heart). With a look-ahead that plays likely moves on redrawn copies to the next turn: 27.5% at A20, not released |
| Spire Mentor, StS2 [SM] [SM-PB] [SM-EX] | Strategy: per-character PPO transformer, embedding tables for cards, relics, potions, events, 160 action slots. Its predecessor used a deck histogram (count, upgraded) of 1,158 dims, relic multi-hot 294, 25 scalars | Combat: chance-node MCTS, non-clairvoyant. Leaves scored by a hand-tuned 172-feature evaluation, no rollouts; learned value nets exist as optional leaf evaluators and are not deployed; a learned playout net since 21 Oct. Strategy: PPO with MCTS in the loop at 250 simulations (1,000 for bosses) | 46 workers + one RTX 3090. 960 games per iteration in about 208 s; campaigns of 150 iterations, 31–82 h per the playbook, collection 86–97% of wall-clock. Update 497 s on CPU vs 33 s on GPU | Ironclad 96.0% at A0 and 27.2% at A10 with 8,000 simulations; a later champion 30.0% at A10 (400 runs). Switching from clairvoyant to honest search cost 22.8 points |
| Zero-search StS2 paper [ZS] | Up to 192 entity tokens (5 ids + 32 numbers each). Hand cards are tokens, piles are multisets. 6 blocks, width 256, 6.57M parameters. Each legal action is a token with up to two pointers | One network for combat and strategy, PPO, shaped reward, four value heads (return, P(win), floors remaining, end-of-combat HP fraction). Restarts from stored states, weighted by where runs die | Two GPU servers, 256 simulator processes, about 50M rows per controlled run. 21–27 ms per decision on one core | A0 80.2%, A10 8.05% mean over five characters (Ironclad 15.6%). Mentor re-run in the paper's host, 400 common seeds per character: 8.8% vs 8.2% |
| BottledAI (rules) and language models, StS1 A20 [SB] | Game state as text | Rule bot; LLMs at maximum reasoning | 33–62 USD per run for the top three models | 10 seeds each. BottledAI clears Act 1 in 20%, never Act 2. Best model wins Act 3 in 20%; one model killed the Heart once. Streamers: 56–71% Act 3, 46–65% Heart. All-player telemetry: 7% and 4% |

Also seen: spire-agent (MCTS over sts_lightspeed for combat, expert deck templates, an LLM for unresolved trade-offs; several A20 Heart wins claimed, no rate) [SA]. AscensionAI (PPO on the live game: zero wins in 21,700 games; moving to sts_lightspeed with a 1-ply combat search raised mean floor 14.7 → 26.4) [AA] [AA-R]. AlphaSpire (separate strategy and combat networks guiding search; its released networks without search won 0.55% at A10 in [ZS]) [AS]. sts2-rl-research (separate battle and choice agents, 26.6% at A2) [S2].

Three results inside [ZS] and [SM] matter most here.

1. **Swapping layers.** The learned non-combat policy with Mentor's combat search gave the best Regent result (70 wins); learned combat with Mentor's strategy did badly (22 and 2 wins). On Mentor's own figures it leads the zero-search policy by 4.8–7.8 points from A4 to A10 and by nothing at A0 [ZS].
2. **A learned fight-outcome surrogate failed.** Built to replace MCTS during strategy training, it had good per-fight error (HP MAE 0.071 vs 0.135 for a table) and failed in closed loop: survival 0.64–0.68 against a true 0.29, because death is a tail event. Eight hypotheses were eliminated and the line was closed [SM-SU].
3. **Search helps, distilling it did not.** Search at the final bosses added about 3 points to the zero-search policy; distilling it back into the policy made it worse [ZS].

### Collectible card games

- **ByteRL, Legends of Code and Magic (LOCM)** [BL]. One network for draft and battle. Card embeddings are shared; picked cards are mean-pooled into a deck embedding, updated after each pick and fixed as context during battle. LSTM of 256 units, one value head, reward ±1 at game end only. V-trace + UPGO, self-play against a pool of past models. 24 units of (1 V100 + 600 CPU cores) for 72 h. First in both tracks of COG 2022. Against a fixed baseline: end-to-end 81% vs 64.7% (evolved draft) and 68% (separately trained neural draft) on v1.2; 65.5% vs 56.5% on v1.5. MCTS on top of the policy helped early (56%) and faded with training (51% at 144 h); not adopted.
- **ByteRL, Hearthstone** [BH]. Same design; deck building is 30 sequential picks from about 270 cards per hero, trained on the game outcome alone. Gamma 1.0 instead of 0.99: about +7 points. Forced random first picks keep decks diverse. 24 V100 and 5,856 cores, 23 days. Beat a former top-10 China player 3:0 twice. No search. A later paper trained an adversary that exploits ByteRL in LOCM [BX, snippet].
- **LOCM competition history** [LS]. Every edition on the fixed card pool was won by search with a static or tuned card ordering for the draft: minimax depth 3 (Coac), MCTS with card weights from harmony search (Chad), flat simulation (DrainPower). Once the pool was randomised per match, the top three were all neural.
- **Draft-only RL** [V20]. PPO drafter, reward = battle win with a fixed battle agent, 30,000 self-play episodes, 30 minutes to 3 hours. It beat all tested draft agents and lifted a tournament agent from 10th to 4th (34.0% → 46.1%). The variant that ignores previously picked cards beat the history and LSTM variants at that budget.
- **Hearthstone search with learning.** Determinised MCTS on information sets with a small value network (256-128-64 on 750 inputs) that cuts rollouts short: first-player win rate 73.5% → 82.0% at 0.5 s per move [SW]. Chance-event bucketing plus a learned high-level rollout policy: 57.2% against the same search without it, at 5–10× the cost per rollout [ZB]. An open-source engine reports a network default policy needing under 15k iterations where random rollouts needed over 300k [PH]. Deck search: MAP-Elites with an online surrogate of game outcomes [ME].
- **Tales of Tribute**, a competitive deck-builder [TT]. The 2023 winner ran five MCTS trees, one per sampled seed, searched only the current turn, and scored with a heuristic whose weights were evolved per game stage; card values came from human tier lists. No neural entry is reported.
- **Magic: The Gathering.** Drafting is treated as imitation of human picks: a Siamese ranking network on about 100M picks reaches about 68% agreement on a known set and 55.4% on an unseen one; the input representation hardly matters for cards seen in training [MG] [MW]. I found no strong full-game agent.
- **Yu-Gi-Oh** [YG]. RL on fixed decks, card text embedded by a language model, over 100M games. No measurement against humans, no learned deck building.
- **Pokémon battles.** Metamon: transformers up to 200M parameters, offline RL on 475k human battles plus self-play, no search, top 10% of the ladder [PM]. Foul Play: root-parallel MCTS over sampled hidden states with a hand evaluation instead of rollouts, GXE 88% in Gen 9 random battles; Metamon drew with it in two generations and beat it in two [PF] [PM]. PokéChamp: LLM-driven minimax, projected Elo 1300–1500 [PC].
- **Gwent, Monster Train:** my searches found no agent with reported strength. **Balatro:** only language-model benchmarks [snippet].

### Dominion

Provincial coevolves "buy menus" over a few million games and remained the reference bot. A Rainbow DQN that only buys (2 × 256 MLP, about 7,000 games, 45 minutes on a 1080 Ti) beat it 575–363 and lost 418–558 to a village/smithy engine it never explored [DH]. A PPO MLP on a 6-card subset needed 100k self-play games (90 minutes) to match a Monte-Carlo agent trained on 5M games; masking illegal actions before the softmax was the decisive fix [DD]. A multiset representation with Soft Actor-Critic is described as the first learning agent making all decisions, beaten only by search in some kingdoms [DG, snippet, blocked]. The commercial app embeds card components and lets a network guide look-ahead [DT].

### NetHack

In the 2021 challenge the best symbolic bot (AutoAscend: a state parser plus a behaviour tree of scripted strategies) scored a median nearly three times the best neural agent; a leading neural entry had trained for 10B steps per role on 16 V100 [NH]. Stated reasons: encoded wiki knowledge, hierarchy, long-horizon credit assignment. Cloning AutoAscend from 3.2B transitions plus RL fine-tuning reached a mean score of 1,551 against 8,556, with sub-log-linear returns to data [HH]. Language models scored 1.5% progression in 2024 [BA]; one ascension was reported in September 2026 (1 of 3 runs, open-book, with tools and guards the model wrote itself) [NL]. A 2026 review still finds no approach with significant progress [NR].

This does not carry over. NetHack offers horizons of 10^4–10^5 steps and menu-hidden state. Slay the Spire has about 100 strategic decisions per run and an exact, fast simulator; here the rule bot is last and RL policies lead.

## 2. How deck building was solved

1. **Fixed order or tier list.** Won while the pool was static (LOCM, Tales of Tribute) and lost once it was not [LS] [TT]. The StS rule bot clears 0.2 acts at A20 [SB].
2. **Weights tuned through playouts** (harmony search, coevolved buy menus, MAP-Elites) [LS] [DH] [ME].
3. **Imitating human picks.** The metric is agreement, not strength. In StS a model with card-ID embeddings predicted human picks best (55.8%) and played worst (50 seeds, weak evidence); the public 77M-run dump has a 1.5% A20 win rate [DS].
4. **RL drafter against a frozen battle agent, reward = battle result** [V20]. Hours of CPU.
5. **End-to-end RL, deck and battle sharing card embeddings** [BL] [BH]. +9 to +16 points over method 2 or a separately trained draft, at datacentre scale.
6. **Whole-run PPO, with or without search for the fights** [SE] [SM] [ZS]. The delayed pay-off is handled by sample count (10^5–10^6 runs), gamma near 1, progress shaping, and curricula that put training where runs die.
7. **Oracle distillation.** Mentor first labelled each option with MCTS over two hallway fights, an elite and a boss, and trained a listwise ranker (44.9% top-1 agreement) [SM-EX]. PPO replaced it.

No strong system learned deck building from a value of the deck in isolation. The signal is always the outcome of games played by the agent that will pilot the deck.

## 3. How card sets and piles were encoded

| Encoding | Used by | Evidence |
|---|---|---|
| Count vector into a linear layer (equal to sum-pooled embeddings) | Mentor before PPO [SM-EX]; this project | Enough for a value with win-AUC 0.67–0.77 |
| Learned card embedding, mean-pooled, shared across phases | ByteRL [BL] | Sharing the embedding 55%, sharing the picked-set feature 53%, each vs not sharing |
| Card as a feature vector or text embedding, not an ID | [SE-IN] [V20] [DS] [YG] [SW] | Needed for unseen cards; hardly matters for known cards [MG] |
| One token per hand card, pooled multisets for piles, attention across entities | [ZS] [SE-IN]; generic form [ST] | No published ablation against pooling |
| Pre-computed interaction features (per-target damage, lethal flags) | [SE-IN] [S2] | Authors' choice; no ablation |

Draw order is hidden by sorting or pooling the draw pile [SE-IN] [ZS]. The zero-search paper states that its observation includes the enemy's internal next-move ID [ZS]; under this project's rules that is a leak.

## 4. What worked with small compute

- Search with a tuned hand evaluation: every LOCM fixed-pool winner, Tales of Tribute, Foul Play, Mentor's combat [LS] [TT] [PF] [SM-EX].
- A small network that guides or truncates rollouts, not one that replaces search [SW] [ZB] [PH].
- RL on one decision surface with a frozen game-play agent: 30 minutes to 3 hours [V20] [DH] [DD].
- Whole-run PPO on one consumer GPU in days [SE-IC] [SM]. Not small: ByteRL, the NetHack neural entries, Yu-Gi-Oh, Metamon.

## 5. Design lessons for this agent

**Coupling.** Keep the search for fights and learn the strategy by RL with that same search in the loop: Mentor's layout, and the best cell of the zero-search swap table. Training fights must use the honest search; a reduced budget is acceptable (Mentor's cheaper-simulation test gained 2.5 points, p=0.028, below its own 3-point bar) [SM-PB]. Do not replace fights with a learned outcome model [SM-SU]. This cuts against the "fight-outcome model composed along the map" proposal in `c2_strategy_layer.md` §6 item 2, unless that model first passes a closed-loop survival test on Acts 1–3.

**Action head.** Score each legal option with MLP(option embedding, pointed-to entity, state summary) and a masked softmax [ZS]. One head serves card rewards, shops, rest sites, paths and events, and replaces V(after) − V(now) with a directly optimised preference.

**Encoder.** The present 666-feature input already is a sum-pooled card embedding, and nobody has shown that attention over card tokens beats pooling for a known pool. Start with pooled card, upgraded-card and relic embeddings plus the option's own embedding: 0.2–1M parameters, a few ms on one core [SE-AR].

**Reward and value.** Win plus progress (0.02 per floor, 0.1 per act, win 1–3), gamma 1 or above 0.9995 [SE-IC] [BH]. Auxiliary value heads for P(win), floors remaining and end-of-fight HP fraction [ZS]; the last is the bridge to a combat terminal value that knows the run.

**Curriculum.** Uniform over ascensions [SM-PB]. Restart episodes from stored act starts and elite or boss fight starts, weighted by where fresh runs are lost: +3.6 points for one character, +0.4 for another [ZS]. Boss practice with strong decks [SE-IC]. 53% of this project's A20 deaths are on boss floors (`c2_strategy_layer.md` §6).

## 6. Smallest experiment here and its cost

**PPO on the card-reward decision only, at A10, with the current search playing every fight.**

- Policy: pooled embeddings as above, option-scoring head, value head, initialised by cloning the current A10 picker from existing logs.
- Training: 60 iterations × 2,048 runs, stochastic picks at card rewards, everything else frozen. Reward = the existing target, min(floor,51)/51 + win, gamma 1. Training seeds disjoint from validation seeds. Keep entropy low after cloning: a copied entropy schedule cost Mentor 13 points [SM-PB].
- Cost: 123k runs at 3.2–4 s per run at most (`program.md`: "3.94 s per game", "3.18 s per game") = at most 110–137 core-hours, about 3.5 h on 40 vCPU. Updates on a net this small: about one CPU-hour in total. Evaluation on 2,000 paired seeds: about 4 core-hours. No GPU.
- Engineering: sampling and logging at one hook in the C++ agent, an embedding lookup in the existing inference header, a PPO trainer.
- Gate: paired McNemar on 2,000 seeds resolves about +3 points. Below +1.5 points, one surface carries too little signal; go to the multi-surface version.
- Why this size: Mentor's screen of 10,240 training runs resolved only effects of 8 points or more; its campaigns of 38,400 to 144,000 runs produced +7.5 to +15.5 points [SM-SU] [SM-PB].
- If it passes: add rest sites, shops and paths to the same head, then restarts. That is about 10^6 runs, roughly 1,000 core-hours, a day on the current fleet. A GPU pays only if the encoder becomes a token transformer (updates 15× faster in [SM-PB]).

## 7. Top transfers and dead ends

Confidence figures are my estimates.

| # | Transfer | Confidence |
|---|---|---|
| 1 | PPO on the whole strategic policy with honest search in the loop, option-scoring head, win + progress reward | High, 70%, for at least +5 points at A10 and A20 once three or more surfaces are covered [SM-PB] [ZS] [BL] [SE-IC] |
| 2 | A learned combat policy (PPO on fights, then on runs) as the prior at every tree depth and as the playout policy; not a leaf value | Medium, 50% [ZB] [PH] [SM]; +5.6 points from shallow look-ahead in [SE-IC]. Each rollout costs 5–10× more [ZB] |
| 3 | Failure-weighted restarts and boss practice | Medium, 45% [ZS] [SE-IC] |
| 4 | Auxiliary value heads, with V(post-fight state) as the fight's terminal utility | Medium-low, 35%; used in [ZS] without an ablation |
| 5 | Expert winners' data as a weak prior and a source of boss-practice decks | Low, 25% [SE-IC]; mass human data hurts [DS] |

**Dead ends.**

- A learned fight-outcome surrogate in place of combat [SM-SU].
- Imitating mass human picks, and pick agreement as a metric [DS] [MG].
- Static tier lists as the end state [LS] [SB].
- Distilling boss search back into the policy (one result) [ZS].
- Cloning a rule bot at scale [HH].
- Language models as the player: Act 3 in 20% of runs at tens of dollars per run, outside the CPU budget [SB].
- A larger encoder before the training signal changes [MG] [V20].
- Comparing against clairvoyant-search numbers [SM-PB].

## Sources

- [SE] https://github.com/jackybabes/spire-engine
- [SE-IN] https://github.com/jackybabes/spire-engine/blob/main/docs/network-inputs.md
- [SE-AR] https://github.com/jackybabes/spire-engine/blob/main/docs/architecture.md
- [SE-IC] https://github.com/jackybabes/spire-engine/blob/main/docs/models/ironclad-v1.md
- [SM] https://github.com/tfriedel/spire-mentor
- [SM-PB] https://github.com/tfriedel/spire-mentor/blob/main/docs/ppo-champion-training-playbook.md
- [SM-SU] https://github.com/tfriedel/spire-mentor/blob/main/docs/combat-surrogate-investigation.md
- [SM-EX] https://raw.githubusercontent.com/tfriedel/spire-mentor/main/docs/run-ai-explainer.html (describes the pre-PPO design)
- [ZS] https://arxiv.org/abs/2611.12088 (preprint; read as https://arxiv.org/html/2611.12088)
- [S2] https://github.com/shuchi-lab/sts2-rl-research
- [AS] https://github.com/AlphaSpire2/alphaspire
- [AA] https://github.com/JustinoChan/AscensionAI · [AA-R] https://justinochan.github.io/AscensionAI/
- [SB] https://arxiv.org/abs/2610.12596 (read as https://arxiv.org/html/2610.12596, first 100k characters)
- [SA] https://github.com/AttemorySystem/spire-agent
- [DS] https://dev.to/doronsinger/part-3-good-models-finish-last-2n6a
- [BL] https://arxiv.org/abs/2303.04096 (read as https://ar5iv.labs.arxiv.org/html/2303.04096)
- [BH] https://arxiv.org/abs/2303.05197 (read as https://ar5iv.labs.arxiv.org/html/2303.05197)
- [BX] https://arxiv.org/abs/2404.16689 [snippet]
- [LS] https://arxiv.org/html/2305.11814v2
- [V20] https://www.sbgames.org/proceedings2020/ComputacaoFull/209690.pdf
- [SW] https://arxiv.org/abs/1808.04794 (read as https://ar5iv.arxiv.org/html/1808.04794)
- [ZB] https://skatgame.net/mburo/ps/cig17-hsai.pdf
- [PH] https://github.com/peter1591/hearthstone-ai
- [ME] https://arxiv.org/abs/2112.03534 (abstract)
- [TT] https://arxiv.org/html/2305.08234v4
- [MG] https://arxiv.org/html/2407.05879v1
- [MW] https://arxiv.org/abs/2009.00655 (abstract)
- [YG] https://github.com/sbl1996/ygo-agent/blob/main/README.md
- [PM] https://arxiv.org/html/2504.04395v2
- [PF] https://pmariglia.github.io/posts/foul-play
- [PC] https://arxiv.org/abs/2503.04094 (abstract)
- [DH] https://arxiv.org/html/2405.06846v1
- [DD] https://ianwdavis.com/dominion2.html (read through the mirror https://hn.jakechampion.name/reader/https://ianwdavis.com/dominion2.html)
- [DG] https://ojs.aaai.org/index.php/AIIDE/article/view/27518 [snippet, blocked]
- [DT] https://www.templegatesgames.com/dominion-ai
- [NH] https://arxiv.org/html/2203.11889v1
- [HH] https://arxiv.org/abs/2305.19240 (read as https://ar5iv.labs.arxiv.org/html/2305.19240)
- [BA] https://arxiv.org/html/2411.13543v1
- [NL] https://kenforthewin.github.io/blog/posts/llm-nethack-ascension/
- [NR] https://iclr-blogposts.github.io/2026/blog/2026/revisiting-the-nle/
- [ST] https://arxiv.org/abs/1810.00825 (abstract)
