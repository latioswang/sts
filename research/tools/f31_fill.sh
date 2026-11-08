#!/usr/bin/env bash
# F31 operational: (re)generate only the branch-eval seeds of DIR in [START, END] whose file is missing, empty or not a whole
# number of rows (2,688 bytes = 672 floats), 4 single-seed processes at a time. Idempotent: re-run after a container restart.
# Branch eval is deterministic per seed, so the result equals an uninterrupted run. usage: f31_fill.sh DIR START END
set -uo pipefail
cd "$(dirname "$0")/../.."
D=$1; A=$2; B=$3; mkdir -p "$D"
for s in $(seq "$A" "$B"); do f=$D/branch_$s.bin
  if [ ! -f "$f" ] || [ ! -s "$f" ] || [ $(( $(stat -c%s "$f") % 2688 )) -ne 0 ]; then echo "$s"; fi; done > "$D/redo.txt"
echo "F31 FILL $D: redo $(wc -l < "$D/redo.txt") of $((B - A + 1)) seeds"
P=$(sed 's/^/--param /' research/best_a10_params.txt | tr '\n' ' ')
xargs -P4 -I{} sh -c "VF_LOG_DIR=$D /home/user/sts_lightspeed/build/fair_eval --agent fair --seed_start {} --count 1 --asc 10 --threads 1 $P --branch_eval 10 8" \
  < "$D/redo.txt" >> "$D/summary_redo.tsv"
n=$(ls "$D"/branch_*.bin | wc -l)
echo "F31 FILL $D: $n files; redo summary: $(grep -c BRANCH "$D/summary_redo.tsv") branch, $(awk -F'\t' '$10==2' "$D/summary_redo.tsv" | wc -l) crashed"
