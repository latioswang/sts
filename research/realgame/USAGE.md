# Using the bridge and the recordings

Ways to use this work, from the lightest to the real game. Everything up to [Run against the real game](#run-against-the-real-game) needs no copy of Slay the Spire, and was run from fresh clones of both repositories on 2026-11-09.

| You want to | You need | Go to |
|---|---|---|
| A simulator that plays like the game, for your own agent | Only [latioswang/sts_lightspeed](https://github.com/latioswang/sts_lightspeed) | Its [USAGE.md](https://github.com/latioswang/sts_lightspeed/blob/master/USAGE.md) |
| Replay 1,765 real runs through your build of the simulator | Both repositories and `zstd` | [Replay the recordings](#replay-the-recordings) |
| Real-game states and moves as data | This repository, `zstd` and Python | [The recordings as data](#the-recordings-as-data) |
| Check a different simulator against the game | The recordings, and your simulator | [Another simulator](#check-another-simulator) |
| Play seeds on the real game and in the simulator side by side, or let the agent play | Your own copy of the game on Steam, three mods, an Apple silicon Mac | [Run against the real game](#run-against-the-real-game) |

## Two repositories

- **[latioswang/sts_lightspeed](https://github.com/latioswang/sts_lightspeed)** is the simulator, a fork of gamerpuppy's. It builds and works on its own.
- **[latioswang/sts](https://github.com/latioswang/sts)** (this one) has, under `research/`, the agent (`cpp/`), its evaluation (`eval/`), the bridge (`realgame/`) and the recordings (`realgame/recordings/`). The rest of the repository is a strategy knowledge base for human play.

The bridge is compiled with the simulator, through the simulator's CMake, which finds this repository's `research/` folder from `STS_RESEARCH_DIR`.

## Build the bridge

```bash
git clone https://github.com/latioswang/sts.git
git clone --recurse-submodules https://github.com/latioswang/sts_lightspeed.git
cd sts_lightspeed
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSTS_RESEARCH_DIR=$PWD/../sts/research
cmake --build build --target sts_bridge fair_eval -j 8
```

This gives `build/sts_bridge` (mirror mode, live mode, replay and the self-test) and `build/fair_eval` (the agent's evaluation in the simulator). It needs CMake 3.19 or newer and a C++17 compiler, and was checked on macOS with Apple clang 21.

## Replay the recordings

From the `sts` clone:

```bash
python3 -I research/realgame/tools/recordings.py unpack
python3 -I research/realgame/tools/run_regression.py --bridge ../sts_lightspeed/build/sts_bridge
```

`unpack` needs `zstd` on the path (`brew install zstd` or `apt install zstd`) and takes a few minutes. It fills `research/realgame/traces/` with one file per run. The replay takes a minute or two on 4 cores, longer on a busy machine, and on today's simulator ends with:

```
1765 recordings in ...: 1466 pass, 251 limited, 48 fail
commands checked: 754533 of 823344
by status: completed 1382, game_nondeterminism 161, trace_end 84, diverged 82, trace_mismatch 39, no_rng 15, past_real_stop 2
known failures still failing: 48
```

- It exits 1 if any run got worse than [`regression_baseline.tsv`](regression_baseline.tsv), the result each run had when it was last understood. Known failures are listed there with the reason.
- `research/realgame/logs/replay_results.jsonl` has a line per run: where it stopped, why, the first difference and how many moves were checked.
- `--only 20261107` replays one day's runs. `--jobs N` sets the processes.
- After a change to the simulator, rebuild `sts_bridge` and run it again. A fix should make runs get further and never shorter. Once a change is understood, `--update_baseline` records the new results.

The [README](README.md#regression-test) explains what pass, limited and fail mean.

## The recordings as data

Each run is a gzipped text file. The first line is a JSON header, and every line after it is one event:

| Line | What it is |
|---|---|
| `{...}` | The header: `class`, `seed`, `seed_text` (as the game shows it), `asc`, the test `scenario` if any, and start and end times |
| `C <command>` | A command sent to the game, in [Communication Mod](https://github.com/ForgottenArbiter/CommunicationMod)'s language: `START`, `PLAY 3 0` (card from 1, with 0 for the tenth, then target), `END`, `CHOOSE 1`, `POTION`, `PROCEED`, `SKIP`, `CONFIRM`, `RETURN`, `LEAVE`, `WAIT`, `STATE` |
| `G <json>` | The game's state as Communication Mod reports it: screen, deck, relics, potions, map, and in combat the piles and every monster |
| `N rng ...` | The state of each of the game's random streams (two seeds and a counter), reported by the test mod, in runs from 2026-11-04 on |

The game's event text is left out, apart from Neow's options, and the map is kept only on the map screen. There are 1,765 runs: 953 Ironclad, 273 Silent, 273 Defect and 266 Watcher, with 613 at ascension 10 or above.

```python
import collections, glob, gzip, json

plays = collections.Counter()
for path in glob.glob("research/realgame/traces/*.trace.gz"):
    with gzip.open(path, "rt") as f:
        header = json.loads(f.readline())
        state = {}
        for line in f:
            kind, _, rest = line.partition(" ")
            if kind == "G":
                state = json.loads(rest).get("game_state", {})
            elif kind == "C" and rest.startswith("PLAY"):
                hand = state.get("combat_state", {}).get("hand", [])
                index = (int(rest.split()[1]) - 1) % 10   # from 1, and 0 is the tenth card
                if index < len(hand):
                    plays[hand[index]["name"]] += 1
print(plays.most_common(5))   # [('Defend', 50789), ('Strike', 42520), ('Strike+', 23263), ...]
```

The moves were chosen by our agent, plus test scenarios that handed out items, so they are not a sample of human play. They are an exact record of what the real game did with those moves.

## Check another simulator

The comparison in [`Compare.cpp`](Compare.cpp) is written for sts_lightspeed, but the recordings only hold the game's own states and the commands sent to it. To check another simulator, start each run from the header's class, seed and ascension, apply the `C` commands in order, and compare your state with the `G` state that follows each one. `Compare.cpp` shows which fields to trust and how each is read. Some commands have no simulator action, such as entering the shop or clicking through a finished room, and `Mirror.cpp` handles them under "Real-only steps". The `N rng` lines let you check each random stream's counter, which finds an extra or missing roll at the move where it happens.

If you build this for another simulator, we would like to hear about it. The [fix list](reports/simulator_fixes.md) is a checklist of 219 things a Slay the Spire simulator can get wrong, each with the game's rule.

## Run against the real game

You need your own copy of Slay the Spire on Steam, and ModTheSpire, BaseMod and Communication Mod from the Steam Workshop. The scripts were written for an Apple silicon Mac, where the game runs under Rosetta. The [README's Setup](README.md#setup) has the five steps: game and Rosetta, mods, build, game folder and profile, launch.

Then, from the `sts` clone:

```bash
../sts_lightspeed/build/sts_bridge mirror --research_dir research --seed_start 1000 --count 3
```

That plays three seeds on the game and in the simulator, and stops each at its first difference. `--class THE_SILENT` (or `DEFECT`, `WATCHER`) starts as another character, `--asc 20` sets the ascension, and `--max_floor 17` stops after act 1. For long batches, `run_batch.sh` restarts the game every few seeds. It looks for the bridge in `~/Projects/sts_lightspeed/build`, so with the clones side by side set `STS_BRIDGE=$PWD/../sts_lightspeed/build/sts_bridge`. `STS_TIME_SCALE=5` runs the game's clock five times as fast once the test mod is built (see [Long batches](README.md#long-batches)).

```bash
../sts_lightspeed/build/sts_bridge live --research_dir research --seed_start 5000 --count 1
```

That lets the agent play one real run. Each decision is logged to `research/realgame/logs/live-<seed text>.jsonl`.

To give runs particular cards, relics, potions, encounters or all three keys, build the test mod and use a scenario file (see [Scenarios](README.md#scenarios)). `research/realgame/report.py` turns the stops into a report with a repro command for each.

## Contributing

Forks and pull requests are welcome, and so are questions and reports in [the issues](https://github.com/latioswang/sts/issues). Some things that would help:

- **A difference you found.** Give the seed text, character, ascension and floor, and what the game and the simulator did. A Communication Mod transcript or a recording helps most.
- **Other platforms.** On Linux the bridge and relay should need only the game's paths changed in `setup_game.sh` and `launch_game.sh`. Windows would also need the bridge's Unix socket (`CommClient.cpp`) and the bash scripts replaced. Nobody has tried either yet.
- **Event options the agent never takes**, through `--explore_events`, and the [known gaps](reports/simulator_fixes.md#known-gaps).
- **More recordings.** Runs recorded through `commrelay.py` become recordings with `tools/export_traces.py`, which drops the game's event text. Open an issue to share them.

A change to the simulator's behaviour needs a seed where the game shows it, or the game method that decides it, and a row in the [fix list](reports/simulator_fixes.md). Describe the game's rule in your own words: please don't paste decompiled code. Run the replay before and after.
