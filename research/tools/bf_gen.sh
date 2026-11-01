#!/usr/bin/env bash
# Generate the interventional fight dataset with the current best agent on train seeds.
# usage: bf_gen.sh OUT_NAME [asc=0] [games=500] [budget_secs=9000]
# Reads the smoke run's per-game cost (research/vf/data/bf_smoke/games.tsv) and thins the logged
# decisions (bf_every) so the whole generation fits the wall-clock budget on 4 threads.
set -euo pipefail
cd "$(dirname "$0")/../.."
NAME=${1:?name}; ASC=${2:-0}; GAMES=${3:-500}; BUDGET=${4:-9000}
SMOKE=research/vf/data/bf_smoke/games.tsv
SECS=$(awk '{s+=$11; n++} END {if (n) printf "%.1f", s/n; else print 60}' "$SMOKE")
EVERY=$(python3 -c "import math; print(min(4, max(1, math.ceil($SECS * $GAMES / 4.0 / $BUDGET))))")
echo "smoke cost ${SECS}s/game at bf_every=1 -> bf_every=$EVERY for $GAMES games within ${BUDGET}s"
PARF=research/best_params.txt; [ "$ASC" = 10 ] && PARF=research/best_a10_params.txt; [ "$ASC" = 20 ] && PARF=research/best_a20_params.txt
ARGS=$(sed 's/^/--param /' "$PARF" | tr '\n' ' ')
mkdir -p "research/vf/data/$NAME"
VF_LOG_DIR="research/vf/data/$NAME" python3 research/eval/run.py "$NAME" train $ARGS --param bf_log=1 --param bf_every=$EVERY --asc "$ASC" | tail -2
python3 research/eval/log.py "$NAME" train info "$NAME: $GAMES train games of the asc-$ASC best with the interventional fight logger (bf_log=1, bf_every=$EVERY, bf_sims 100)" --asc "$ASC" > /dev/null
echo "BF GEN $NAME DONE"
