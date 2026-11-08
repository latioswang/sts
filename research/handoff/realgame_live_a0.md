# Handoff: one real-game A0 run with the best agent, paused before each boss

Written 2026-11-08 by the research coordinator session (cloud). For a Claude Code session running on the owner's Mac, where
Slay the Spire, Communication Mod and the bridge live. The cloud session cannot reach the Mac; everything below has to happen
there.

## Goal

Play **one real Slay the Spire run** (Ironclad, ascension 0, a fresh seed) with the bridge's **live mode**
(`research/realgame/`) driving the **current A0 best agent**. **Pause before each act boss**: acts 1, 2 and 3 (A0 with no keys
has no act 4). At each pause, write the owner a report on the act just played (format below) and wait for their "continue". At
the end, write a whole-run summary.

The agent's decisions must stay exactly the research agent's. Pausing only delays the next command. Do not change the agent, the
parameters or simulator behaviour to make the run "nicer".

## The agent to run

| Item | Value |
|---|---|
| Name | `f6a0_upg`, the confirmed A0 best (`research/best_name.txt`) |
| Params | `research/best_params.txt` on branch **`autoresearch/ironclad`** |
| Strength in the simulator | 85.2% wins on 2,000 fresh seeds (`results.tsv`, split `fs1282000n2000.a0`). The real game will differ: earlier live runs won 2 of 3 |
| Models it loads | `research/models/vr_171.txt` (`vr_model=171`), `research/models/bf_g1.txt` (`bf_model=1`). The move prior (`prior_mlp=1`) is compiled in |
| Simulator | fork `latioswang/sts_lightspeed` at **`1b05bdc`** (`research/eval/sim.txt` / the pin in `research/setup.sh` on `autoresearch/ironclad`) |

Params today (check the file, which wins if they differ):
`aoe_bonus=2 bf_bonus=10 bf_model=1 boss_pol=1 card_pol=1 chest_pol=1 end_turn_w=0.05 ev_pol=1 hall_potions=0 map_discount=0.85
map_pol=1 potion_v=8 potion_v_boss=4 prior_mlp=1 prior_mlp_depth=0 rest_pol=1 scaling_bonus=3 scaling_want=3 select_pol=1
shop_pol=1 upg_power_bonus=2 vr_bonus=60 vr_model=171 vr_relic_w=30 vr_shop=1 vr_upg_w=60 w_elite_act1=1 w_event=2 w_monster=0.5
w_shop_rich=3`

## Three traps (checked from the cloud; verify on the Mac)

1. **The code is split across two branches.**
   - The bridge (`research/realgame/`) is only on the default branch `claude/ironclad-strategy-kb-rHCI9` (merged in PR #11).
   - The agent there is old: its `research/cpp/FairAgent.cpp` has **no `vr_model` support** and there is no
     `research/vf/vr_mlp.h`. A bridge built from the default branch would silently ignore `vr_model`, `vr_bonus`, `vr_relic_w`,
     `vr_shop` and `vr_upg_w`, and play a different, weaker agent.
   - So: build from **`autoresearch/ironclad`** with the bridge folder brought over:
     `git checkout -B live-a0 origin/autoresearch/ironclad && git checkout origin/claude/ironclad-strategy-kb-rHCI9 -- research/realgame`.
     The bridge sources may need small compile fixes against the newer `FairAgent` API. Keep any fix minimal and say what you
     changed.
2. **`VR_MODEL_DIR` must be set.**
   - `research/vf/vr_mlp.h` defaults to the cloud path `/home/user/sts/research/models`.
   - The bridge sets `BF_MODEL_DIR` and `SV_MODEL_DIR` itself (`bridge_main.cpp`), but **not** `VR_MODEL_DIR`. On the Mac the
     V model would fail to load and turn itself off without an error.
   - Export `VR_MODEL_DIR=<repo>/research/models` in the shell that starts the bridge. Then confirm that
     `research/models/vr_171.txt` actually loads: add a one-line stderr message when `vr::model(n)` loads or misses, or check
     that the live log's card picks differ from the KB-only choice.
3. **Live mode cannot pause or resume.**
   - `Live::run()` always begins with `START IRONCLAD <asc> <seed>` and plays to the end.
   - `--max_floor` exists only for mirror mode.
   - The game itself auto-selects the boss: `Live.cpp` sends `CHOOSE boss` when the map shows `boss_available`. That is
     the place to pause.
   - Add a small opt-in flag, e.g. `--pause_before_boss`. When the map screen has `boss_available` and the flag is set:
     - write `{"t":"pause","floor":…,"act":…}` to `logs/live-<seed>.jsonl`;
     - block until a file `logs/continue-<seed>` exists (poll every ~2 s; the relay and the game just wait);
     - delete that file, then send `CHOOSE boss` as before.

   Default off, so other runs are unchanged. The bridge process must keep running across pauses, so start it as a
   long-running background task, not a foreground command that times out.

## Build and run (see `research/realgame/README.md` for the full setup)

```bash
# fork at the pinned commit
git -C $HOME/Projects/sts_lightspeed fetch origin && git -C $HOME/Projects/sts_lightspeed checkout 1b05bdc
# build both targets against this repo's research/ (the fork's CMakeLists adds sts_bridge when research/realgame/bridge_main.cpp exists)
mkdir -p $HOME/Projects/sts_lightspeed/build && cd $HOME/Projects/sts_lightspeed/build
cmake .. -DCMAKE_BUILD_TYPE=Release -DSTS_RESEARCH_DIR=<repo>/research -Dpybind11_DIR="$(python3 -c 'import pybind11;print(pybind11.get_cmake_dir())')"
make -j8 fair_eval sts_bridge
```

(`autoresearch/ironclad`'s `research/setup.sh` has no `STS_SIM_DIR` / `STS_TARGETS` switches. Those are on the default
branch's copy, so the plain cmake above is the simple route.)

**Before the real run:**

1. **Selftest:** `sts_bridge selftest --research_dir research --seed_start 1000 --count 5` must pass. This is the converter
   round trip, and it includes the leak check.
2. **Params:** every key in `best_params.txt` must be read by the agent; grep each key in `research/cpp/`.
3. **Simulator sanity check** (optional, ~1 min): `fair_eval --agent fair --seed_start 1282000 --count 8 --asc 0 --threads 4 $(sed 's/^/--param /' research/best_params.txt | tr '\n' ' ')`
   with `VR_MODEL_DIR` and `BF_MODEL_DIR` set. These seeds are from the 2,000-seed run that gave 85.2%, so they should
   mostly win. Mostly losses means a model didn't load.

**The run:**

```bash
research/realgame/launch_game.sh            # game + relay (README step 5); do NOT set STS_TIME_SCALE for live runs
export VR_MODEL_DIR=$PWD/research/models
$HOME/Projects/sts_lightspeed/build/sts_bridge live --research_dir research --params research/best_params.txt \
    --seed_start <fresh seed, e.g. 7001> --count 1 --asc 0 --pause_before_boss
```

Pick a seed not used before: `logs/live_results.jsonl` lists the earlier live seeds, around 5000. To continue after a
report: `touch research/realgame/logs/continue-<seedText>`.

## What to report at each pause (before the act-N boss)

Use the bridge's own logs:
- `research/realgame/logs/live-<seed>.jsonl`: one line per decision, with state summary, action, search stats and fallbacks.
- `research/realgame/logs/transcript-*.jsonl`: every game state and command, from the relay.

Report numbers taken from these logs only. Don't estimate.

1. **The act in brief.**
   - Path taken: rooms by floor.
   - Each fight: enemies, HP before → after, turns.
   - Elites, and what they dropped.
   - Card rewards (what was offered, what was taken or skipped), relics, potions found and used.
   - Shop purchases and removals, rest-site choices (rest or smith what), event choices.
   - State at the pause: HP / max HP, gold, deck list, relics, potions.
2. **Highlights.**
   - **Damage per card:**
     - Count each `PLAY` as the drop in total enemy HP plus enemy block between the state before the card and the next
       state where the play has resolved.
     - Credit damage from powers or relics with a delay separately ("indirect": Combust, Juggernaut, thorns, end-of-turn
       effects).
     - Give a table: card, plays, total damage, best single play, and the fight it happened in.
   - **Block per card:** the player's block gain at each play, in the same table form. Feel No Pain, Metallicize and
     similar go in an indirect row.
   - **The hardest fight:** HP lost, the turn that decided it, and what the agent did there (cards played, potion use).
   - **Synergies that fired**, each backed by numbers from the logs. Examples:
     - Strength sources × multi-hit or Heavy Blade;
     - Vulnerable timing before big hits;
     - exhaust payoffs (Feel No Pain / Dark Embrace / Sentinel);
     - block-to-damage (Body Slam / Entrench);
     - draw engines.

     If no real synergy fired, say so.
   - **Fallbacks and converter warnings** (`t:"fallback"` lines): how many, and on which screens.
3. Then stop and wait for the owner's "continue".

**After the run:** result (win or death floor), score, the per-card damage and block tables for the whole run, the three
decisions that mattered most and why, and every fallback or oddity seen.

## Rules

- **No hidden information.** The leak rule in `research/realgame/README.md` (live mode) stays as is. Don't add anything
  that reads the seed or the true draw order.
- **No changes to agent or simulator behaviour.** The only code changes allowed: the pause flag, a model-load log line, and
  compile fixes for the branch merge. Commit them to a local branch (e.g. `live-a0`). Push or open a PR only if the owner
  asks.
- **Don't touch Steam.** Keep the game folder `~/.sts_realgame` and the profile that `setup_game.sh` wrote. Don't use the
  Steam install or cloud saves.
- **If something blocks the run, stop and tell the owner** with the exact error. Examples: the selftest fails, a model
  doesn't load, or the bridge stalls. Don't work around it by playing by hand.
