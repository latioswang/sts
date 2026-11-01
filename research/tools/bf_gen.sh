#!/usr/bin/env bash
# Generate the interventional fight dataset with the current best agent on train seeds, in RESUMABLE
# chunks (the queue runner can be killed by the 2-hour background limit; completed chunks are skipped).
# usage: bf_gen.sh OUT_NAME [asc=0] [games=500] [budget_secs=9000] [chunk=50] [seed_start=300000]
# Reads the smoke run's per-game cost (research/vf/data/bf_smoke/games.tsv) and thins the logged
# decisions (bf_every) so the whole generation fits the wall-clock budget on 4 threads.
# Each chunk's logs live in research/vf/data/OUT_NAME/chunk_K/ (deleted and redone if interrupted).
set -euo pipefail
cd "$(dirname "$0")/../.."
NAME=${1:?name}; ASC=${2:-0}; GAMES=${3:-500}; BUDGET=${4:-9000}; CHUNK=${5:-50}; SEED0=${6:-300000}
BIN=${FAIR_EVAL_BIN:-/home/user/sts_lightspeed/build/fair_eval}
SMOKE=research/vf/data/bf_smoke/games.tsv
SECS=$(awk '{s+=$11; n++} END {if (n) printf "%.1f", s/n; else print 60}' "$SMOKE")
EVERY=$(python3 -c "import math; print(min(4, max(1, math.ceil($SECS * $GAMES / 4.0 / $BUDGET))))")
if [ -z "${PARF:-}" ]; then PARF=research/best_params.txt; [ "$ASC" = 10 ] && PARF=research/best_a10_params.txt; [ "$ASC" = 20 ] && PARF=research/best_a20_params.txt; fi
ARGS="$(sed 's/^/--param /' "$PARF" | tr '\n' ' ') ${EXTRA:-}"   # PARF / EXTRA env: other params (e.g. the Heart track's)
OUT=research/vf/data/$NAME; mkdir -p "$OUT"
echo "smoke cost ${SECS}s/game -> bf_every=$EVERY; $GAMES games in chunks of $CHUNK from seed $SEED0 (params: $PARF)"
CHUNKS=$(( (GAMES + CHUNK - 1) / CHUNK ))
for ((k = 0; k < CHUNKS; k++)); do
  D=$OUT/chunk_$k
  if [ -f "$D/.done" ]; then continue; fi
  rm -rf "$D"; mkdir -p "$D"
  S=$(( SEED0 + k * CHUNK )); N=$CHUNK; [ $(( S + N )) -gt $(( SEED0 + GAMES )) ] && N=$(( SEED0 + GAMES - S ))
  T0=$(date +%s)
  VF_LOG_DIR=$D "$BIN" --agent fair --seed_start "$S" --count "$N" --asc "$ASC" --threads 4 $ARGS --param bf_log=1 --param bf_every=$EVERY > "$D/games.tsv"
  touch "$D/.done"
  echo "chunk $k/$CHUNKS done: $N games, $(( $(date +%s) - T0 ))s, $(du -sh "$D" | cut -f1)"
done
cat "$OUT"/chunk_*/games.tsv > "research/runs/$NAME.train.a$ASC.tsv"
python3 research/eval/log.py "$NAME" train info "$NAME: $GAMES train games of the asc-$ASC best with the interventional fight logger (bf_log=1, bf_every=$EVERY, bf_sims 100)" --asc "$ASC" > /dev/null
echo "BF GEN $NAME DONE"
