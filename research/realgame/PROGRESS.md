# Progress log

Making the [sts_lightspeed](https://github.com/gamerpuppy/sts_lightspeed) simulator match the real Slay the Spire, checked side by side against the game. This log says what was done and in what order, where it stands, and what's next. For setup and commands, see the [README](README.md). For every fix, see the [fix list](reports/simulator_fixes.md).

## Where things are

| What | Where |
|---|---|
| Simulator with the fixes | Branch `fidelity` of [latioswang/sts_lightspeed](https://github.com/latioswang/sts_lightspeed/tree/fidelity) |
| Bridge to the real game, mirror mode, live mode | `research/realgame/` on branch `realgame/bridge` of [latioswang/sts](https://github.com/latioswang/sts/tree/realgame/bridge) |
| Every fix, with the seed that found it and the game's rule | [`reports/simulator_fixes.md`](reports/simulator_fixes.md) |
| Scenarios used for the runs below | [`plans/`](plans) |
| Debugging scripts | [`tools/`](tools) |

The research loop still builds upstream plus `research/patches/sts_lightspeed.patch`, so none of this changes its results until it switches.

## What was done, in order

Work ran from 2026-11-02 to 2026-11-04.

1. **Connected to the game.** Communication Mod and a small relay let a process drive the real game. Mirror mode plays one seed on the game and in the simulator with the same actions, and stops at the first difference. Live mode lets the agent play the real game.
2. **Matched the profile.** The simulator assumes everything is unlocked, so `write_profile.py` unlocks the game's profile. It also mutes the game.
3. **Forked the simulator.** Fixes went on branch `fidelity`, off the research baseline `e47bb63`. Each difference was read against the game's bytecode before being fixed, and the fix list records the rule.
4. **Played whole runs.** Ordinary seeds 1000 to 1029 ran to the end of each run, which found most of the early differences.
5. **Forced rare content.** The agent only reaches some cards and relics by luck, so a test-only mod gives each seed a few items that hadn't come up. `plan_a` covers what was missing, and `plan_u` gives every card upgraded. `plan_k` gives all three keys, to reach act 4.
6. **Went to ascension 20.** Two audits compared every `ascensionLevel` read in the game's monsters, events and dungeons with the simulator. Real runs then found what the audits missed.
7. **Checked the time scale.** Batches run at 5x. Seed 2123 reached the same state on floor 42 at both speeds, with Reaper killing Reptomancer and two daggers alive. HP afterwards was 76 at 5x and 86 at normal speed, which is what the simulator says. So a difference that looks odd gets rerun at normal speed before anyone believes it. Seed 2115 stopped the same way at 5x, but its normal speed run took another path and never repeated that kill.
8. **Tried what no run had seen.** Ascensions 5, 10, 15 and 18, and 30 new seeds (1100 to 1129). These found new bugs, such as a shop's rare card chance.
9. **Looked at other simulators.** Another fork's fix list found three bugs we still had. See [Other simulators](#other-simulators).
10. **Measured the effect on research.** See [Effect on the agent](#effect-on-the-agent).
11. **Added a regression test.** Every recorded run can now be checked with no game running, so the game is only needed for new runs. See [Regression test](#regression-test).

## How one difference gets fixed

1. A batch reports a stop in `logs/mirror_results.jsonl`. `tools/show_results.py` prints it.
2. `tools/floor_trace.py` or `tools/tail_trace.py` shows the commands and states leading up to it. Seed text is base 35 with no letter O, and `mirror_results.jsonl` lists it for each seed.
3. `tools/jm.py` prints the game's bytecode for the rule. `tools/rng_users.py` lists which classes touch a random stream, and `STS_TRACE_RNG` logs who rolled it.
4. Fix the simulator, rebuild, and run `sts_bridge selftest` at ascension 0 and 20.
5. Rerun the seed on the game, then commit with the seed and floor in the message, and add a row to the fix list.

Rows marked "code" in the fix list were found by reading the game's code and haven't been seen on the game yet.

## Where it stands

Counted on 2026-11-04. Each seed's latest run counts, on the build of that time, so not every seed has run on the newest build.

| Runs on the real game | Result |
|---|---|
| Ascension 0, seeds 1000 to 1029 | 30 of 30 with no difference |
| Ascension 0, seeds 1100 to 1129, new for this round | 30 of 30, after fixes for the three that stopped |
| Ascension 20, seeds 2100 to 2127 with items injected | 27 of 28, 19 reaching floor 50 or later. 2105 stops after a Discovery, which can't be matched |
| Ascension 20, seeds 4000 to 4019 | 20 of 20, most ending early when the agent dies |
| Ascensions 5, 10, 15, 18, seeds 2100 to 2127 with items injected | 28 of 28. 2110 and 2115 needed fixes first, and 2115 ran at normal speed |
| Act 4 with all keys | Five seeds reached the Heart with no difference |
| Live mode | Three real runs: two wins and one loss |

The fix list has 150 rows: 121 found by a seed on the game and 29 found by reading code.

"No difference" means none on that seed's path. It doesn't prove the simulator matches everywhere, and new seeds were still finding bugs in the latest round.

**Known gaps** are listed in the [fix list](reports/simulator_fixes.md#known-gaps). The largest is other classes' cards.

## Effect on the agent

Same 300 validation seeds, `best_params.txt` and `best_a20_params.txt`, on this Mac with `BF_MODEL_DIR` set. `tools/compare_eval.py` prints the comparison.

| Ascension | Baseline `e47bb63` | Fork | Paired difference, 95% |
|---|---|---|---|
| 0 | 69.7% | 67.3% | -2.3% +- 5.8% |
| 20 | 1.7% | 1.0% | -0.7% +- 1.8% |

Neither difference is significant. Deaths on floor 50 at ascension 0 went from 20 to 36, which hasn't been looked into. The research record for these seeds and parameters is 75.0% on Linux, and how much of that gap comes from the platform's random numbers hasn't been checked either. This comparison ran before the last few fixes.

## Other simulators

Surveyed on 2026-11-04 by cloning each one and counting implemented cards, since names can mislead.

| Project | Finding |
|---|---|
| [Attemory/sts_lightspeed](https://github.com/Attemory/sts_lightspeed) (C++, MIT) | Same codebase, plus all of Defect with orbs. The source for Defect |
| [hewzhew/SpireSim](https://github.com/hewzhew/SpireSim) (Rust, MIT) | All four characters, with a comparison tool. The source for Silent and Watcher |
| [klottick/spire-gym](https://github.com/klottick/spire-gym) (Rust, MIT) | Watcher in progress. A second reference |
| [jdc5549/sts_lightspeed](https://github.com/jdc5549/sts_lightspeed) (C++) | Bug fixes found the same way. Three went into `8bf1499` |
| [klottick/sts_lightspeed_all_characters](https://github.com/klottick/sts_lightspeed_all_characters) | No more cards than upstream, despite the name |
| [GaoPeng415/SlayTheSpireHeadlessEngine](https://github.com/GaoPeng415/SlayTheSpireHeadlessEngine) | Decompiled game code with no license. Don't copy from it |

## Regression test

[`tools/run_regression.py`](tools/run_regression.py) plays every recording back through mirror mode with no game. See the [README](README.md#regression-test) for how it works.

Measured on 2026-11-04 on this Mac, with `fidelity` at `729fa36`:

| Measure | Result |
|---|---|
| Recordings | 613 runs, 51 MB |
| Time | 42 seconds on 4 processes with the machine otherwise quiet, and 80 to 95 seconds under load |
| Pass | 307 |
| Limited | 267: mostly a Discovery in a recording with no random streams (132) and other classes' cards (28) |
| Known failures | 39: 35 older recordings whose seed later got further, and 4 not looked into |

**Does it catch a bug?** We reverted one fix (`3347784`, a shop's rare card chance) in a copy of the simulator and ran it. It reported three runs as worse and exited 1, naming seeds 1103 and 1101 and the card rewards that differ. The unchanged build exits 0.

**What it can't do.** It only checks paths the game has already played, so new content still needs the game.

**How often.** A pass costs about a minute, so at 1% of the machine's time it can run every 100 minutes.

## Coverage and score

[`tools/coverage.py`](tools/coverage.py) counts what the runs on the game have exercised and gives a score out of 100. [`tools/update_progress.py`](tools/update_progress.py) runs the regression test and the count together and writes the files behind a private dashboard. Only what still replays with no difference on the current simulator counts.

| Category | Points | Target for each item |
|---|---|---|
| Cards | 35 | 10 plays, 5 upgraded, 8 different situations. Curses and statuses: 5 turns in hand |
| Relics | 15 | Held in 5 runs |
| Potions | 5 | Used 5 times |
| Monsters | 15 | 3 fights below ascension 17, and 3 from 17 up |
| Events | 10 | 3 visits, and every option chosen |
| Powers | 5 | Seen in 3 runs |
| Ascensions | 5 | 3 runs reaching floor 17, at each of 0 to 20 |
| Full runs per class | 10 | 60 clean runs, then 300 |

The class target comes from the rule of three: with no failure in n independent runs, the chance of a failure per run is below 3/n at 95% confidence. So 60 clean runs bound it at 5%, and 300 at 1%. The other targets are judgment calls and can change. A situation is a combination of Strength, Weak, Frail, a Vulnerable or Weak target, a target with block, several monsters, an elite, a boss, and high ascension.

On 2026-11-04 the score is 64.0, with the Defect cards counted from branch `other-classes`.

| Class | Cards played | Clean full runs | What's left |
|---|---|---|---|
| Ironclad | 75 of 75 | 116 | More new seeds, up to 300 |
| Defect | 72 of 76, inside Ironclad runs | 0 | Run the seeds that stopped on other classes' cards, then start runs as Defect |
| Silent | 0 of 75 | 0 | The port is in progress |
| Watcher | 0 of 77 | 0 | Port it after Silent |

Every run so far starts as Ironclad. The simulator can set up the other classes' starting decks and relics, but mirror mode only starts Ironclad, and the agent's choices outside combat are tuned for it, so starting as the other classes is work still to do.

A score of 100 means these targets were met. It doesn't prove the simulator matches everywhere.

## How the agent and the checks compare

Checked on 2026-11-04. Other projects use their own seeds and setups, so these figures are a guide and not a controlled comparison.

| Agent | Setting | Result | Played on |
|---|---|---|---|
| This project, Ironclad | Ascension 0 | 31 wins on 60 plain seeds (52%) | Real game, mirror mode |
| This project, Ironclad | Ascension 0 | 34 wins on the same 60 seeds (57%). 67.3% over 300 validation seeds | Simulator, the fork |
| This project, Ironclad | Ascension 0 | 75.0% over 300 validation seeds, the research record from before the fixes | Simulator, Linux |
| This project, Ironclad | Ascension 20 | 1.0% over 300 validation seeds on the fork. The research record is 4.0% | Simulator |
| This project, Ironclad | Live mode | 2 wins in 3 runs | Real game |
| [bottled_ai](https://github.com/xaved88/bottled_ai), Ironclad | Ascension 0 | 20% over 50 runs, average floor 32.7 | Real game |
| bottled_ai, Watcher, its best | Ascension 0 | 52% over 50 runs | Real game |
| [sts-rl-agent](https://huggingface.co/Jialeiv/sts-rl-agent) | Ascension 0 | 14% over 50 seeds | sts_lightspeed |
| GPT-6.1 Sol, from [Epoch AI](https://epoch.ai/publications/ai-systems-are-improving-rapidly-at-slay-the-spire-and-its-sequel) | Ascension 0 | 97% over 60 runs, all classes | Not checked |
| GPT-6.1 Sol | Ascension 20 with the Heart | 20% over 60 runs. Ironclad 27% over 15 | Not checked |

- **bottled_ai's ascension.** Its documents don't name one, and some articles say 20. Its code starts every run with `start <class> 0 <seed>`, which is ascension 0, at tag `release-03` as well. The ten Ironclad wins at the top of its table score 726 to 828. Ours score around 800 at ascension 0 and 1,863 to 2,308 at ascension 20. Its 50 Ironclad runs give a 95% interval of 8.9% to 31.1%.
- **Mirror runs are real play.** The agent's moves are played on the real game, so they count as well as live runs. On seeds 1000 to 1029 and 1100 to 1129, each seed's latest run won 31 of 60, with a margin of about 13 points. `fair_eval` on the same seeds, with the same settings on a newer build, won 34, and 52 of the 60 ended the same way on the same floor. The other 8 ran on older builds, which is the likely reason, but this hasn't been checked. So the gap down from 67% or 75% isn't a gap between the simulator and the game. It comes from which seeds were played, the platform and the fixes, and how much from each hasn't been measured.
- **Ascension 20.** No open source project we looked at publishes an ascension 20 win rate we could confirm. AlphaStS's README gives no win rate for whole runs.

Real game checks in other simulators, from their own documents:

| Project | Checked against the game |
|---|---|
| This project | 360 runs on 128 seeds, about 156,000 compared steps. Mostly Ironclad, ascensions 0 to 20. Counts repeat runs and runs on older builds |
| SpireSim | Four complete ascension 20 Heart runs, one per class, plus recorded cases |
| sts-engine | Act 1 Ironclad snapshots |
| spire-gym | Its README says it doesn't establish parity with the game |

Anything borrowed gets the same treatment as our own fixes: read it against the game's bytecode, then run it on the real game.

## What's next

1. **Other classes' cards.** 212 of the 320 cards in Prismatic Shard's pools do nothing when played. Port Defect from Attemory's fork, then Silent and Watcher from SpireSim. Check each card against the game's bytecode and on the real game, and keep Ironclad results unchanged on runs without other classes' cards.
2. **Empty Cage and Tiny House.** The agent never takes them, so they've only been compared with the game's code.
3. **The floor 50 deaths** above.

Cleanup put off for later:

- Make `fidelity` the fork's default branch, or at least point to it from `master`.
- Move the bridge to its own repository. It sits in a repository that opens on a strategy knowledge base.
- Decide whether to offer the fixes to upstream. `fidelity` also carries the research patch, so it would need splitting first, and upstream hasn't been pushed since 2024.
- Write a contributor guide, then share the project.

## Things that caught us out

- **`fair_eval` without `BF_MODEL_DIR`.** On this Mac it silently plays untrained, which cost one comparison.
- **Passing parameters in zsh.** `$P` doesn't split into words, so `fair_eval` silently used its defaults. Use an array.
- **A self-test pass doesn't prove much.** A crash from `areMonstersBasicallyDead` passed 50 seeds of self-test and only appeared on a real run at floor 50.
- **Scenario relics in the wrong pool.** A boss relic put in the common pool can arrive through Neow or Scrap Ooze, which a real run can't do, and the stop that follows means nothing. Offer it through a boss chest instead (`plan_b`).
- **Normal speed needs longer waits.** Mirror's waits were tuned at 5x. At normal speed they ran out during a Smoke Bomb, after Nilry's Codex and after a transform, and each looked like a difference until the wait was lengthened.
- **Rebuilding during a batch.** Don't rebuild `build/` while one is running. Use a second build folder.
