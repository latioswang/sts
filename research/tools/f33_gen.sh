#!/usr/bin/env bash
# Phase F33 (offline measurement): split-half branch decisions, resumable. Runs every seed in [START, START+N) that has no
# line yet in DIR/summary.tsv (a line is written only when that seed's process finished, so in-flight seeds are re-run),
# one single-seed process per seed, 4 at a time. Deterministic per seed: re-running after a restart equals an uninterrupted run.
# usage: f33_gen.sh DIR START N ASC PARAMFILE K M KIND     (KIND 1 = card reward, 2 = boss relic; K: decision k = seed % K)
set -uo pipefail
cd "$(dirname "$0")/../.."
D=$1; A=$2; N=$3; ASC=$4; PF=$5; K=$6; M=$7; KIND=$8
mkdir -p "$D"; touch "$D/summary.tsv"
cut -f1 "$D/summary.tsv" | sort -u > "$D/.done"
seq "$A" $((A + N - 1)) | grep -vxFf "$D/.done" > "$D/.todo" || true
echo "F33 GEN $D: $(wc -l < "$D/.todo") of $N seeds to run"
P=$(sed 's/^/--param /' "$PF" | tr '\n' ' ')
xargs -P4 -I{} sh -c "VF_LOG_DIR=$D /home/user/sts_lightspeed/build/fair_eval --agent fair --seed_start {} --count 1 --asc $ASC --threads 1 $P --branch_eval $K $M --branch_kind $KIND" \
  < "$D/.todo" >> "$D/summary.tsv"
echo "F33 GEN $D: $(cut -f1 "$D/summary.tsv" | sort -u | wc -l)/$N seeds done; $(grep -c BRANCH "$D/summary.tsv") branch, $(awk -F'\t' '$10==2' "$D/summary.tsv" | wc -l) crashed, $(awk -F'\t' '$2=="BRANCH" && $3<0' "$D/summary.tsv" | wc -l) no decision"
