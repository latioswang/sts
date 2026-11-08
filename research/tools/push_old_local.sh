#!/usr/bin/env bash
# Old local-only data sets (Phase A/B: gen0, gen1; Phase S: svdata) -> HF store, verify counts, free local disk.
set -uo pipefail
cd "$(dirname "$0")/../.."
D=research/vf/data; M=$D/.old_local_pushed
for n in gen0 gen1; do
  [ -d $D/$n/all ] || { mkdir -p $D/$n/all; find "$(pwd)/$D/$n" -maxdepth 1 -type f -name '*.bin' -exec ln -sf {} $D/$n/all/ \; ; }
done
for n in gen0 gen1 svdata; do
  want=$(ls $D/$n/all | wc -l)
  for try in 1 2 3 4 5 6; do
    STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push $n 2>&1 | grep -E "^PUSH|Error|limit" | tail -3
    have=$(python3 research/tools/data_sync.py ls 2>/dev/null | awk -v n=$n '$1==n{print $2}')
    echo "$(date -u +%H:%M:%S) $n local $want store ${have:-0}"
    [ "${have:-0}" = "$want" ] && break
    sleep 600
  done
  [ "${have:-0}" = "$want" ] || { echo "PUSH OLD FAILED $n"; exit 1; }
done
for n in gen0 gen1 svdata; do rm -rf $D/$n; done
touch "$M"; echo "OLD LOCAL PUSHED AND FREED $(df -h /home/user | tail -1)"
