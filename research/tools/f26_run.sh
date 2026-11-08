#!/usr/bin/env bash
# Phase F26 (measurement only; pre-registered in program.md F26): decision-level evaluation of run-value models at A20 from
# branch playouts. 1,600 seeds (5,400,000-5,401,599, unused), one card-reward decision per seed (k = seed % 10), each option
# played out M=8 times by the A20 best from re-seeded copies (same 8 seeds for every option); then f26_branch.py.
set -euo pipefail
cd "$(dirname "$0")/../.."
D=research/vf/data/branch_f26_a20; mkdir -p $D
VF_LOG_DIR=$D /home/user/sts_lightspeed/build/fair_eval --agent fair --seed_start 5400000 --count 1600 --asc 20 --threads 4 \
  $(sed 's/^/--param /' research/best_a20_params.txt | tr '\n' ' ') --branch_eval 10 8 > $D/summary.tsv
echo "F26 seeds done: $(grep -c BRANCH $D/summary.tsv) branch, $(awk -F'\t' '$10==2' $D/summary.tsv | wc -l) crashed"
awk -F'\t' '$10==2{print "F26 CRASH seed " $1}' $D/summary.tsv | head -20
python3 research/analysis/f26_branch.py $D 171 240 241 243
