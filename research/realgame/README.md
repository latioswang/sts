# Real-game bridge

Connects the research agent (`research/cpp/FairAgent`) to the real Slay the Spire through Communication Mod.

- **Mirror mode** plays the agent inside `sts_lightspeed` on the game's seed, sends each action to the game, and stops at the first state difference. Every difference is a simulator bug or a bridge bug.
- **Live mode** lets the agent play the real game. Each decision rebuilds a simulator state from the game's JSON, so a simulator difference cannot derail the run.

## Status

| Part | State |
|---|---|
| Game launch and Communication Mod connection | Works |
| Mirror mode, 20 seeds to the end of act 1 | 18 matched, 2 stopped where the game itself isn't deterministic ([report](reports/divergences-act1.md)) |
| Mirror mode, acts 2 to 4 | Only checked as far as floor 18 so far; expect more differences |
| Live mode | Won its first real run (seed 5000, floor 51, no fallbacks) |
| Simulator fixes | [On a fork](reports/simulator_fixes.md); the research loop still builds upstream plus the patch |
| JSON to simulator converter | Passes the offline round trip |
| Agent hooks | Unset in research runs |

## Which simulator

`research/setup.sh` builds upstream plus `patches/sts_lightspeed.patch` by default, which is what the research loop uses. The bridge is meant to run against branch `fidelity` of [latioswang/sts_lightspeed](https://github.com/latioswang/sts_lightspeed), which adds the fixes in [reports/simulator_fixes.md](reports/simulator_fixes.md):

```bash
STS_SIM_DIR=$HOME/Projects/sts_lightspeed STS_SIM_BRANCH=fidelity STS_TARGETS="fair_eval sts_bridge" research/setup.sh
```

The fixes change simulation results, so research numbers from before them aren't comparable with runs after them.

## Setup

Machine: Apple silicon Mac, `arm64`. The bridge runs natively. The game runs under Rosetta because its bundled Java 8 is `x86_64`.

**1. Game and Rosetta**

Install Slay the Spire (app 646570) through Steam. It ships its own Java 8 in `SlayTheSpire.app/Contents/Resources/jre`, so no separate Java install is needed.

```bash
softwareupdate --install-rosetta --agree-to-license
```

**2. Mods.** Get them from the Steam Workshop. Steam puts them in `~/Library/Application Support/Steam/steamapps/workshop/content/646570/<id>/`.

| Mod | Workshop ID |
|---|---|
| ModTheSpire | 1605060445 |
| BaseMod | 1605833019 |
| Communication Mod | 2131373661 |

`steamcommunity.com` was unreachable from this Mac on 2026-11-02, both direct and through the local proxy. Steam's console works without it. Open `steam://open/console` and run:

```
workshop_download_item 646570 1605060445
workshop_download_item 646570 1605833019
workshop_download_item 646570 2131373661
```

Subscribing is not needed, and wouldn't help: ModTheSpire's Workshop search needs the Steam API, which doesn't start when the game is launched this way (`no appID found`). `setup_game.sh` copies the jars to the game folder instead.

**3. Build**

```bash
STS_SIM_DIR=$HOME/Projects/sts_lightspeed STS_SIM_BRANCH=fidelity STS_TARGETS="fair_eval sts_bridge" research/setup.sh
```

`setup.sh` still defaults to the cloud paths when `STS_SIM_DIR` is unset.

**4. Game folder, profile and relay**

```bash
research/realgame/setup_game.sh
```

Run this with the game closed. It does three things.

- **Builds a separate game folder**, `~/.sts_realgame` (`STS_GAME_DIR` overrides it). The game keeps preferences, saves and run history in its working directory, so running from here keeps the research profile and runs out of the Steam install, where Steam Cloud could sync them. The Steam folder stays unchanged.
- **Writes the profile the simulator assumes** (`write_profile.py`): everything unlocked, all bosses seen (an unseen boss is forced), the final act on (it needs a win with three characters), tutorials off, Neow on (a fresh profile skips Neow on its first run), Fast Mode on, and Upload Data off so research runs don't reach the developers' stats.
- **Writes** `~/Library/Preferences/ModTheSpire/CommunicationMod/config.properties`:

```
command=<python3> <repo>/research/realgame/commrelay.py
runAtGameStart=true
```

The mod splits `command` on whitespace, so neither path can contain a space. Keep `steam_appid.txt` out of the game folder: without it the Steam API stays off, so these runs can't touch achievements or cloud saves.

**5. Launch**

```bash
research/realgame/launch_game.sh
```

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

## Long batches

The game's heap fills up after dozens of runs in one process. `run_batch.sh` restarts the game every few seeds and carries on after a crash:

```bash
research/realgame/run_batch.sh mirror 1000 20 --chunk 5 --max_floor 17
```

```bash
research/realgame/run_batch.sh live 5000 3 --chunk 2
```

It starts and stops the game itself, so close any game you started by hand.

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

A stop with status `game_nondeterminism` isn't a simulator bug. After a discovery (Attack, Skill, Power or Colorless Potion, or the card), the game rolls `cardRandomRng` once per generation on every frame of about 0.25 seconds, so that stream depends on the frame rate until the next floor. The same seed and actions can play out differently in the game itself.

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

## Things to keep in mind

- **Screen.** Keep the Mac awake and unlocked while a batch runs.
- **Models.** `bf_model` loads from `research/models`. The bridge sets the path itself and refuses to start if the model fails to load. `fair_eval` falls back to `/home/user/...` and silently plays untrained when the file is missing, so set `BF_MODEL_DIR` when running it on this Mac.
- **Agent randomness across platforms.** `std::default_random_engine` and `std` distributions differ between libc++ and libstdc++, so a given seed can play differently here than on the Linux cloud runs. The simulator's own RNG does not use them.
- **Game saves.** `saves/IRONCLAD.autosave` in the game folder is base64 XORed with `key`. Decoding it shows the game's real relic pools and RNG counters, which is the quickest check when a pool looks wrong.

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
| `setup_game.sh`, `write_profile.py`, `launch_game.sh`, `run_batch.sh` | Game folder, profile, launch and batches |
| `reports/` | Divergence reports and the list of simulator fixes |

Changes outside this directory, all unset or unchanged in research runs: `FairAgent::onCombatAction` and a public `stepOutOfCombat`; `search::gameActionObserver` in the simulator patch; a `sts_bridge` CMake target; `STS_RESEARCH_DIR` as a CMake cache variable; `STS_SIM_DIR` and `STS_TARGETS` in `setup.sh`.
