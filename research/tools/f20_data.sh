#!/usr/bin/env bash
# Phase F20 data (pre-registered in program.md F20): 30,000 A20 games with the A20 best + exploration and the v1 run-state
# logger (rv_log=1, 666 features); holdout = first 1,000 runs (chunks 0-9); k32 subsample; empty files (crashed games)
# dropped; raw non-holdout chunks' run files deleted afterwards (games.tsv kept: crash seeds).
set -euo pipefail
cd "$(dirname "$0")/../.."
N=rundata_f20_a20; D=research/vf/data
ASC=20 PARF=research/best_a20_params.txt LOGP="--param rv_log=1 --param explore_eps=0.2" \
  research/tools/sv_gen.sh $N 30000 100 5300000
mkdir -p $D/${N}_hold/all
for k in 0 1 2 3 4 5 6 7 8 9; do for f in $D/$N/chunk_$k/run_*.bin; do [ -s "$f" ] && ln -sf "$(pwd)/$f" $D/${N}_hold/all/; done; done
echo "F20 HOLD files: $(ls $D/${N}_hold/all | grep -c '^run_')"
python3 research/vf/subsample_runs.py $D/$N/all $D/${N}_k32/all --k 32
echo "F20 K32 runs: $(ls $D/${N}_k32/all | grep -c '^run_')"
rm -rf $D/$N/all
for k in $(seq 10 299); do find $D/$N/chunk_$k -name "run_*.bin" -delete; done
for k in $(seq 0 299); do awk -F'\t' -v k=$k '$10==2{print "F20 CRASH seed " $1 " chunk " k}' $D/$N/chunk_$k/games.tsv; done
echo "F20 DATA DONE $(du -sh $D/$N | cut -f1) raw kept (holdout chunks), $(du -sh $D/${N}_k32 | cut -f1) k32"
