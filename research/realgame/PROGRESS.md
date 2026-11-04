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
7. **Checked the time scale.** Batches run at 5x. Seed 2123 stopped at 5x and matched to floor 52 at normal speed, so a difference that looks odd gets rerun at normal speed before anyone believes it.
8. **Tried what no run had seen.** Ascensions 5, 10, 15 and 18, and 30 new seeds (1100 to 1129). These found new bugs, such as a shop's rare card chance.
9. **Looked at other simulators.** Another fork's fix list found three bugs we still had. See [Other simulators](#other-simulators).
10. **Measured the effect on research.** See [Effect on the agent](#effect-on-the-agent).

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
| Ascensions 5, 10, 15, 18, seeds 2100 to 2127 with items injected | 27 of 28. 2110 ran clean to floor 51 after its fix. 2115 is below |
| Act 4 with all keys | Five seeds reached the Heart with no difference |
| Live mode | Three real runs: two wins and one loss |

The fix list has 149 rows: 120 found by a seed on the game and 29 found by reading code.

"No difference" means none on that seed's path. It doesn't prove the simulator matches everywhere, and new seeds were still finding bugs in the latest round.

**Open checks:**

- **Seed 2115, ascension 10.** At 5x it stopped when Reaper killed Reptomancer with minions alive, the same pattern as seed 2123's time scale artifact. It's rerunning at normal speed to confirm.

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
