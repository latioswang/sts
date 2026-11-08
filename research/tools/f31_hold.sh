#!/usr/bin/env bash
# Phase F31 (pre-registered in program.md F31): the held-out A10 decision set (local) — 1,600 seeds 5,460,000-5,461,599, one
# card-reward decision per seed (k = seed % 10), M=8 playouts per option by the A10 best; never used for training.
set -euo pipefail
cd "$(dirname "$0")/../.."
D=research/vf/data/branch_f31_hold; mkdir -p $D
VF_LOG_DIR=$D /home/user/sts_lightspeed/build/fair_eval --agent fair --seed_start 5460000 --count 1600 --asc 10 --threads 4 \
  $(sed 's/^/--param /' research/best_a10_params.txt | tr '\n' ' ') --branch_eval 10 8 > $D/summary.tsv
echo "F31 hold: $(grep -c BRANCH $D/summary.tsv) branch, $(awk -F'\t' '$10==2' $D/summary.tsv | wc -l) crashed"
python3 research/analysis/f26_branch.py $D 171 251 | sed 's/^F26/F31 HOLD/'
