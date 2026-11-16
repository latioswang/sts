# W1 — Slay the Spire prior work: what the outside world knows (stage 1, 2026-11-16)

Tools: WebSearch and WebFetch were available. Every URL below was opened in this session unless tagged **[snippet only]** (seen in search results, page not opened). Not reachable: `tomokinotes.pages.dev` (outside this session's allowed network domains; it has to be allowed in the network settings before its StS2 bot posts can be read), the Edinburgh thesis PDF (over the fetch size limit), and the Scribd and untapped.gg guides (blocked). Searches used generic terms only.

## 0. Three findings that change the picture

1. The strongest published non-LLM agent has this project's shape (fair chance-node MCTS in combat plus a learned run-level layer), but its run-level layer is one policy trained by PPO on full runs with the real search inside the environment: Spire Mentor, StS2 Ascension 10 Ironclad 27–30%.
2. The two best frontier LLMs, with no search, now kill the A20 Heart in 18–33% of runs (Epoch, November 2026). Their tactics are weaker than search; strategic knowledge carries them. This project's A20 Heart figure is 0.3%.
3. Independent post-mortems agree on where runs are lost: the deck built in Acts 1–2, and HP hoarded instead of spent. Combat execution in ordinary fights is not the main loss.

## 1. Search and learning agents

### 1.1 Spire Mentor (T. Friedel, 2026): the closest analogue
**What.** Rust port of StS2 plus an agent. Combat is MCTS with chance nodes for draws and monster rolls ("non-clairvoyant"), a 216-feature leaf evaluation, a learned playout net, and for Ironclad a move prior in elite and boss fights. Out of combat, a per-character transformer policy takes every non-combat decision (path, rewards, shop, events, rest, removals).
https://github.com/tfriedel/spire-mentor · https://tfriedel.github.io/spire-mentor/ · https://tfriedel.github.io/spire-mentor/training.html

**Result** (self-reported, own simulator). Ironclad A0 96.0%, A8 84.5%, A9 56.4%, A10 27.2% at 8,000 simulations; newest champion 30.0% at A10 (400 runs, interval 25.7–34.7). The champion of two weeks earlier scored 11.0% at A10. An independent re-run in another host gave Ironclad 16.8% of 400 (section 1.2).

**Mechanism.**
- PPO steps are non-combat decisions only. Fights are resolved by the real MCTS inside the environment at a reduced budget: 250 simulations, 1,000 at bosses. Deployment uses 1,000/4,000 up to 4,000/16,000.
- Return = 0.5 · floors/total + 0.5 · won, γ = 1, potential shaping on HP fraction and upgrade count.
- Ascension is sampled uniformly from A0 to A10 for every game.
- Promotion needs a pre-registered paired-seed gate (McNemar test).
- Side results: a cheap combat surrogate in place of search was rejected, because it made fights look easier (survival 0.64–0.68 against 0.29 true). Up-weighting hard ascensions cost about 2 points. Prioritised level replay gave nothing (−1.1 ± 1.3). "Cheaper searches, more games" gained 2.5 points. A silently disabled learning-rate anneal cost about 6 points. Eight root-parallel trees of 1,000 simulations play worse than one tree of 8,000. The belief that high ascension is a combat ceiling was found wrong: one run-policy leg gave Defect +18.5 and +21.0 points at A7 and A8.

**Needs.** 38,400–144,000 full runs per training leg (36–62 hours) on 24 cores. The GPU does only the PPO update (34 s per 960 games); collection was 97% of wall-clock. Search speed: 1,000 simulations ≈ 14 ms on one thread.

**Transfer: high.** The cost is CPU collection, the architecture class is the same, and this project's strategy layer is a patchwork of heuristics and value-net deltas, not one policy optimised on run outcome. Caveats: StS2 A10 is easier than StS1 A20 Heart, no component ablation is published, and the independent re-run was lower than the self-report.

### 1.2 Zero-Search RL for StS2 (Chen and Xie, arXiv 2611.12088, November 2026)
**What.** One 6.57M-parameter entity transformer plays every decision, combat included, by greedy argmax with no lookahead. https://arxiv.org/html/2611.12088

**Result.** A10 Ironclad 15.59% (2,560 games); five-character mean 8.05%; A0 mean 80.2%. Mentor in their host: mean 8.8% against their 8.2%.

**Mechanisms worth borrowing.**
- Counterfactual replay attribution. Of 125 final-boss losses, 72% could not be won by any of 12 replays that re-decided all of Act 3, and 58% not by replaying the boss fights with the same dice. Their conclusion: the deck built in Acts 1–2 was the error.
- Layer crossing. Mentor's search played on decks built by their policy beat their own combat play (gained 83 seeds, lost 57, p = .034). Each agent's run policy suits its own combat player.
- Failure-driven restarts. Restart points (seed plus action prefix) at act starts and combat starts, sampled where runs are lost: Regent 10.55% against 6.97% uniform at A0; no effect for Silent.
- The policy barely conditions on ascension (flipping the input changes 5.9% of actions) while its win-probability head moves from 0.16 to 0.80. The value head knows more than the policy uses.
- Distilling search into the policy failed: agreement with the search rose, wins fell (60–62 against 74).

**Needs.** 0.9–1.7 billion decisions per character on GPU servers; 21–27 ms per decision on one core.

**Transfer.** The end-to-end recipe does not transfer: GPU-scale, and it still trails search. The replay-attribution and layer-crossing diagnostics transfer directly (**high**). Restarts: **medium**.

### 1.3 sts-rl-agent on sts_lightspeed (Jialeiv, 2026)
**What.** A learned non-combat scorer (100k-parameter MLP, REINFORCE, about 15,000 games, roughly 10 minutes on 12 cores) on top of the simulator's stock search. https://github.com/Jialeiv/sts-rl-agent · https://huggingface.co/Jialeiv/sts-rl-agent

**Result.** A0 Ironclad, 50 seeds, 50,000 simulations: 6% → 14% wins; 0% at A20. The combat search there sees the true draw order; the simulator's README lists its tree search as "knowing the state of the game's rng" (https://github.com/gamerpuppy/sts_lightspeed).

**Mechanism and negatives.** The largest gain came from map pathing. Six attempts to compress combat into a network failed: behaviour cloning, DAgger, REINFORCE fine-tuning, an attention scorer, 1-ply value lookahead, and policy/value-guided PUCT, which lost to blind rollouts at equal budget (26.7 against 33 floors).

**Transfer.** Nothing new in strength; this project is far ahead and leak-free. It is independent confirmation, on the same simulator, that learned leaf and prior networks lose to rollouts at equal budget (**high** confidence in the negative).

### 1.4 Hand-written and small-scale agents
- Bottled AI: one-turn graph search with a comparator of about 40 values, plus hand priority lists; best strategy 52% (Watcher, ascension not stated). https://github.com/xaved88/bottled_ai. At A20 Ironclad it clears Act 1 in 20% of runs (SpireBench, section 2.2).
- D. Singer's series (Silent): a port of that bot averages floor 34 and wins 26%; his learned evaluators' apparent gains vanished with more games; dominance pruning removed 90–94% of states; without foresight of draws, elite losses rose from 1 to 3 of 8; elites are where the agent bleeds (28.9 HP per fight against a human's 5.4). https://dev.to/doronsinger/part-6-what-not-to-consider-54g5 · https://dev.to/doronsinger/part-11-pick-your-battles-d3p. His card-pick models trained on human logs: better pick prediction gave worse play, and the public dump he used has a 1.5% A20 win rate. https://dev.to/doronsinger/part-3-good-models-finish-last-2n6a
- AlphaStS: per-fight AlphaZero with win and HP value heads; no results published. https://github.com/MANGO1234/AlphaStS
- AscensionAI: PPO against the live game, 16,900 games, no recorded victory at A0. https://justinochan.github.io/AscensionAI/AscensionAI_Technical_Writeup.pdf
- AutoClad: imitation of a scripted agent on sts_lightspeed, no results. https://github.com/benmuth/AutoClad
- Edinburgh undergraduate thesis: MCTS battles, card rewards scored by simulated win rate, A10. https://project-archive.inf.ed.ac.uk/ug4/20233951/ug4_proj.pdf **[snippet only]**

**Transfer: low.** Dominance pruning is the one mechanism of interest and belongs to the combat-search survey.

## 2. Language-model players

### 2.1 Epoch AI SpireClimb (October–November 2026)
**What.** Screenshot-and-click harness, maximum reasoning, persistent notes, wiki search; 15 fixed seeds per character. https://epoch.ai/benchmarks/spireclimb · https://epoch.ai/publications/ai-systems-are-improving-rapidly-at-slay-the-spire-and-its-sequel

**Result.** StS1 A20 Heart, 60 runs: GPT-6.1 Sol 20% (Ironclad 27%); GPT-6 Astra 17%, rising to 33.3% after a harness bug that withheld the Neow bonus was fixed (Sol moved to 18.3%, which Epoch calls noise); the Claude models 0%. At A0 the top four win 92–98%.

**Mechanism.** Pre-trained game knowledge, visible in every model's reasoning.

**Transfer.** An LLM per decision breaks the fixed-compute rule (seconds per decision). The result is evidence about where the value is: weak tactics plus human strategic knowledge is far ahead of this project on the Heart track. **High** confidence in that reading. **Low** confidence that an LLM-labelled teacher would distil well: LLM card picks agree with human picks at only ρ = 0.16 (section 2.2).

### 2.2 SpireBench (Lowe and Keemink, arXiv 2610.12596, October 2026): the best failure analysis available
**What.** 20 LLMs through a text harness on CommunicationMod, A20 Ironclad, 10 runs each, compared with 206,082 human A20 Ironclad runs and four expert streamers. https://arxiv.org/html/2610.12596

**Result** (share of runs clearing Act 1 / 2 / 3 / Heart). Claude Fable 5.1 90/50/20/0; GPT-6 Astra 90/50/10/0; GPT-5.6 Sol 70/20/10/10; human population 47/15/7/4; Baalorlord (687 runs) 89/77/70/63; panacea108 92/78/71/65. Cost $8–62 per run.

**Mechanism of failure, measured.**
- Elites before the Act 1 boss: models 0.9–1.4, population 2.0, streamers 2.6.
- Campfires: models rest at 57–75%, population 23%, streamers 9%. HP at the Act 1 boss: models 90%, streamers 53–56%.
- Human runs with the models' cautious Act 1 profile clear Act 2 in 15.5% of cases against 38.0%. Each Act 1 elite multiplies the odds of clearing Act 2 by 1.38; each rest by 0.85.
- Card skipping: models 8–9%; humans 21% / 39% / 53% in Acts 1 / 2 / 3. Deck at the Act 1 boss: 19.3 cards against 16.8. At death, models hold 1.4 fewer upgrades and 1.9 fewer relics than humans on the same floor.
- Deaths: 27% at the Act 1 boss; elite rooms kill 17.8% per fight against 2.7% for hallways.
- Cross-run memory did not help: 17% of stored lessons were false, and cautious advice outnumbered risky advice 7 to 1.

**Transfer.** The behavioural table is a ready-made audit sheet (**high** as a diagnostic). Whether this agent shares the bias has to be measured. Per the sibling note `c2_strategy_layer.md`, its A0, A10 and Heart tracks rest below an HP threshold (50–70%) and gate elite value on HP ≥ 60%, which is the pattern experts do not follow.

### 2.3 Others
AgenticSTS (StS2 A0; a skill-memory layer took 3/10 to 6/10, p ≈ 0.37): https://huggingface.co/papers/2607.02255. Orak (floors reached only; best model about equal to a novice human): https://arxiv.org/html/2506.03610. Xiao and Yang (LLMs as difficulty testers): https://arxiv.org/abs/2410.02829. No transferable mechanism.

## 3. Top humans

- **Win rates.** Strong players exceed 50% at A20 Heart; XecnaR reports 80–95% depending on character (Epoch article above). A20 Ironclad Heart kills: 46–65% for four streamers (SpireBench). Population: 3–4% (https://megacrit.com/news/2026-5-22-neowsletter-issue-22/ and SpireBench).
- **Streaks** (one forum list, October 2022): Ironclad 18 (Lifecoach1981), Silent 16, Defect 11, Watcher 38, rotating 16. https://steamcommunity.com/app/646570/discussions/0/5595184948711282784
- **Headroom.** ForgottenArbiter estimates optimal play at A20 Heart at 75% or more for Ironclad. https://forgottenarbiter.github.io/Is-Every-Seed-Winnable/
- **Conversion.** Experts who clear Act 3 kill the Heart about 90% of the time (63/70, 65/71); they lose most in Act 2 (89 → 77). This project's Heart wins are a few percent of its A20 wins, so the decks it brings to Act 4 are not Heart decks. [my inference from the two sets of numbers; the tracks differ in key collection]

**How experts say they win with Ironclad at A20** (Panacea's guide; panacea108 is the 65% streamer above): https://gameplay.tips/guides/slay-the-spire-ironclad-guide-high-winrate-ascension-20-heart-killing.html
- Path for the most elites, with about three hallway fights first; avoid three elites with no campfire; events beat late hallways; no shop under about 170 gold.
- An upgrade is worth more than a hallway fight; rest only if the path ahead would kill.
- Before the first elite, take damage that saves HP now, and prepare for the specific Act 1 boss. In Act 2, answer Book of Stabbing (Weak/Vulnerable), Slavers (turn-one or area damage) and Champ. Become greedy for engines only in Act 3: Corruption with Dark Embrace or Feel No Pain, Fiend Fire, Barricade, Reaper, Offering.
- Potions go where they save the most HP; Strikes are removed before Defends.

An intermediate player's log of 61 A20 Ironclad losses puts 37 in Act 2 (Gremlin Leader 7, Slavers 7, Collector 5, Book of Stabbing 4) and 3 at the Heart; the named error is slow cards taken early. https://dingbangliu.codeberg.page/posts/2025-07-30-stsinsights2.html. A second guide lists Heart needs: at least four turns because of the damage cap, block every turn, draw to absorb statuses, Disarm, Flame Barrier. https://steamcommunity.com/sharedfiles/filedetails/?id=2429404117

## Top transfers

1. **One run-level policy trained by RL on full runs with the real combat search in the loop** (Spire Mentor). Non-combat decisions are the steps; return mixes floor progress and win; ascension sampled uniformly; reduced search budget in training, full budget at test; paired-seed promotion gate. CPU-bound. Confidence: high that it applies, medium on the size of the gain at A20 Heart.
2. **Re-price HP toward the expert profile** (SpireBench): more Act 1 elites, upgrade instead of rest, skip more cards in later acts, smaller and more upgraded decks. Audit the agent against the table in 2.2 first; the RL in (1) should find this unaided, and the audit shows whether it is missing today. Confidence: high as a diagnostic, medium as a lever.
3. **Counterfactual replay attribution and layer crossing** (Zero-Search). Replay lost runs from act starts and from the fatal fight with fresh dice, to split losses into "the deck could not win" and "the play lost it". Cheap with a cloneable simulator, and it decides where the effort goes. Confidence: high.
4. **Restart-state training for late-game situations** (Zero-Search). Save states at act and boss starts and train or evaluate run-level models from them, weighted to where runs die. It addresses the scarcity of Act 4 data at a 0.3% win rate. Confidence: medium (it helped one character of two).
5. **Search-side pieces from Mentor**: a learned playout policy in place of a leaf value net, a move prior only in elite and boss fights, and more simulations where runs are decided (4× at bosses). No ablation is published, and shifting budget between fights needs a ruling under the fixed-compute rule. Confidence: low to medium.

## Dead ends

- **Leaf value nets, policy/value-guided PUCT, distilling search into a network.** Failed here, in sts-rl-agent (six ways), in Zero-Search (distillation lost wins) and in Singer's series (the gain vanished with sample size).
- **End-to-end RL without search.** It needs about a billion decisions per character on GPUs and still trails search; the small-scale version (AscensionAI) never won a run.
- **A cheap combat surrogate inside run-level training.** Mentor measured it: fights look twice as survivable, so the run policy learns the wrong price of risk.
- **Imitating public human run logs.** The large dump is weak players (1.5–4% at A20), and better pick prediction produced worse play. Expert logs exist but number in the hundreds.
- **An LLM at decision time, or LLM cross-run memory.** Seconds and dollars per decision; the memories were 17% false and biased toward caution.
- **Root-parallel trees, prioritised level replay, up-weighting hard ascensions.** Mentor measured each as neutral or negative.
- **Hand-written priority lists.** Bottled AI clears Act 1 at A20 in 20% of runs.
