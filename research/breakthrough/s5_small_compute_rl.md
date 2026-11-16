# S5 — Deep RL and expert iteration on small compute: results, throughput, costs, plan

Web search and page fetch were available. Every external number carries the URL I opened (arXiv PDFs were converted to text and the passages read). Labels: **[measured]** = micro-benchmark I ran for this report on a project-class machine (4 vCPU Intel Xeon 2.1 GHz with AVX-512; g++ -O3 -march=native; ONNX Runtime 1.29; PyTorch 2.15 CPU; throwaway script, not kept); **[project]** = `research/program.md` entry, by date; **[inferred]** = my arithmetic; **[from memory, unverified]** = not opened.

**Bottom line.** (1) Data is not this project's constraint: the existing CPUs can produce 1–2.5M search-labelled runs per day, the same order as the 4.2M games of KataGo's whole 19-day run. (2) The constraint is play-time inference: a few milliseconds per decision on one core buys about 1,000 evaluations of a net up to ~100k parameters, 150–400 batched evaluations of a 1M-parameter MLP, or 3–5 evaluations of a small transformer. (3) Published small-compute successes therefore point to two shapes: a tiny, incrementally updated net at every tree node, or a larger net with a few-simulation search; a GPU is needed only to train a larger teacher, and one GPU-week costs about $60–350.

## 1. What was achieved on what hardware

| System | Hardware | Wall-clock / data | Result | Source |
|---|---|---|---|---|
| AlphaZero | 5,000 first-gen TPUs (self-play) + 64 second-gen TPUs (training) | 9 h chess / 12 h shogi / 34 h Go; 44M / 24M / 21M games at 800 simulations | Passed Stockfish after 4 h | https://arxiv.org/abs/1712.01815 |
| ELF OpenGo | 2,000 V100 | 13–14 days ≈ 74 GPU-years | Top level | as quoted in https://arxiv.org/abs/1902.10565 |
| KataGo (g104) | ≤28 V100 | 19 days ≈ 1.4 GPU-years; 4.2M games, 241M samples | Surpassed ELF: "factor of 50 reduction" | https://arxiv.org/abs/1902.10565 |
| KataGo (g170) | 28 → 46 V100 | 157 days; passed g104 after 12–14 days | +700 Elo over g104 | https://github.com/lightvector/KataGo/blob/master/TrainingHistory.md |
| Gumbel AlphaZero in Pgx, 9x9 Go | 1 A100 | 8.6 h; ≈105M frames, 32 simulations, 6-block ResNet, 1,024 parallel games | Passes the authors' own baseline checkpoint; no external strength claim | https://arxiv.org/abs/2303.17503 |
| MiniZero, 9x9 Go / Othello | 2 Xeon E5-2678 v3 + 4 GTX 1080 Ti | ≈30 h; per 200 training steps: 378.5 s (AlphaZero, n=200), 40.6 s (Gumbel, n=16), 23.3 s (Gumbel, n=2) | At equal time n=2 ≈ n=200; n=16 slightly worse | https://arxiv.org/abs/2310.11305 |
| AlphaZero.jl, Connect Four | i5-9600K + RTX 2070 | 15 iterations × 1–2 h; 5,000 games per iteration, 600 simulations, 1.6M parameters | Clearly beats depth-5 minmax; still imperfect in openings | https://jonathan-laurent.github.io/AlphaZero.jl/stable/tutorial/connect_four/ |
| EfficientZero | 4 GPUs | 7 h per Atari-100k game, 50 simulations; quotes MuZero at 64 TPUs × 12 h | 194% mean, 109% median human | https://arxiv.org/abs/2111.00210 |
| EfficientZero V2 | 8× RTX 3090 server | 2.7 h per 100k training steps (Walker Run, 1.3M parameters); 16–32 simulations | Atari-100k normalised mean 2.43 | https://arxiv.org/abs/2403.00564 |
| DouZero (Deep Monte-Carlo, no search) | 48 cores + 4 GTX 1080 Ti, 45 actors | Beat supervised baseline in 2 days, DeltaDou in 10; 30 days total | First of 344 on Botzone | https://arxiv.org/abs/2106.06135 |
| PerfectDou | 880 CPU cores + 8 GPUs | 2.5×10⁹ steps | Beats DouZero (~10¹⁰ steps); at 10⁹ steps wins 73.2% against DouZero at 10⁹ | https://arxiv.org/abs/2203.16406 |
| Suphx (Mahjong) | 44 GPUs (4 Titan XP + 40 K80) | 2 days, 1.5M games per RL agent | Each RL stage beat the supervised agent | https://arxiv.org/abs/2003.13590 |
| 2048, n-tuple TD (CPU only) | 24 CPU cores, no GPU | 4×10¹⁰ actions, ≈5.5 days | 609,104 average at 1 s/move | https://arxiv.org/abs/1604.05085 |
| Othello, TD n-tuple + MCTS only at test time | "standard hardware (no GPU or TPU)" | not stated in abstract | Beats Edax up to level 7 | https://arxiv.org/abs/2204.13307 |
| Expert Iteration, Hex | not read | 100k–243k positions per iteration, teacher = 10,000-simulation MCTS | 75.3% against MoHex 1.0 | https://arxiv.org/abs/1705.08439 |
| Searchless chess (pure distillation) | not stated | 10M games → 15.3B action-value labels = 8,864 Stockfish-days at 50 ms; 10M steps of batch 4,096 | 9M parameters: 2025 tournament Elo; 270M: 2299 (teacher 2711) | https://arxiv.org/abs/2402.04494 |

Two readings. First, KataGo's gain is a stack of 1.25–1.65× factors (product ≈ 9× [inferred]), not one trick: playout-cap randomisation 1.37×, forced playouts with policy-target pruning 1.25×, global pooling 1.60×, auxiliary policy targets 1.30×, auxiliary ownership/score targets 1.65×, game-specific features 1.55× (Table 2, same paper). Second, few-simulation Gumbel search is real but not free: the original paper reports MuZero "fails to learn from 16 or less simulations" on 9x9 Go while Gumbel MuZero "learns reliably even with 2", with training-step speed-ups of 5.9× (n=32) to 24.3× (n=4) (https://object.cloud.sdsc.edu/v1/AUTH_da4962d3368042ac8337e2dfdd3e7bf3/ml-papers-txt/ICLR/2022/policy_improvement_by_planning_with_gumbel__15d5fc26.txt), yet MiniZero's equal-time test shows only parity.

## 2. Throughput facts

**Environment steps per second.**

| System | Hardware | Throughput | Source |
|---|---|---|---|
| IMPALA, one machine | 48 CPUs + 1 P100 | 21–24K frames/s | https://arxiv.org/abs/1802.01561 |
| IMPALA, distributed | 500 CPUs + 1 P100 | 250K frames/s | same |
| Sample Factory (asynchronous PPO) | 36 cores + 1 RTX 2080 Ti | 135,893 frames/s on Atari (pure simulation: 181,740) | https://arxiv.org/abs/2006.11751 |
| EnvPool, Atari, simulation only | 12-core laptop / 32 cores / 256 cores | 49K / 200K / 1.07M frames/s | https://ar5iv.labs.arxiv.org/html/2206.10558 |
| PufferLib 3.0 / 4.0 (PPO, C environments) | 1 RTX 5090 | up to 4M / 15M steps/s | https://puffer.ai/blog/3.0/ , https://puffer.ai/blog/4.0/ |
| sts_lightspeed, random playouts | 16 threads | 1M playouts in 5 s | https://github.com/gamerpuppy/sts_lightspeed |
| This project's full search agent | one 4-vCPU box | 2,000 A0 runs in 693 s [project 2026-11-10 01:28] ≈ 1.4 core-seconds per run ≈ 250k runs/day per box, ≈ 2.5M/day on ten [inferred, assuming 4 threads] | `research/program.md` |

The simulator side is already in the EnvPool/PufferLib class. In all the systems above the single GPU serves the learner and batched policy inference; it does not make the environment faster.

**Inference cost per evaluation, one CPU thread.**

| Network | Cost | Source |
|---|---|---|
| MLP 204-42-42-1, 10.5k parameters (shape of the project's `sv_55`) | 1.4 µs hand-written C++; 6.6 µs through ONNX Runtime at batch 1; 0.3 µs in batches of 64 | [measured] |
| MLP 204-256-256-1, 119k | 12.5–14.7 µs at batch 1; 1.9 µs batched | [measured] |
| MLP 512-1024-512-1, 1.05M | 196–222 µs at batch 1; 19 µs batched | [measured] |
| MLP 1024-2048-1024-1, 4.2M | 0.8 ms at batch 1; 77 µs batched | [measured] |
| NNUE-style: 256-wide int16 accumulator updated incrementally + int8 32-32-1 tail | 1.5 µs (4.6 µs at width 1,024), unoptimised code | [measured] |
| Transformer encoder, d=64, 2 layers, 32 tokens, 100k | 256 µs; 122 µs batched | [measured] |
| Transformer, d=128, 4 layers, 64 tokens, 0.6M | 1.4 ms; 1.1 ms batched | [measured] |
| Transformer, d=256, 6 layers, 64 tokens, 3.3M | 5.1 ms | [measured] |
| Stockfish NNUE | design target "millions of evaluations per second per thread"; disabling incremental updates costs 46% speed (−67.6 Elo) | https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md , https://github.com/official-stockfish/Stockfish/wiki/Useful-data |
| KataGo 15–20-block ResNet on CPU | 10–20 playouts/s, i.e. 50–100 ms each | https://github.com/lightvector/KataGo/blob/master/README.md |
| 2048 n-tuple lookup tables | 258,371 moves/s at 1-ply, single thread (≈1 µs per state [inferred]) | https://arxiv.org/abs/1604.05085 |

What the measurements mean here:

- A run takes ≈1.4 core-seconds and has a few hundred combat decisions, so the play-time budget is roughly 3–8 ms per decision [inferred]. One 10k-parameter leaf evaluation already costs about half a rollout [project 2026-11-11 09:19]. Any net above ~30k parameters is therefore dearer than a rollout, and a 1M-parameter MLP costs dozens of rollouts even when batched.
- At batch 1 a net larger than the L2 cache is memory-bound (1M float32 parameters = 4 MB read per evaluation). Batching leaves gave 11×; int8 weights cut the bytes 4× [inferred].
- ONNX Runtime adds about 5 µs per call: irrelevant above 100k parameters, dominant below. Tiny nets should stay hand-written.

**Training throughput on one 4-vCPU box** [measured, PyTorch, 4 threads]: 119k MLP 235k rows/s; 1.05M MLP 36.6k rows/s (3.2B rows/day); 4.2M MLP 13.3k rows/s; transformer 100k/32 tokens 6.6k rows/s; 0.6M/64 tokens 657 rows/s (57M rows/day); 3.3M 160 rows/s. MLPs up to a few million parameters train on billions of rows without a GPU; transformers do not. The GPU advantage on transformers is about two orders of magnitude [from memory, unverified].

## 3. Costs

On-demand prices per GPU-hour, read 2026-11-16:

| GPU | Marketplace low / median (https://getdeploying.com/gpus) | RunPod community / secure (https://www.runpod.io/pricing) | Lambda (https://lambda.com/pricing) |
|---|---|---|---|
| T4 | $0.20 / $0.69 | — | — |
| L4 | $0.31 / $0.79 | $0.44 / $0.69 | — |
| RTX 3090 | $0.10 / $0.24 | $0.22 / $0.50 | — |
| RTX 4090 | $0.33 / $0.50 | $0.34 / $0.89 | — |
| RTX 5090 | $0.35 / $0.72 | $0.69 / $1.39 | — |
| A10 | $1.00 / $1.63 | — | $1.29 |
| A100 | $0.42 / $1.99 (80 GB) | $1.19 / $2.09 (80 GB) | $1.99 (40 GB) |
| H100 | $1.80 / $3.95 | $1.99 / $2.89 (PCIe) | $3.79 PCIe, $4.89 SXM |

AWS reference: T4 box (g4dn.xlarge, 4 vCPU, 16 GB) $0.526 on demand, $0.224 spot (https://instances.vantage.sh/aws/ec2/g4dn.xlarge); a 4-vCPU CPU box (c7i.xlarge) $0.1785, $0.057 spot (https://instances.vantage.sh/aws/ec2/c7i.xlarge). So an RTX 4090 costs about two to five CPU boxes.

Example budgets [inferred from the rows above]:

| Run | GPU-hours | Cost today |
|---|---|---|
| Pgx Gumbel AlphaZero, 9x9 Go | 8.6 A100-h | $10–18 |
| EfficientZero, one Atari-100k game | 28 | $10–25 on 4090-class |
| One GPU for one week | 168 | 4090: $57–150; A100: $200–350; H100: $330–820 |
| DouZero, to beat DeltaDou / full run | 960 / 2,880 (plus 48 cores) | $100–700 on 3090-class |
| KataGo g104 | ≈12,300 V100-h | $4k–6k on 4090-class; $12k–20k on A10-class |
| 8 GPUs for 4 weeks | 5,376 | 4090: $1.8k–4.8k; A100: $6k–11k; H100: $11k–26k |

Free tiers: Colab states resources "are not guaranteed and not unlimited", free sessions run at most 12 hours (https://research.google.com/colaboratory/faq.html). Kaggle gives about 30 GPU-hours a week [from memory, unverified]. Usable for a one-off training job, not for a loop.

## 4. Recipes for teacher–student training from search

1. **Policy target = the whole search distribution, not the chosen move.** In Expert Iteration, tree-policy targets (root visit shares) gave +50 ± 13 Elo over chosen-action targets at equal prediction error (https://arxiv.org/abs/1705.08439). With few simulations, use Gumbel's completed-Q target, which also covers unvisited actions (Gumbel source above). KataGo prunes forced-exploration visits from the target.
2. **Value target = outcome of the strong player, on decorrelated states.** ExIt: a value net "without severe overfitting requires more than 10⁵ independent samples", one position per game. The project's outcome-target test used 9.1k battles and failed because all rows of a battle shared one label [project, Phase B gen0 entry]: an order of magnitude under that bar, and three under what the CPUs now generate in a day. The later 32M-row net regressed the mean of 8 random rollouts, so at best it reproduces the rollout it replaces [project 2026-11-11 08:59].
3. **DAgger: label the states the student visits.** Naive imitation breaks because "future observations depend on previous predictions" (https://arxiv.org/abs/1011.0686). ExIt added three batches of 100k student-visited, teacher-labelled positions: +120 Elo; the student then matched its 10,000-simulation teacher (87/162).
4. **Iteration schedule.** ExIt: batch (3 × 243k moves) or online (buffer of the latest 243k, or grow 10% per iteration). KataGo: window growing from 250k to 22M samples, a gate of 100 wins in 200 games, net grown from 6×96 to 20×256 blocks at days 0.75 / 1.75 / 7.5.
5. **Playout-cap randomisation.** KataGo runs a full search on 25% of moves (recorded for the policy target) and a cheap one elsewhere, so more games finish for value training (1.37×).
6. **Auxiliary targets.** KataGo's ownership/score heads were its largest factor (1.65×). They give per-state signal where the outcome label is shared.
7. **Reanalyse.** Re-search stored states with the newest net or a larger budget; EfficientZero relabels 99% of policy targets (https://arxiv.org/abs/2111.00210). Offline compute is unlimited by the program's rules, so a 10–100× search is a legal teacher.
8. **Privileged teacher, legal student.** PerfectDou's critic sees all hands in training while the actor sees only legal information; it beat DouZero with about a tenth of the samples. Suphx's "oracle guiding" does the same.
9. **Distil a strong slow engine into a tiny CPU net.** A standard Stockfish NNUE training set holds about 16B positions searched to depth 9, and data derived from the big-network engine Lc0 is "generally of higher quality" (https://github.com/official-stockfish/nnue-pytorch/wiki/Training-datasets).

**Teacher data used elsewhere:** ExIt 10⁵–10⁶ positions; AlphaZero.jl 75k games [inferred: 15 × 5,000]; Suphx 1.5M games; KataGo 241M samples; DouZero and PerfectDou 10⁹–10¹⁰ steps; NNUE 1.6×10¹⁰ positions; searchless chess 1.5×10¹⁰ labels, with accuracy still rising from 10k to 1M games; 2048 4×10¹⁰ actions. The project's largest set is 3.2×10⁷ rows. Its holdout error was flat from 22.5M to 32.4M rows (11.93 → 11.83) and the 29k-parameter net played no better than the 10k one [project 2026-11-11 12:45]: features, capacity and target were binding, not rows.

## 5. A realistic compute plan in three tiers

**Tier A — CPU only (today).**
Feasible: a daily expert-iteration generation. Ten boxes produce ≈1–2.5M full runs/day, a few hundred million root decisions with visit counts and Q-values [inferred]; one box trains a 1M-parameter MLP on 3.2B rows/day [measured]. That is KataGo-scale data per day. Candidate students: (a) an NNUE-style net (sparse card, pile, enemy and power features, wide incrementally updated first layer, int8), 1.5–5 µs, usable at every node; (b) a 0.1–1M-parameter policy/value MLP evaluated in batches at the root and shallow nodes with Gumbel selection; (c) table-lookup TD features in the 2048 style. No GPU is needed for any of these.
Not feasible: transformers above ~100k parameters (57M rows/day), ResNet-scale AlphaZero.
Main risk: the representation. The project's value error did not move with rows, and the larger net lost its gain to its cost. A bigger MLP on the same 204 features may saturate the same way; the bet is on new inputs and new targets, and the owner closed the leaf-value line, so this needs an explicit decision.

**Tier B — one rented GPU for a week ($60–350).**
What it buys: a teacher the CPUs cannot train, namely a set or transformer encoder of 1–10M parameters over cards, relics and enemies, trained on 1–10B CPU-generated rows, then distilled into Tier A students; plus one Pgx-scale experiment (8.6 A100-hours bought a full Gumbel AlphaZero run on 9x9 Go). Self-play stays on CPUs. The same GPU would train larger out-of-combat models, where decisions are rare and a 1–5 ms evaluation is affordable.
Main risk: the teacher cannot run at play time (1.4–5 ms per evaluation on CPU), so everything depends on distillation; searchless chess lost about 400–700 Elo to its teacher even with 9–270M parameters. Second risk: engineering time for data shipping exceeds the GPU week.

**Tier C — several GPUs for weeks ($2k–25k).**
What it buys: a KataGo-style closed loop with a GPU-sized net inside the training search (batched inference serving CPU self-play workers), playout-cap randomisation, auxiliary heads, a growing net and periodic distillation into the CPU agent; or DouZero-scale model-free training (10⁹–10¹⁰ steps) for the run-level policy. KataGo needed 1.4 GPU-years to pass ELF; DouZero needed 4 older GPUs for 10–30 days.
Main risk: strength learned with a big net in the loop may not survive the fixed CPU budget at play time. No published system in this list was built under that rule, so the transfer is unproven. Second: Ascension 20 Heart may be limited by deck-building credit assignment, which a combat loop does not address.

## 6. Top transfers and dead ends

**Top transfers, ranked.**

1. **Search-improved targets in a daily expert-iteration loop** (recipes 1–4): policy from visit distributions or completed Q, value from the search agent's battle outcome on about one state per battle over 10⁶–10⁷ battles. Confidence that it is feasible on current CPUs: high. Confidence that it beats rollouts at equal wall-clock: medium-low; the root prior already captures part of it.
2. **NNUE-style incrementally updated net** trained on those targets and on reanalysed deep-search labels. It is the only published design that keeps ~1,000 evaluations per few milliseconds. Confidence: medium; feature design is the work.
3. **Gumbel root selection with completed-Q targets**, to make a 0.1–1M-parameter net pay off at 16–64 simulations. Confidence: medium; MiniZero shows parity, not a win, at equal time.
4. **KataGo's efficiency set**: auxiliary targets (damage taken, turns to kill, end-of-fight HP), playout-cap randomisation for data generation, larger nets only as data grows. Confidence: medium-high as a multiplier on 1–3, none alone.
5. **Privileged teacher** (critic or auxiliary head sees draw order in training; the deployed net does not). Confidence: medium; the published gain was 10× in samples, which are already cheap here, so expect a smaller benefit.

**Dead ends.**

- More rows for the same small MLP on the same features with rollout-mean targets (project result; consistent with recipe 2).
- MuZero or EfficientZero learned models: the simulator is exact and fast, and MiniZero's authors prefer Gumbel AlphaZero when the real environment is available. Atari-100k sample-efficiency methods optimise the wrong resource here.
- Transformers or ResNets inside the play-time tree: 0.26–5 ms per evaluation [measured]; KataGo's CPU backend manages 10–20 evaluations per second.
- Model-free PPO or Deep Monte-Carlo as the combat player on CPUs: the published wins needed 10⁹–10¹⁰ steps on 4–8 GPUs and replaced weak search, not a tuned one.
- Free GPU tiers as the training platform for a loop.
