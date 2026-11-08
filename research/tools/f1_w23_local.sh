#!/usr/bin/env bash
# Phase F1: f1w23's batch regenerated on the coordinator VM (worker blocked on a restart approval it would only take from the
# owner directly; 2026-11-06 04:2x UTC). Same lines as research/workers/f1data.md for W=23 / A0 / SEED 5000000 / HOLD=a0.
# Resumable (sv_gen.sh skips finished chunks). Ends with research/vf/data/.f1w23_local_done.
set -euo pipefail
cd "$(dirname "$0")/../.."
D=research/vf/data
ASC=0 PARF=research/best_params.txt LOGP="--param rv_log=1 --param explore_eps=0.2" research/tools/sv_gen.sh rundata_f1_w23 30000 100 5000000
python3 research/vf/subsample_runs.py $D/rundata_f1_w23/all $D/rundata_f1_w23_k32/all --k 32
mkdir -p $D/rundata_f1_a0_hold/all && for k in 0 1 2 3 4 5 6 7 8 9; do ln -sf $(pwd)/$D/rundata_f1_w23/chunk_$k/run_*.bin $D/rundata_f1_a0_hold/all/; done && ls $D/rundata_f1_a0_hold/all | wc -l
python3 research/vf/runstore.py pack $D/rundata_f1_a0_hold/all $D/rundata_f1_a0_hold_z/all && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_f1_a0_hold_z
python3 research/vf/runstore.py pack $D/rundata_f1_w23_k32/all $D/rundata_f1_w23_k32_z/all && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_f1_w23_k32_z
python3 research/vf/runstore.py pack $D/rundata_f1_w23/all $D/rundata_f1_w23_z/all && STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push rundata_f1_w23_z
rm -rf $D/rundata_f1_w23/chunk_* $D/rundata_f1_w23/all $D/rundata_f1_w23_k32 $D/rundata_f1_a0_hold   # raw copies; the _z sets are verified
touch $D/.f1w23_local_done; echo "F1 W23 LOCAL DONE"
