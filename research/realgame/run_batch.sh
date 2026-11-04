#!/usr/bin/env bash
# Runs a long mirror or live batch, restarting the game every few seeds. The game's heap fills up over dozens of runs in
# one process (OutOfMemoryError), and a crash partway through a chunk only loses that chunk's remaining seeds.
#
#   run_batch.sh <mirror|live> <seed_start> <count> [--chunk N] [any other sts_bridge options]
#
# STS_TIME_SCALE speeds up the game's animations when the scenario mod is built (scenario_mod/build.sh); mirror runs
# use it, and live runs should not. STS_BRIDGE overrides the bridge binary. Run it from anywhere; the game and bridge logs go to research/realgame/logs/.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
RESEARCH="$(cd "$HERE/.." && pwd)"
BRIDGE="${STS_BRIDGE:-$HOME/Projects/sts_lightspeed/build/sts_bridge}"
SOCK=/tmp/sts_commrelay.sock
GAME_START_TIMEOUT_SEC=120
GAME_APPEAR_SEC=10

mode="$1"; seed="$2"; remaining="$3"; shift 3
chunk=10
extra=()
while [ $# -gt 0 ]; do
  case "$1" in
    --chunk) chunk="$2"; shift 2 ;;
    *) extra+=("$1"); shift ;;
  esac
done

stop_game() {
  pkill -f "ModTheSpire.jar" 2>/dev/null
  sleep 2
  pkill -f "commrelay.py" 2>/dev/null
  sleep 1
  rm -f "${STS_GAME_DIR:-$HOME/.sts_realgame}"/hs_err_pid*.log
}

start_game() {
  # Rosetta occasionally fails to start the game ("Attachment of code signature supplement failed"); another try works.
  local attempt waited
  for attempt in 1 2 3; do
    rm -f "$SOCK"
    mkdir -p "$HERE/logs"
    ("$HERE/launch_game.sh" > "$HERE/logs/game.log" 2>&1 &)
    waited=0
    while [ "$waited" -lt "$GAME_START_TIMEOUT_SEC" ]; do
      if [ -S "$SOCK" ] && python3 -I "$HERE/commctl.py" state 2>/dev/null | grep -q "not in game"; then
        return 0
      fi
      # Java takes a moment to appear. Checking sooner reads as a failed start and launches a second game.
      [ "$waited" -lt "$GAME_APPEAR_SEC" ] || pgrep -f "ModTheSpire.jar" > /dev/null || break
      sleep 2
      waited=$((waited + 2))
    done
    echo "warning: game start attempt $attempt failed: $(tail -n 1 "$HERE/logs/game.log" | cut -c1-120)" >&2
    stop_game
  done
  echo "error: the game did not start" >&2
  return 1
}

trap stop_game EXIT
export BF_MODEL_DIR="${BF_MODEL_DIR:-$RESEARCH/models}"
while [ "$remaining" -gt 0 ]; do
  this=$(( remaining < chunk ? remaining : chunk ))
  stop_game
  start_game || exit 1
  echo "--- $mode seeds $seed to $((seed + this - 1))"
  "$BRIDGE" "$mode" --research_dir "$RESEARCH" --seed_start "$seed" --count "$this" ${extra[@]+"${extra[@]}"}
  status=$?
  [ "$status" -ne 0 ] && echo "warning: this chunk ended early (exit $status); going on with the next one" >&2
  seed=$((seed + this))
  remaining=$((remaining - this))
done
