# Real-game bridge

Connects the research agent (`research/cpp/FairAgent`) to the real Slay the Spire through Communication Mod.

- **Mirror mode** plays the agent inside `sts_lightspeed` on the game's seed, sends each action to the game, and stops at the first state difference. Every difference is a simulator bug or a bridge bug.
- **Live mode** lets the agent play the real game. Each decision rebuilds a simulator state from the game's JSON, so a simulator difference cannot derail the run.

## Status

| Part | State |
|---|---|
| Simulator build on macOS | Works |
| Agent hooks | Unset in research runs; 40 `fair_eval` games identical before and after |
| JSON to simulator converter | Passes the offline round trip: 100 games, 42,765 battle decisions, 0 action or visible-field differences, no dependence on draw order |
| Divergence report | Works on sample results; no real results yet |
| Game launch and Communication Mod connection | **Not run yet** |
| Mirror mode on the real game | **Not run yet** |
| Live mode on the real game | **Not run yet** |

The mods are not installed on this machine yet. Everything that talks to the game is written against the mod's Java source and has never talked to the game, so expect bridge bugs on first contact.

## Setup

Machine: Apple silicon Mac, `arm64`. The bridge runs natively. The game runs under Rosetta because its bundled Java 8 is `x86_64`.

**1. Game and Rosetta**

Install Slay the Spire (app 646570) through Steam. It ships its own Java 8 in `SlayTheSpire.app/Contents/Resources/jre`, so no separate Java install is needed.

```bash
softwareupdate --install-rosetta --agree-to-license
```

**2. Mods.** Subscribe on the Steam Workshop. Steam then downloads them to `~/Library/Application Support/Steam/steamapps/workshop/content/646570/<id>/`.

| Mod | Workshop ID |
|---|---|
| ModTheSpire | 1605060445 |
| BaseMod | 1605833019 |
| Communication Mod | 2131373661 |

`steamcommunity.com` may be unreachable from this Mac's network (seen on 2026-11-02, both direct and through the local proxy). Subscribing from another device works as well, because the Steam client on this Mac does the download.

**3. Build**

```bash
STS_SIM_DIR=$HOME/Projects/sts_lightspeed STS_TARGETS="fair_eval sts_bridge" research/setup.sh
```

`setup.sh` still defaults to the cloud paths when `STS_SIM_DIR` is unset.

**4. Point the mod at the relay**

```bash
research/realgame/setup_game.sh
```

This writes `~/Library/Preferences/ModTheSpire/CommunicationMod/config.properties` with:

```
command=<python3> <repo>/research/realgame/commrelay.py
runAtGameStart=true
```

The mod splits `command` on whitespace, so neither path can contain a space.

**5. Launch**

```bash
research/realgame/launch_game.sh
```

Then, in the game's settings, turn on **Fast Mode**. Communication Mod's README says it has not been tested without it.

## How the pieces connect

```
game + Communication Mod  <-stdin/stdout->  commrelay.py  <-Unix socket->  sts_bridge or commctl.py
```

The mod owns one child process for the life of the game. The relay is that process, which means the bridge can be rebuilt and restarted without restarting the game. It also records every protocol line to `logs/transcript-*.jsonl`.

Drive the game by hand:

```bash
research/realgame/commctl.py state
```

```bash
research/realgame/commctl.py send "START IRONCLAD 0 ABC"
```

## Mirror mode

```bash
$HOME/Projects/sts_lightspeed/build/sts_bridge mirror --research_dir research --seed_start 1000 --count 20
```

Add `--max_floor 17` to stop at the end of act 1. The seed text is lightspeed's `SeedHelper::getString(seed)`, the same base-35 format as the game, and the agent seed is derived the same way as in `fair_eval`.

Each step goes in this order:

1. Take any real-only steps the simulator has no action for, such as entering the merchant or clicking through a finished room. They use no game randomness.
2. Compare the states. Any hard difference stops the run.
3. Send the agent's action.

Compared: HP, gold, floor, act, boss, keys, relics, potions, deck, map, rewards, shop, boss relics, event ID, and in combat the player, hand, all three piles in order, and each monster's HP, block, powers, move, and intent damage.

A *soft* difference means the comparison itself is unsure (for example a move byte missing from the table). It is logged but does not stop the run.

After a stop, the bridge loses the current run on purpose to get back to the main menu, since the mod has no abandon command. This takes a minute or two.

```bash
research/realgame/report.py
```

This writes `reports/divergences.md` with the first difference per seed, a repro command, and a suspected cause. The suspected cause is only where to look first. Confirm it from the log before calling it a simulator bug, and do not change simulator behaviour without saying so, because the research results depend on it.

## Live mode

```bash
$HOME/Projects/sts_lightspeed/build/sts_bridge live --research_dir research --seed_start 5000 --count 3
```

`logs/live-<seed>.jsonl` has one line per decision: state summary, chosen action, simulations, root visits, each root action's visits and value, search time, and converter warnings. `t: "fallback"` lines mark decisions the agent could not make, which the bridge resolved with a simple rule. Those are the first cases to debug. Results go to `logs/live_results.jsonl`.

**Leak rule.** The converter never reads the run's seed; simulator RNG streams come from the bridge's own generator. The draw pile is reordered independently of its true order, except for cards the player knows are on top (Headbutt, Warcry), and the search re-shuffles it on every simulation anyway. The self-test checks this on every decision by reversing the JSON pile and requiring an identical result.

**Known limits of a state rebuilt from JSON**

- Unknown until seen, as they are to a player: a louse's bite damage and a Darkling's Nip damage before the first one (defaults are used).
- Not tracked: gold stolen by Looters and Muggers, burning elite position on the map (the JSON does not show it, so this matters only with `keys=1`), bottled cards, and cards auto-played outside the bridge's own commands.
- A monster's move history starts when the bridge first sees the fight, so start the bridge before the fight begins.
- The effects still queued behind a mid-card selection (for example Burning Pact's draw) are not in the rebuilt state.

## Testing without the game

```bash
$HOME/Projects/sts_lightspeed/build/sts_bridge selftest --research_dir research --seed_start 3000 --count 100
```

Plays games in the simulator and sends every decision's state through JSON and the converter. This tests the converter only. It says nothing about whether the simulator matches the game. Event screens with several phases are reported but do not fail the test, because the test's JSON has no real page structure; only the real game can test those.

```bash
python3 -m pytest research/realgame/test_report.py
```

## Things to watch for on the first real runs

- **Neow.** The game offers the full four blessings only if the previous run reached the boss; otherwise it offers two. The simulator always has four. A mirror run after an early death will stop at Neow as a bridge gap.
- **Locked content.** A fresh profile has locked cards and relics. The simulator assumes everything is unlocked, so card rewards would differ for reasons unrelated to RNG.
- **Models.** `bf_model` loads from `research/models`. The bridge sets the path itself and refuses to start if the model fails to load. `fair_eval` falls back to `/home/user/...` and silently plays untrained when the file is missing, so set `BF_MODEL_DIR` when running it on this Mac.
- **Agent randomness across platforms.** `std::default_random_engine` and `std` distributions differ between libc++ and libstdc++, so a given seed can play differently here than on the Linux cloud runs. The simulator's own RNG does not use them.

## Files

| File | Purpose |
|---|---|
| `commrelay.py` | The process the mod launches; relays to a Unix socket and records the transcript |
| `commctl.py` | Drive the game by hand |
| `CommClient` | Bridge side of the socket |
| `GameIds`, `GameIdTables.inc` | Game IDs to simulator enums. Monsters, powers and move bytes come from gamerpuppy/sts_lightspeed PR #3 (MIT), the rest from the simulator's save-file tables |
| `Compare` | Simulator state compared with game JSON |
| `ActionSender` | Simulator actions to mod commands, shared by both modes |
| `Mirror` | Mirror mode |
| `StateConverter` | Game JSON to simulator state |
| `Live` | Live mode |
| `CommJson`, `SelfTest` | Offline converter round trip |
| `report.py` | Divergence report |
| `setup_game.sh`, `launch_game.sh` | Mod config and game launch |

Changes outside this directory, all unset or unchanged in research runs: `FairAgent::onCombatAction` and a public `stepOutOfCombat`; `search::gameActionObserver` in the simulator patch; a `sts_bridge` CMake target; `STS_RESEARCH_DIR` as a CMake cache variable; `STS_SIM_DIR` and `STS_TARGETS` in `setup.sh`.
