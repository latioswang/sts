# Plan D — Evidence first: measure where the strength is missing, then build

Written 2026-11-16 from the stage-1 material; nothing was built or run. Sources are cited by file prefix (`00` = 00_internal_map.md; d1, r1, c1, c2, h1–h7, w1, w2, s2–s6). FA / FS = research/cpp/FairAgent.cpp / FairSearch.cpp. "1×" = the current simulation caps. Win rates are Ascension 20 (A20), Act-3 win, unless stated. [estimate] = my number; [inferred] = my reading.

## 1. Thesis

The missing strength has not been located: the record supports "the decks lose", "the pilot loses" and "the search cannot see past the turn" about equally, because none of the five measurements that would separate them was ever made (00 §3 rows j, k, l, u; d1 §6.6). A ten-day diagnostic programme of about 350 core-hours, built mostly on switches that already exist, will state in win-rate points how much of the gap belongs to decks, piloting, hidden information and luck, and a pre-registered table turns those numbers into one of three builds. Now is the time because every line is closed and the next step is a build of about four months, while the measurements cost ten days and about $16 of compute.

Evidence:

1. **The two internal diagnoses contradict each other and both are weak.** "All tracks lose mainly in combat search quality" (F39) against "deck-strength gap" (F42b, F50), the latter from 180 and 240 hand-read traces, correlational; entry HP also reflects combat quality (00 §7.2; d1 §5).
2. **No ceiling exists.** The only clairvoyant run used lightspeed's own search and scored 9.6% with 16 crashes (r1 §4). Outside, hidden information was worth 22.8 points to Spire Mentor (s4 §1), and an Ironclad agent with exact-state search reports 50.1% at A20 Heart (s6 §1a). Whether this project's gap to that is 1 point or 20 is unknown (r1 §6.5).
3. **The search curve stops at 4×**, on older bests and 1,000 seeds: A20 6.0 / 8.5 / 9.3% flattens, A10 48 / 53 / 58% does not; the Heart fight stays at 47–49% lost (d1 §3).
4. **"Out-of-combat is saturated" was measured one decision at a time** under the agent's own continuation with 8 noisy playouts (halves agree in 35–60% of decisions); the one decision type that got a learned correction gained +0.030 / +0.050 per run from +0.011–0.013 per decision (h2 §1, §2; 00 §7.3).
5. **Outside evidence points both ways.** spire-engine reaches 21.9% with no search (s6 §1a); in the zero-search paper the best cell was a learned strategy with Mentor's search, and 58–72% of final-boss losses could not be won on replay (w1 §1.2). Yet the method survey rates determinisation as "no step change left" and, one item later, names the dead tree below the turn boundary as the known failure case (w2 §A1, §A2).
6. **The agent's profile looks like the opposite of the experts', and it got there by experiment.** Experts fight 2.6 Act-1 elites and rest at 9% of campfires (w1 §2.2). The A20 best still carries an 80%-HP elite gate and an Act-1 elite penalty (`elite_hp=0.8`, `w_elite_act1=-2`, best_a20_params.txt), kept because they gained floors (h5 §1), and its losing runs heal at 56% of rest-site visits [computed from d1 §7]. Either its fights cost too much HP or its decks cannot cash relics; par HP per fight is unknown (d1 §6.8).
7. **Outcomes are about half luck per seed.** Of 153 A20 seeds won at 1×, 79 are won at 2× (d1 §3); no replicate runs exist (d1 §6.6). Every training signal and every gate depends on a variance split nobody has.
8. **Cost asymmetry.** Closing the learned-leaf line took 97,500 games and 21 ledger rows (h1 §2; r1 §3). The whole programme costs less than that one phase.

## 2. Target design

The design is a function of the measurements. Three parts are fixed; the table chooses the rest.

**2.1 Fixed parts, needed by every branch.**
- A diagnostic binary, `research/diag/diag_main.cpp`. The harness is frozen, ties the agent seed to the run seed (eval_main.cpp:51) and kills games after 900 s (line 194), so nothing is added there.
- A **fight bench**: saved fight-entry states (deck, relics, potions, HP, encounter, ascension) replayed many times with fresh hidden state. It is the first gate for any combat change and the restart source for any training.
- A per-run log (path, rest-site choices, card picks and skips, purchases, removals, damage per fight), so agent and human runs go through one script.

**2.2 Decision table.** Quantities (defined in §4): `C16` = clairvoyant search at 16× with today's strategy layer, a practical ceiling for combat-only work [inferred; not a strict bound]; `S64` = fair search at 64×; `Ref` = 1× agent's fight win rate on expert reference decks (Act-2 boss entered at 80% HP / the Heart), `Own` = the same on its own median decks; `Doomed` = share of fatal fights that win under 10% of replays at 16×; `G_deck`, `G_hp` = win-rate gain from gifted relics, removals and upgrades / from gifted HP. Thresholds are my proposals for pre-registration [estimate].

| Pattern | Reading | Direction |
|---|---|---|
| `C16` < 20% and `S64` < 15%; `Ref` ≥ 85% / 75%; `Doomed` ≥ 60%; `G_deck` ≥ 2 × `G_hp` | decks lose, the pilot is adequate | **Strategy-first**: one run-level policy trained by reinforcement learning with the honest search in the loop |
| `C16` ≥ 30% or `S64` ≥ 20%; `Ref` < 70% / 55%; `Doomed` ≤ 35%; `G_hp` ≥ 2 × `G_deck` | the pilot loses winnable fights | **Combat-engine-first**: tree across the turn boundary, outcome-trained boundary value or playout, run-aware terminal |
| `C16` ≥ 25% and `Ref` − `Own` ≥ 20 points | both, and they multiply | **Unified, staged**: strategy policy on today's engine, engine rebuilt behind the fight bench, policy retrained on the new engine |
| `C16` < 15%, `Ref` − `Own` < 10, both gifts < +3 | the gap is elsewhere | one-week coverage and objective audit (h6 §6.1), then back to the owner |

Modifiers. If K-world search (D1) gains 2 points or more at equal search actions, it is adopted in any branch, since it is a legal agent. If the share of seeds won at least once in 8 replicates is at least 3 × the mean win rate, wins lie inside the agent's own decision noise and reinforcement learning has a nearby signal; under 1.7 ×, wins are seed-bound, and restarts plus a privileged critic become mandatory.

**2.3 Most likely branch.** My prior: unified-staged 45%, strategy-first 30%, combat-first 20%, none 5% [estimate]. The first two share their first build, so 75% of the mass starts as below.

- **State encoding (strategy).** Embedding tables for card (id × upgraded), relic and potion identity, sum- and mean-pooled; about 30 scalars (HP, gold, floor, act, ascension, keys); act boss; per legal map node the room types of the next six rows (extending the path features at FA:1448-1471). Each legal option is its own vector: type, entity embedding, price.
- **Model.** Trunk 2 × 256 on the pooled state, an option scorer with a masked softmax, value heads for run return, P(Act-3 win), P(Heart kill) and HP fraction after the next fight. About 0.4M parameters. It replaces V(after) − V(now), the KB values, the map DP and path model, the event table, the Neow rule and the shop order.
- **Training target and loop.** PPO; steps are non-combat decisions; every fight is played by the real FairSearch at 250 simulations (1,000 at bosses), Mentor's setting (w1 §1.1); return = win plus 0.02 per floor, γ = 1 (s4 §5); ascension sampled uniformly; start by cloning the current bests; restarts from saved act starts and boss entries, weighted by where runs die (w1 §1.2).
- **Hidden information, training only.** (i) A privileged critic that sees the true upcoming card rewards, relic-pool order and next-act boss, while the actor sees legal inputs only (the largest single ablation in s2 §2: 22% → 82%). (ii) Clairvoyant and 16× search as the fight-outcome teacher for the HP-after-fight head. (iii) Returns averaged over replicate fights from saved entry states. An out-of-combat leak test (missing today, c2 §5) enforces the boundary.
- **Search at play time.** Unchanged FairSearch at 1× first. Then the terminal value becomes the critic's value of the post-fight state (never tried, 00 §3 row a), read from a table over end-HP bucket × potions kept, filled once per fight.
- **Combat engine, only if the table says so.** K-world or bucketed draw nodes so statistics survive the turn boundary (c1 §6.1), plus a sparse entity-level boundary value trained on realised outcomes of 16× and clairvoyant play from the fight bench. This needs the owner to reopen the evaluator line (§6).
- **Play-time cost.** About 100 strategic decisions per run (c2 §6.4), each one trunk pass of ≈ 80 µs [inferred from s5 §2: 119k-parameter MLP 12.5–14.7 µs, 1.05M 196–222 µs] plus ≤ 25 option heads: ≈ 15 ms per run against 780–1,830 ms of search (c2 §5), 1–2%, paid by lowering `sims` under the existing equal-actions calibration. Terminal table: ≤ 64 batched passes per fight, ≈ 1 ms [estimate]. Engine: a decision is ≈ 1,000 simulations of ≈ 6 µs, 87% of it rollout (c1 §4); a boundary value of 1.5–5 µs (s5 §2) keeps 700–1,000 simulations, whereas a learned playout at 1.5 µs per action leaves about 120, so only a table-lookup playout is admissible (prior-guided rollouts lost at 2.43× cost, 00 §2).
- **Reused:** simulator, FairSearch, harness, seed discipline, loggers, real-game bridge, the V and path models as warm-start critics.

## 3. Roadmap

Compute prices use s5 §3: a 4-vCPU box at $0.1785 per hour on demand. Training runs cost 0.5–1.0 core-seconds at reduced search [estimate from 0.78–1.83 s at 1×, c2 §5].

| Phase | Deliverable | Effort | Compute | Gate (fresh paired seeds) | Kill |
|---|---|---|---|---|---|
| **P0** weeks 1–2 | Diagnostics D1–D6, filled decision table, branch memo | 10 days | 350 core-h ≈ $16 | Validity only: bests bit-identical with switches off; fair arm reproduces 7.6% ± 1.2 on 2,000 seeds; clairvoyant ≥ fair on every track; leak test passes | No pattern matches after the one-week audit: no build, back to the owner |
| **P1** weeks 3–7 | Strategy policy v1 on today's engine, all non-combat decisions | 18 days | 3 campaigns × 5M runs ≈ 2,100–4,200 core-h ≈ $95–190 | Pre-step: card-reward-only PPO at A10, 123k runs, +3 points on 2,000 seeds (s4 §6). Main: A20 ≥ 15%, A10 ≥ 62%, A0 ≥ 84% at 1×, 2,000 seeds, P ≥ 0.99, confirmed on 2,000 more | A20 < 11% after 15M runs: strategy on this engine is exhausted; go to P3 |
| **P2** weeks 7–9 | Fight terminal = critic value; identity-aware potions | 6 days | 600 core-h ≈ $27 | +2 points A20, no loss at A10, 4,000 seeds | Two negative reads: keep the hand terminal |
| **P3** weeks 8–14 | Combat engine as scoped by D1–D3 | 20 days | 1,500 core-h ≈ $67 | Fight bench (2,000 entry states × 8 fights): +5 points fight wins at equal search actions; then +3 points A20 on 4,000 seeds; then one policy retrain | Bench gain < +2 points after two designs: close again |
| **P4** weeks 13–18 | A20 Heart objective: keys and Heart reward in the policy, act-4 restarts, Heart bench | 12 days | 2,500 core-h ≈ $112 | A20 Heart ≥ 8% on 2,000 seeds, then ≥ 20% | < 3% after two campaigns |
| **P5** at each milestone | Claim package | 8 days | < 50 core-h | below | — |

Total: about 18 weeks, 75 engineering days, 7,000–9,000 core-hours ≈ $320–410 [estimate], no GPU. If strategy-first alone is selected, P3 is dropped; if combat-first, P3 moves ahead of P1 and P1 shrinks to the s4 §6 pre-step.

**What a defensible state-of-the-art claim is (P5).**
- *Settings.* Ironclad; 1× budget on one core, the 2× figure beside it as the owner's milestone rule allows (h1 §1); win definition stated (Act-3 double boss, or Heart with keys); "no hidden information", backed by the combat leak test and a new out-of-combat one.
- *Seeds.* A sealed slice of 2,000 per setting, drawn once from the fresh pointer (1,443,700, c2 §5) before P1 and never used for selection; the old 500-seed test split is spent (h6 §5). Wilson intervals, in win points.
- *Comparability.* Best fair specialised agent: spire-engine, 21.9% on 1,024 seeds of its own engine, 27.5% with unreleased look-ahead (s6 §3). A lower bound above 21.9% needs about 24% observed on 2,000 seeds; above 27.5%, about 29.5% [computed]. Engines differ (the fidelity fork alone moved this project's A20 from 6.2% to 3.0%, r1 §4), so a simulator figure must say so. Language models are real-game, four-character, 60 runs: 33.3%, interval about 23–46% (s6 §4); "matches" needs an Ironclad real-game A20 Heart rate of 33% or more, "beats" a lower bound above 46%. The 50.1% hybrid is clairvoyant and not a fair comparator (s6 §3).
- *Real game versus simulator.* The simulator is checked against 1,765 recordings (1,466 pass, 49 explained; h1 §1) and the bridge has played three live runs (realgame/README.md). A real-game claim needs at least 200 live A20 runs.
- *Cheapest path.* T1 in the simulator: one sealed 2,000-game run (about 1 core-hour), the out-of-combat leak test (2 days) and 100 live runs as a parity check. For the Heart, any rate reliably above 8% is the first fair specialised Ironclad figure (s6 §4).

## 4. First two weeks

One binary, eight experiments. "Exists" was checked in research/cpp and research/eval. Costs are [estimate] from the per-game times in c2 §5.

| ID | Question | Code | Data | Cost | Readout |
|---|---|---|---|---|---|
| **D1** | What is hidden information worth to our own search, and how much of that is tree depth? | **Missing.** `determinise` is unconditional (FS:470). Add `diag_clair`: 1 skips it, so the copy keeps the true draw order and random streams; 2 = **K-world**: K fixed determinisations per decision, cycled, so hands recur and children past END_TURN are revisited (K = 4, 16). About 20 lines, default off. Precedent: `cheat_combat`, "DIAGNOSTIC ONLY" (FA:146) | 2,000 seeds × {A20, A10, Heart, A20 Heart} × {fair, clairvoyant, K4, K16} at 1×; clairvoyant at 4× and 16× on 500 | 36 core-h | Win and death rate per stage (d1 §2 layout); Heart conversion; `C16`. Unlike the 9.6% row: our search, not lightspeed's. Unlike tree reuse (tie, F41a): worlds recur inside one search |
| **D2** | Where does fair search saturate? | **Exists** (`sims`); only the 900 s alarm is in the way. Print simulations per decision (`stats.simulations`, FA:177, counted but never written) | Current bests at 2–64×; 1,000 seeds at A20 to 16×, 500 otherwise; one 16× arm without early stop (the unexplained high-simulation anomaly, h5 §4.6) | 150 core-h | Win against log budget per track and stage; `S64`; where 1× and 64× choose differently in boss fights |
| **D3a** | Deck or pilot at the Act-2 boss and the Heart? | **Partly.** `bc.init(g, enc)` on a hypothetical run state exists (FA:422-446); the bridge injects cards, relics, potions and keys (realgame/Scenario.cpp:152-197). Missing: replace-deck and set-HP spec, potions on, full `sims`. About 150 lines | 40 states at floor 33 × 3 bosses and 40 at floor 55 × Heart, from three sources: expert logs (§6), 24 decks written from `ironclad_archetypes.md` and the expert guide (w1 §3), the agent's own winners / median / losers. 100 fights per cell at 1×, 4×, 16×, clairvoyant | 44 core-h | The 3 × 4 matrix of fight win rate and HP lost; `Ref`, `Own` |
| **D3b** | Were fatal fights winnable? | **Partly.** In-process copies with a private map and re-seeded streams exist (FA:1420-1432) | 400 lost A20 and 300 lost Heart runs: fatal fight × 16 replays at 1×, 16×, clairvoyant; fatal act × 8 replays | 10 core-h | `Doomed`: the zero-search paper's attribution (w1 §1.2) on this agent |
| **D3c** | What does win rate pay per unit of each resource? | **Missing**, about 80 lines: a driver over the public `playBattle` and `stepOutOfCombat` (FairAgent.h) that applies gifts between steps | 2,000 seeds A20 and Heart per arm: HP restored after hallway fights; full heal before bosses; +2 relics per act; 3 Strikes removed and 3 upgrades; elites forced with and without the HP gift | 8 core-h | `G_hp`, `G_deck` as causal effects, replacing "winners hold 10.2 vs 8.4 relics" (d1 §4) |
| **D4** | How far is the profile from experts? | **Mostly exists**: `FAIR_TRACE` (eval_main.cpp:54) and analysis/trace_stats.py give removals, upgrades, relics, shop gold, rests and HP per fight. Missing: elites per act, skip rate per act, deck size at bosses | The D1 fair arm, traced; against the published expert table (w1 §2.2) and, if logs are allowed, per-fight damage as the par of d1 §6.8 | 0 extra | One table: agent / streamers / population / language models |
| **D5** | Luck or policy? | **Missing**: an agent-seed salt (eval_main.cpp:51 ties it to the run seed) | 8 replicates × 1,000 seeds × {A20, A10, Heart} | 9 core-h | Per-seed win probability; share of seeds won at least once; effective sample size for gates |
| **D6** | What does the Heart fight look like? | **Exists** (`FAIR_TRACE`) | 500 Heart-track games | 1 core-h | Entry HP, deck, relics, potions; turns survived; Heart HP left; missing key (d1 §6.3, §6.7) |

Schedule. Days 1–3: switch, binary, bit-identity gate, D2 and D5 running. Days 4–6: fight bench and deck injector; D1, D3b, D4, D6. Days 7–9: reference decks, D3a, D3c. Day 10: memo. The fleet finishes all compute in about nine hours.

Go or stop. Validity first: if the clairvoyant arm is not at least as good as the fair arm, or the fair arm misses the ledger level, the instrument is wrong and nothing is read. Then the §2.2 table decides. Two results would stop a direction outright: `C16` under 15% ends combat-engine-first; `Ref` within 10 points of `Own` at both the boss and the Heart ends strategy-first as a deck argument. If the patterns conflict, the unified-staged branch is the default, because its first build is shared.

## 5. Expected outcome

- **T1, A20 Act-3 ≥ 25%: 45%.** For: an existence proof at 21.9–27.5% under a comparable budget (s6 §3); Mentor's campaigns gave +7.5 to +15.5 points each (s4 §6); search adds 4.8–7.8 points over a search-free policy at high ascension (s4 §1). Against: twelve phases of value work moved A20 by 1–2 points (h4 §6.4); the anti-expert profile was reached by experiment; engines differ. I put the same build without P0 at about 37% [estimate]: the diagnostics buy the difference by ordering P1 and P3 correctly and by sizing the training signal.
- **T2, A20 Heart ≥ 20%: 12%.** It needs about 35–40% keyed Act-3 clears times 50–60% Heart conversion; today these are 3.65% and 14% (d1 §2). No fair specialised agent is above 8% on any character (s6 §3).
- **T3, A20 Heart ≥ 45%: 3%.** That is the clairvoyant hybrid's level, reached there with 32,000–384,000 simulations (s6 §1a), here required without hidden information at about 1,000.

Most likely failure: the mixed pattern. The strategy policy lifts A20 to 12–18% and stalls because, given this engine's HP cost per fight, avoiding elites remains the correct policy; the engine then gains on the bench but shrinks in full runs, as most kept gains did on confirmation (h3 §2); Heart conversion stays near 50% at A0 and A20 Heart stays under 5%. Second: a diagnostic artefact (expert decks that need expert piloting, clairvoyance confounded with depth), which is why D1 has the K-world arm, D3a three deck sources and D3c causal gifts.

## 6. Conflicts and owner decisions

1. **Clairvoyant switch for measurement only.** It breaks the play-time rule by design, never enters the ledger as an agent result, and lives in the diagnostic binary.
2. **Search at 8–64× as information.** The standing ruling is 2× for milestone scores only (h1 §1).
3. **A separate diagnostic binary**, so the frozen harness is untouched.
4. **Expert reference data.** Allow network access to public expert run logs (the 687-run streamer history used by SpireBench, s6 §2) or supply the files. Without it D3a runs on KB-authored and own decks only, D4 on published aggregates, and the par of d1 §6.8 stays unknown.
5. **Conditional reopening of the learned evaluator**, asked only if the table selects the combat or unified branch. It is not a continuation: the target is the realised outcome of 16× and clairvoyant play, one sample per fight, instead of the mean of 8 random rollouts; the input is entity-level and sparse instead of 204 aggregates; it sits at the turn boundary under a K-world tree instead of replacing rollouts everywhere.
6. **Rejected results touched, and what differs.** Models replacing rules lost −0.07 to −0.62 (r1 §3): P1 optimises a policy on run outcome with exploration and never ranks options with an observational value. The hallway potion ban was a keep (+0.049): it is lifted only inside P2, with an identity-aware value and its own gate. Value on HP trade-offs lost (R6, F5): P2's critic is trained with those decisions in its action set and has a kill rule.
7. **Gates in win points** on paired seeds, not the floor-plus-win score, for A20 and Heart (the score hides wins, 00 §7.15).
8. **A sealed test slice** and **an A20 Heart track** with its own objective (never trained, 00 §3 row r).
9. **Money and machines.** About $16 for P0, $320–410 in total, CPU only. Live validation runs need the owner's Mac.
10. **Play-time budget:** no change requested; the strategy network's 1–2% is taken out of `sims`.

## 7. What this plan deliberately does not do, and what from the other possible directions it would adopt later

Not done:
- No architecture is committed before P0 reports; the §2.3 design is a forecast.
- No network in the tree above a few microseconds, no transformer at play time (s5 §6).
- No learned fight-outcome surrogate in place of fights during training; Mentor's made fights look twice as survivable (s4 §1).
- No further single-parameter tuning, no budget moves between fights (rejected three times, 00 §2), no language model at decision time, no imitation of mass human logs (w1 dead ends).
- No search-free end-to-end agent.

Adopted later, when the table or a gate calls for it:
- From combat-engine-first: bucketed draw nodes, a sparse incrementally updated boundary value, balance-trained playout weights at zero run-time cost (w2 §B2, §B4), common random numbers with sequential halving at the root (c1 §6.4).
- From strategy-first: the privileged critic, failure-weighted restarts, boss practice on reference decks (the D3a bench doubles as that curriculum).
- From the unified direction: one network with auxiliary heads across ascensions, and the post-fight value as the shared currency between fights and the run.
- From engineering practice: joint tuning of the remaining constants under sequential paired tests, as background work only.
