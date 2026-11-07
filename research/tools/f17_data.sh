#!/usr/bin/env bash
# Phase F17 data (pre-registered in program.md F17; copy of f16_data.sh): 15,000 Heart-track games with the then Heart best + exploration, features-v2
# run-state logger (rv_log2); holdout = first 1,000 runs (chunks 0-9); k32 subsample with --dim 670; empty files (crashed
# games) dropped; raw non-holdout chunks deleted afterwards.
set -euo pipefail
cd "$(dirname "$0")/../.."
N=rundata_f17_heart; D=research/vf/data   # vr_N=rundata_f17_heart
ASC=0 PARF=research/best_heart_params.txt LOGP="--param rv_log2=1 --param explore_eps=0.2 --param keys=1 --param chest_pol=1" \
  research/tools/sv_gen.sh $N 15000 100 5285000
mkdir -p $D/${N}_hold/all; echo "features v2 rows (666 + 4 key features)" > $D/${N}_hold/all/DIM670
for k in 0 1 2 3 4 5 6 7 8 9; do for f in $D/$N/chunk_$k/run_*.bin; do [ -s "$f" ] && ln -sf "$(pwd)/$f" $D/${N}_hold/all/; done; done
echo "F17 HOLD files: $(ls $D/${N}_hold/all | grep -c '^run_')"
python3 research/vf/subsample_runs.py $D/$N/all $D/${N}_k32/all --k 32 --dim 670
echo "F17 K32 runs: $(ls $D/${N}_k32/all | grep -c '^run_')"
rm -rf $D/$N/all
for k in $(seq 10 149); do find $D/$N/chunk_$k -name "run_*.bin" -delete; done   # keep games.tsv (crash seeds)
echo "F17 DATA DONE $(du -sh $D/$N | cut -f1) raw kept (holdout chunks), $(du -sh $D/${N}_k32 | cut -f1) k32"
