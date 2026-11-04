#!/usr/bin/env bash
# Push every local shard-format run-data set to the HF store as <name>_z, verify the stored file count.
set -uo pipefail
cd "$(dirname "$0")/../.."
D=research/vf/data
for d in $D/rundata*/; do
  n=$(basename "$d"); case $n in *_z) continue;; esac
  [ -f "$d/all/index.tsv" ] || { echo "SKIP $n (not shards)"; continue; }
  z=${n}_z; [ -e "$D/$z" ] || ln -s "$n" "$D/$z"
  [ -f "$d/.pushed_z" ] && { echo "SKIP $z (already verified in the store)"; continue; }
  want=$(ls "$D/$z/all" | wc -l)
  for try in 1 2 3 4; do
    STS_PUSH_BATCH=3000 python3 research/tools/data_sync.py push "$z" 2>&1 | grep -E "^PUSH|Error|limit" | tail -2
    have=$(python3 research/tools/data_sync.py ls 2>/dev/null | awk -v n=$z '$1==n{print $2}')
    echo "$(date -u +%H:%M:%S) $z local $want store ${have:-0}"; [ "${have:-0}" = "$want" ] && break; sleep 600
  done
  [ "${have:-0}" = "$want" ] || { echo "PUSH SHARDS FAILED $z"; exit 1; }
  touch "$d/.pushed_z"
done
echo "ALL SHARD SETS PUSHED"
