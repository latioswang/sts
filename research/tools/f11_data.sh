#!/usr/bin/env bash
# Phase F11 data (pre-registered in 2d35a27): 15,000 Heart-track games with the Heart best + exploration, run-state logger;
# holdout = first 1,000 runs (chunks 0-9, all states); k32 training subsample of all runs; then the raw non-holdout chunks
# are deleted (regenerable from the seeds) to stay within the disk allowance.
set -euo pipefail
cd "$(dirname "$0")/../.."
N=rundata_f11_heart; D=research/vf/data
ASC=0 PARF=research/best_heart_params.txt LOGP="--param rv_log=1 --param explore_eps=0.2 --param keys=1 --param chest_pol=1" \
  research/tools/sv_gen.sh $N 15000 100 5240000
mkdir -p $D/${N}_hold/all
for k in 0 1 2 3 4 5 6 7 8 9; do ln -sf "$(pwd)"/$D/$N/chunk_$k/run_*.bin $D/${N}_hold/all/; done
echo "F11 HOLD files: $(ls $D/${N}_hold/all | wc -l)"
python3 research/vf/subsample_runs.py $D/$N/all $D/${N}_k32/all --k 32
echo "F11 K32 files: $(ls $D/${N}_k32/all | wc -l)"
rm -rf $D/$N/all
for k in $(seq 10 149); do rm -rf $D/$N/chunk_$k; done
echo "F11 DATA DONE $(du -sh $D/$N | cut -f1) raw kept (holdout chunks), $(du -sh $D/${N}_k32 | cut -f1) k32"
