#!/usr/bin/env bash
# F3 A20 decision (pre-registered 2026-11-06 16:1x UTC): two near misses on the same base f2a20w (f3a20w 2.25x weights, f3a20p
# V-only) were each extended against f2a20w without promotion (ext_fresh.sh BASE=f2a20w NOPROMOTE=1). Among those whose
# extension qualified (keep-candidate: pooled P >= 0.9 and second-slice dscore > 0), promote the one with the larger pooled
# dscore; if none qualified, nothing changes.
set -euo pipefail
cd "$(dirname "$0")/../.."
best=""; bd=-9
for n in f3a20w f3a20p; do
  row=$(awk -F'\t' -v n="$n" '$10=="keep-candidate" && index($11, n": ")==1' research/results.tsv | tail -1)
  [ -z "$row" ] && { echo "$n: not a keep-candidate"; continue; }
  s1=$(echo "$row" | grep -o 'fs[0-9]*n[0-9]* + fs[0-9]*n[0-9]*' | head -1 | sed 's/ + /,/')
  d=$(python3 research/tools/pooled.py "$n" f2a20w 20 "$s1" act3 | sed -n 's/.*dscore=\([-+0-9.]*\).*/\1/p' | head -1)
  echo "$n: keep-candidate, pooled ($s1) dscore $d"
  python3 -c "import sys; sys.exit(0 if float('$d') > float('$bd') else 1)" && { bd=$d; best=$n; }
done
if [ -n "$best" ]; then TRACK=a20 research/promote.sh "$best"; echo "F3 A20 DECISION: promote $best (pooled dscore $bd)"; else echo "F3 A20 DECISION: none qualified; A20 best stays f2a20w"; fi
