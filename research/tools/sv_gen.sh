#!/usr/bin/env bash
# Phase S data: play train-seed games with the A0 best and the policy/root-value logger (pf_log=1), in
# resumable chunks. usage: sv_gen.sh NAME GAMES CHUNK SEED0   (logs: research/vf/data/NAME/chunk_K/pol_<seed>.bin)
set -euo pipefail
cd "$(dirname "$0")/../.."
NAME=${1:?}; GAMES=${2:?}; CHUNK=${3:?}; SEED0=${4:?}
BIN=${FAIR_EVAL_BIN:-/home/user/sts_lightspeed/build/fair_eval}
ARGS=$(sed 's/^/--param /' research/best_params.txt | tr '\n' ' ')
OUT=research/vf/data/$NAME; mkdir -p "$OUT"
CHUNKS=$(( (GAMES + CHUNK - 1) / CHUNK ))
for ((k = 0; k < CHUNKS; k++)); do
  D=$OUT/chunk_$k
  [ -f "$D/.done" ] && continue
  rm -rf "$D"; mkdir -p "$D"
  S=$(( SEED0 + k * CHUNK )); N=$CHUNK; [ $(( S + N )) -gt $(( SEED0 + GAMES )) ] && N=$(( SEED0 + GAMES - S ))
  T0=$(date +%s)
  VF_LOG_DIR=$D "$BIN" --agent fair --seed_start "$S" --count "$N" --asc 0 --threads 4 $ARGS ${LOGP:---param pf_log=1} > "$D/games.tsv"   # LOGP env: other loggers (Phase S2: --param leaf_log=1)
  touch "$D/.done"
  echo "sv chunk $k/$CHUNKS done: $N games, $(( $(date +%s) - T0 ))s, $(du -sh "$D" | cut -f1)"
done
# flatten: the trainer globs pol_*.bin in one directory
mkdir -p "$OUT/all"; find "$OUT" -path "$OUT/all" -prune -o \( -name 'pol_*.bin' -o -name 'leaf_*.bin' -o -name 'run_*.bin' \) -print0 | xargs -0 -I{} ln -sf "$(pwd)/{}" "$OUT/all/"
echo "SV GEN $NAME DONE: $(ls "$OUT/all" | wc -l) files"
