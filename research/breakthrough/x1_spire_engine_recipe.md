# x1 — spire-engine (jackybabes): how the 93 / 75 / 22% Ironclad agent was made

Read at source on 2026-11-16, public HEAD (commit `420d17b`); files opened as raw text, nothing cloned or run. **[inferred]** = my deduction; **[unverified]** = could not be confirmed.

**Sources opened.** `B` = `https://raw.githubusercontent.com/jackybabes/spire-engine/420d17b19ec3dd112f3228c1d0770f78b9914bd5/`

| Key | URL |
|---|---|
| README | `B/README.md` |
| IC / SI / DE / WA | `B/docs/models/ironclad-v1.md`, `silent-v1.md`, `defect-v1.md`, `watcher-v1.md` |
| ARCH / NI | `B/docs/architecture.md`, `B/docs/network-inputs.md` |
| CHK | `B/docs/checklist/README.md` and `cards.md`, `relics.md`, `monsters.md`, `events.md`, `potions.md`, `powers.md` |
| TR / TF / PH | `B/python/training/train_runs.py`, `train_fights.py`, `pretrain_human.py` |
| RM / FM / PPO | `B/python/core/run_model.py`, `fight_model.py`, `ppo.py` |
| FS / SE / SRS | `B/python/search/fight_search.py`, `search_eval.py`, `B/crates/sts-py/src/search.rs` |
| ENC / ENR / PR | `B/crates/sts-encode/src/encode.rs`, `encode_run.rs`, `B/crates/sts-py/src/practice.rs` |
| HB / HD / HR | `B/python/human/human_bosses.py`, `human_dataset.py`, `human_runs.py` |
| EW / FX / RP / SYN / ER | `B/python/training/eval_worker.py`, `B/python/core/fixed_shapes.py`, `B/python/analysis/run_probe.py`, `synergy.py`, `B/python/release/export_release.py` |
| NET | `B/web/networks.json`; live copy `https://spire-viewer.pages.dev/networks.json` |
| GH | `https://github.com/jackybabes/spire-engine` — `/commits/main/`, `/commit/c06714a…`, `/releases`, `/tags`, `/issues` |

Two web searches found no write-up outside the repository [unverified beyond that]; the model cards and docstrings are the write-up. The `notes/*.md` files the code cites return 404.

## 1. What it is and what it claims

- A Rust rewrite of Slay the Spire 1 (four characters, through Act 4), MIT-licensed, plus "one network per character. Each one plays whole runs on its own: Neow, the map, card rewards, shops, events, rest sites and every turn of every fight" [README]. Public history is three commits dated 2026-11-09: "Exported from the development repository (the decompiled game, real-game recordings and notes stay there)". No issues, releases or tags [GH].
- Results, greedy play (argmax) on fixed seeds 300,000 + j [TR `EVAL_SEED`]:

| Network (date) | A0 | A10 | A20 | Runs | A win is |
|---|---|---|---|---|---|
| ironclad-v1 (2026-10-21) | 93.1% | 75.2% | 21.9% | 1,024 each | Act 3 boss (both bosses at A20) [IC] |
| silent-v1 | 75.8% | 47.9% | 2.9% | 1,024 / 1,024 / 1,536 | Act 3 boss [SI] |
| defect-v1 | 27.5% | 10.4% | 0.9% | 512 / 1,024 / 1,024 | the Heart [DE] |
| watcher-v1 | 43.8% | 26.3% | 8.0% | 512 / 1,536 / 1,536 | the Heart [WA] |

- Ironclad detail: average floor 49.0 / 45.6 / 35.7; reached Act 3 in 96 / 83 / 47%; "about ±1.2 points at A20"; no run timed out; it never takes the Sapphire key [IC].
- With look-ahead: "27.5% at A20 and 76.1% of those boss fights" (against 69.0% without); "not part of this release and was not measured again" [IC]. The run count behind 27.5% is not stated; the script's default is the same 1,024 seeds [SE] [inferred].
- Simulator only: "It plays this engine, not the game itself" [IC].
- Hidden information: it sees "your hand, the draw pile's contents but not its order, the monsters' intents, your relics, potions and deck. It doesn't search ahead or peek at the random numbers" [README]. The draw pile is encoded sorted, and Runic Dome zeroes intents [NI, ENC].
- Compute at play time: one forward pass, "a few milliseconds a decision in WebAssembly" [ARCH]; about 1,200 positions a second on 2 CPU threads [EW]. The look-ahead is "about 20 times slower per run" [WA].

## 2. Architecture

**One network.** `RunNet` extends `FightNet`: one transformer encoder and one set of embedding tables serve fights and every other screen; only token layout and output heads differ [RM, FM].

**Observation.** Each decision is one row of integers (ids into embeddings) and one of floats (raw game numbers, each fed as symlog(x) and x/100): 1,081 ints and 2,114 floats for ironclad-v1 [IC, FM].

*Fight tokens* [FM, NI, ENC]:
- global (scalars, 20 at HEAD: HP, block, energy, turn, pile sizes, gold, ascension, cards played…; character; pending selection);
- player (scalars, powers with amounts summed through a small MLP);
- one pooled relic token (with counters);
- three pile tokens (draw, discard, exhaust: mean card embedding plus upgrades);
- 10 hand-card tokens: card id plus cost this turn, upgrades, damage and block "as shown" (Strength, Weak, Dexterity, Frail applied), magic number, playable, needs target, exhausts;
- 6 monster tokens: id, intent, HP, max HP, block, intent damage per hit and hits, flags, powers;
- 5 potion tokens; up to 64 tokens for a pending card selection; a run token.
- Each (card, target) pair also gets damage, "whether it kills" and the share of HP + block removed [NI, FM]. Encoding 1 (the Ironclad's) had displayed numbers and one-hit damage per monster; hits, whole-play damage, monsters' last two moves and orbs came in encoding 2 [NI].

*Between fights* [RM, ENR]: a run token (screen, act boss, room, event, and scalars: HP, gold, floor, act, ascension, deck size, keys, purge cost, potion slots…), pooled relics, potions, **one token per deck card** (up to 80: id and upgrades), and up to 64 option tokens. "Every screen is a list of up to 64 options" (card, relic, potion, Neow blessing, map node, event option, shop item, rest action), each with numbers such as upgrades, cost, price and affordability. The map is not given as a graph: each selectable node carries, per room type, "the fewest and most rooms of that type on any path from the node to the act's boss" [ENR].

**Network.** Pre-norm transformer encoder, d 128, 6 layers, 4 heads, feed-forward 256, no dropout; 1,074,188 parameters (Defect 8 layers, Watcher 3) [IC, DE, WA, FM].

**Actions.** 171 fixed slots, masked to legal moves: play(hand 0-9, target 0-6), potion(slot 0-4, target), end turn, pick(option 0-63), confirm [ENC]. Logits come from the tokens they refer to: play(h, t) is an additive-attention score of hand token h against target token t; pick(k) from option token k; end turn from the global token — "a card is judged the same way wherever it sits in the hand" [FM]. Between fights: pick from option tokens, confirm (proceed / skip / leave) from the run token, drink or discard from potion tokens [RM].

**Value.** One head on the run token, "so it estimates how the whole run will go", in fights too. A second head scores practice fights during training only [RM].

**Look-ahead** (code is public; not in the released browser model) [FS, SRS, SE]:
1. At a fight move outside a card selection, candidates are the policy's 4 likeliest legal moves plus ending the turn.
2. Each is tried on 8 copies of the run in which "the draw pile is reshuffled (unless the player has Frozen Eye) and the fight's random streams are reseeded"; every candidate sees the same redraws.
3. Each copy is played on by the network's own greedy moves until the player's next turn (1 turn, at most 30 moves) or the fight's end.
4. The leaf is scored by the network's run value; a death scores 0. A candidate's score is the mean over copies — "no maximum over noisy estimates".
5. The best candidate replaces the policy's move if it beats it by a margin, optionally after shrinking the gain by standard errors of the paired difference.
A per-potion bonus outside boss fights was added, since otherwise "the search drinks potions in hallway fights the network keeps for later" (2.5 a run against 0.5, "fewer runs won") [FS].

## 3. Training recipe

**Algorithm.** Clipped PPO with GAE, value loss, entropy bonus, early stop on approximate KL, Adam (eps 1e-5), bfloat16 autocast [TR, PPO]. Script defaults: lr 1e-4, λ 0.95, clip 0.2, entropy 0.005, value weight 0.5, 4 epochs, minibatch 4,096, target KL 0.05, gradient clip 0.5, γ 0.999 [TR]. The released run's `args.json` is not published; which defaults it overrode is [unverified].

**Stages for the Ironclad**, 2026-10-14 to 10-19, "each starting from an earlier one's weights" [IC]:
1. A fight network on random-deck single fights (+1 for a win, potential HP/max HP − 0.3 × monster HP share) [TF].
2. Whole runs (ironclad-runs-1 to 13).
3. Block and exhaust practice fights, because "the run network only took cards it already fought well with, so it never held such a deck and never learned to fight with one" [PR].
4. Boss practice from winning human A20 boss fights (7 runs).
5. "Habit rewards, a win reward of 3, learning-rate annealing and gamma 0.9995" (12 runs).

**Final run** (ironclad-runs-habits-12): ascension 20 only; 300 updates of 2,048 games × 128 steps; the release is update 200 = 52,428,800 steps, "about 1.6 hours" [IC] — about 9,100 steps a second [inferred]. Runs last "about a thousand decisions" [TR]. Total steps over the chain (about 35 named runs [inferred]) are not reported [unverified]; the README says "hundreds of thousands of runs" per network. Other characters' final runs alone: Silent 314.6M steps, Defect 157.3M, Watcher 943.7M [SI, DE, WA].

**Hardware.** "One consumer GPU (an RTX 5070 Ti since 2026-10-18)", 16 GB, under WSL [IC, ARCH].

**Reward** [IC, TR]:
- progress: 0.02 a floor, 0.1 an act; a win: 3 ("3 lifted A20 from about 11% to 17%" [TR]);
- potential-based shaping ψ = 0.2 × HP/max HP − 0.05 × monster HP share, paid as γψ′ − ψ;
- habit rewards, "not potential-based on purpose": a growing cost per card added past a free deck size (earned back on removal), a penalty for resting at high HP, a bonus per upgrade at a rest site, a cost per floor for each basic Strike and removable curse kept, penalties for potions drunk in hallway fights, for skipping an affordable removal and for unspent energy, a bonus per elite won. The weights are not published [unverified].

**Curriculum, restarts, human data.**
- Practice fights are "30% of games; 60% of those are boss fights from winning human A20 runs" (the winner's deck, relics, potions, HP) [IC, HB]. Their return is 0.5 + 0.5 × HP share left [TR].
- "A small cross-entropy pull towards human A20 winners' choices between fights" runs beside PPO (default weight 0.05, card rewards) [IC, TR]. The data is "Mega Crit's 2020 metrics dump"; it holds choices and decks, "fights are not recorded" [HR].
- Earlier runs widen the ascension range; the last trains at A20 only [TR, IC].
- Exploration is policy sampling plus the entropy bonus; later work added more because "the networks put 90% or more on one option at 80% of Act 1's rewards" [TR].
- Added after ironclad-v1: replaying lost practice fights, runs started from human boss states, a fight/deck split [TR].

**What the author says mattered.** "Most of the A20 gain came from the habit rewards (deck size, removing Strikes and curses, upgrading rather than resting at high HP) and the win reward" [IC].

## 4. What mattered and what failed

Helped:
- win reward 3 (A20 "from about 11% to 17%" [TR]) and habit rewards (the release reaches 21.9% [IC]);
- displayed card numbers and per-target damage in the inputs [NI];
- practice on human boss decks and on archetype decks [IC, PR];
- the look-ahead: Ironclad A20 21.9 → 27.5%, boss fights 69.0 → 76.1% [IC]; Watcher A20 15.1 → 22.6% won, human Heart fights 41 → 61% [WA].

Failed:
- a depth-first search scoring a move by its best line: "it lost to the network alone" [SRS];
- search as a training target: a soft target "raised the policy's entropy and lost"; the best move as cross-entropy "flattened the policy at once"; lifting clear wins "did too" [TR];
- imitation without holding the rest of the policy: "won 22% of its runs instead of 80%" [PH]; forcing human winners' picks on the Defect "made its runs shorter, as its fights don't use those cards yet" [DE];
- a fight network trained on practice fights alone "halved ironclad-v1's wins in whole runs" [TR];
- card removal by rule at every shop: 81% → 46%, "it removed its best cards" [RP];
- later checkpoints were worse (updates 250 and 300 below 200) [IC].

Known weaknesses: at A20 "most of its losses there are boss fights" [IC]; Silent: "its decks at the Act 1 boss are the main limit" [SI]; Defect: "its decks are the limit" [DE]. Inputs omit Double Tap-style repeats, changed costs in piles and Frozen Eye's draw order [NI].

## 5. The simulator

- **Design.** Pure Rust, no I/O, `Run` is `Clone`; it stops only at decisions and offers a legal-action list; the game's 13 RNG streams are ported bit-exactly and "golden-tested against the real jar" [ARCH].
- **Checked both ways** [ARCH]. Game → engine: a patched CommunicationMod records real-game fights and seeded runs under a random policy; replay compares "piles in order, HP, block, energy, powers, monster moves, gold, deck, relics, pools and RNG counters" at every decision. Engine → game: network-played runs are played back in the real game overnight ("soak"), checking floor and HP before every decision.
- **Coverage.** Every ported row has both the unit-test and real-game boxes ticked: 372 cards, 179 relics, 65 monsters, 51 events, 42 potions, 133 powers [CHK; my count of the tables].
- **Known deviations.** Frame-timing effects, the game's unseeded calls, Secret Portal's playtime check, patches applied to the real game for recording; no unlock progression [ARCH, README].
- **Speed.** "Random play runs at about a million moves a second" through the batched Python module [README]. Training is network-bound: about 9k steps/s for the Ironclad's last run, and 8.7 s per 262,144-step update (about 30k steps/s) after CUDA-graph work [IC, FX].
- **Released.** Code: engine, encoders, training, search, analysis. Weights are "not in git" and the GitHub releases page is empty [GH], but the viewer serves them: `https://spire-viewer.pages.dev/assets/ironclad-v1.fb21879b8d.safetensors` answered HTTP 200 (4,466,672 bytes per NET). Not published: training logs, checkpoints, human datasets.

## 6. How to use it for this project

**(a) Replicating the recipe on our simulator.** Needed: a batched environment with fixed-size rows, a legal mask and the 171 slots; every non-combat screen as an option list; "as shown" card numbers and per-target damage; a reset-to-scenario call (deck, relics, potions, HP, encounter); habit counters; the human pipeline (`python/human/`, MIT, but tied to their engine's encoder). Shortest path [inferred]: expose their `sts.Envs` Python interface from our simulator and reuse `train_runs.py`.

Compute, all [inferred] from the figures above:
- One clean pass with the recipe known is roughly 0.5-1.3 billion steps, i.e. 5-40 GPU-hours on a 16 GB consumer card at 9-30k steps/s.
- Their own path took six calendar days. Budget 100-200 GPU-hours for a first working replication, with a 4-8 core host beside the GPU (about 500-1,500 CPU-hours).
- CPU-only is impractical. At about 600 positions/s per thread for inference [EW] and about 12 forward-equivalents per sample for four PPO epochs, a 4-vCPU learner manages about 200 steps/s: a month for 500M steps. Our ten machines could supply rollouts; the learner needs a GPU.

**(b) Evaluating their agent against ours.**
- *On their engine:* the weights are downloadable, but ironclad-v1 uses encoding 1 while public HEAD is encoding 3. The loader "refuses" a mismatch [ER], and such a network "runs only with the code it was released from" — a commit absent from the public history [SYN]. Options [inferred]: drive the release's own WebAssembly engine listed in NET; or convert through `upgrade_state` [RM] and first reproduce 21.9% on seeds 300,000-301,023.
- *Our agent on their engine:* `sts-cli serve` gives JSON state and legal actions [README]; our search would need porting onto their cloneable `Run`.
- *Their network on our engine:* needs their encoding reproduced exactly; any mismatch silently weakens it. Not recommended.
- *Cheapest fair comparison:* same win definition, each agent on its own engine, 1,024+ seeds, plus human-winner boss fights rebuilt with [HB].

**(c) Worth copying, ranked.**
1. One run-level value trained through fights and choices together, so fight play prices HP and potions for the rest of the run.
2. Scenario practice: human winners' boss states and archetype decks, 30% of games.
3. Win reward 3, γ 0.9995, and habit rewards taken from human winners' statistics.
4. Entity tokens with logits read from the token acted on, plus displayed numbers and kill features.
5. The policy-guided look-ahead with shared redraws, mean scoring, and a potion price.

Not to copy: search-as-teacher losses; best-line (max) search; a fight network trained apart from runs; imitation without a keep loss; picking checkpoints on the evaluation seeds.

**(d) Where our missing strength probably is.** Stated carefully:
- A no-search 1M-parameter policy beats our search agent at every level (93 / 75 / 22 against 85 / 52 / 8), on a different simulator.
- Their A20 rate roughly doubled (about 11% → 21.9%) through the win reward and out-of-combat habit rewards [TR, IC]; search in fights added about 6 points on top.
- Their fight-local experiments lost: a practice-only fight network halved wins, and unpriced search wasted potions.
- So our gap is more likely in deck building, pathing and resource use, and in fights piloted for the fight rather than the run, than in tactical depth [inferred]: no ablation pairs a strong search with their deck policy.
- A direct test: run our search on human-winner A20 boss fights. They report 69.0% greedy and 76.1% with look-ahead [IC]. If we match that, our deficit is strategic.

## 7. Doubts

- Simulator results, self-reported, not independently replicated; the fidelity evidence is strong but the recordings are private.
- The 1,024 evaluation seeds also chose the checkpoint ("update 200 was its best checkpoint at A20") and steered about 35 runs; expect a point or two of optimism [inferred].
- Not pure self-play: the recipe uses human winners' choices and decks, and hand-written habit rewards.
- A win is the Act 3 boss, not the Heart; the Ironclad never takes keys.
- 27.5% was measured once on older code, with unstated settings and run count.
- The headline cannot be reproduced from public HEAD without the encoding work in 6(b). Training logs and `args.json` are unpublished: habit weights, several hyper-parameters and the Ironclad's total steps are unknown.
- The look-ahead clones the true run and reseeds the fight's streams; I did not audit whether every stream that affects its leaf values is redrawn [unverified].
