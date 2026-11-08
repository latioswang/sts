#!/usr/bin/env bash
# R4-local subsamples -> HF store, verify file counts there, then free local disk (marker for the R4 queue guard).
set -uo pipefail
cd "$(dirname "$0")/../.."
M=research/vf/data/.a10_local_pushed
for b in 0 1 2 3 4 5; do
  n=rundata_a10_l${b}_k32; want=$(ls research/vf/data/$n/all | wc -l)
  for try in 1 2 3 4 5 6; do
    STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push $n 2>&1 | grep -E "^PUSH|Error|limit" | tail -3
    have=$(python3 research/tools/data_sync.py ls 2>/dev/null | awk -v n=$n '$1==n{print $2}')
    echo "$(date -u +%H:%M:%S) $n local $want store ${have:-0}"
    [ "${have:-0}" = "$want" ] && break
    sleep 600
  done
  [ "${have:-0}" = "$want" ] || { echo "PUSH LOCAL FAILED $n"; exit 1; }
done
for b in 0 1 2 3 4 5; do rm -rf research/vf/data/rundata_a10_l${b}_k32; done
touch "$M"; echo "LOCAL A10 PUSHED AND FREED $(df -h /home/user | tail -1)"
