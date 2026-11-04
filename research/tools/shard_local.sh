#!/usr/bin/env bash
# Convert local run-data dirs (research/vf/data/rundata*/all, one file per run) to compressed shards in place:
# pack -> byte-verify -> swap (the shard dir becomes all/) -> delete the per-file copy and its download staging.
set -uo pipefail
cd "$(dirname "$0")/../.."
for d in research/vf/data/rundata*/; do
  n=$(basename "$d"); A=$d/all; S=$d/sh
  [ -f "$A/index.tsv" ] && { echo "SKIP $n (already shards)"; continue; }
  if [ ! -f "$S/index.tsv" ]; then python3 research/vf/runstore.py pack "$A" "$S" || { echo "PACK FAILED $n"; exit 1; }; fi
  python3 research/vf/runstore.py verify "$A" "$S" || { echo "VERIFY FAILED $n"; exit 1; }
  mv "$A" "$d/all.perfile" && mv "$S" "$A" && rm -rf "$d/all.perfile" "$d/_hf" && echo "SWAPPED $n -> shards ($(du -sh "$A" | cut -f1))"
done
df -h /home/user | tail -1
