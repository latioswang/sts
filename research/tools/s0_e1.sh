#!/usr/bin/env bash
# SOTA stage 0, E1 (pre-registered in research/breakthrough/PLAN.md rev 2 §3): our A20 strategy under four pilots on one
# common fresh slice. MEASUREMENT ONLY: diag_clair / cheat_combat arms see hidden information and are never agent results.
# usage: s0_e1.sh SPLIT ARM [ARM ...]   (ARM = a run name whose research/runs/ARM.params exists)
set -uo pipefail
cd "$(dirname "$0")/../.."
SPLIT=$1; shift
export FAIR_ALARM=${FAIR_ALARM:-7200}
pa() { sed 's/^/--param /' "research/runs/$1.params" | tr '\n' ' '; }
B=$(cat research/best_a20_name.txt)
for ARM in "$@"; do
  echo "== $(date -u +%H:%M) $ARM on $SPLIT"
  if [ "$ARM" = "$B" ]; then python3 -u research/eval/run.py "$ARM" "$SPLIT" $(pa "$ARM") --asc 20 --winmode act3 | sed -n 1,3p
  else python3 -u research/eval/run.py "$ARM" "$SPLIT" $(pa "$ARM") --base "$B" --asc 20 --winmode act3 | sed -n 1,4p; fi
done
echo "== $(date -u +%H:%M) done"
