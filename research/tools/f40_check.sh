#!/usr/bin/env bash
# Phase F40 (a): early_stop=1 must leave every game unchanged (same rows except time and search actions) and saves search
# actions. Runs each track's best with and without early_stop=1 on N used seeds (the F39 slices) with one binary.
# usage: f40_check.sh N [BIN]   prints per track: rows compared, rows differing, mean actions base / es and the ratio
set -euo pipefail
cd "$(dirname "$0")/../.."
N=${1:-16}; BIN=${2:-/home/user/sts_lightspeed/build/fair_eval}
T=$(mktemp -d)
for spec in "a20 20 1345500" "a10 10 1346500" "heart 0 1347500"; do
  set -- $spec; TR=$1; ASC=$2; S0=$3
  PF=research/best_${TR}_params.txt; P=$(sed 's/^/--param /' $PF | tr '\n' ' ')
  "$BIN" --agent fair --seed_start $S0 --count $N --asc $ASC --threads 4 $P > $T/$TR.base
  "$BIN" --agent fair --seed_start $S0 --count $N --asc $ASC --threads 4 $P --param early_stop=1 > $T/$TR.es
  python3 - "$TR" "$T/$TR.base" "$T/$TR.es" <<'PY'
import sys
tr, a, b = sys.argv[1:]
def rows(f):
    out = {}
    for line in open(f):
        p = line.split()
        if len(p) >= 12 and p[0].isdigit(): out[p[0]] = p
    return out
A, B = rows(a), rows(b)
common = sorted(set(A) & set(B))
diff = [s for s in common if A[s][:10] + A[s][12:] != B[s][:10] + B[s][12:]]
aa = sum(float(A[s][11]) for s in common) / len(common); bb = sum(float(B[s][11]) for s in common) / len(common)
print(f"F40 CHECK {tr}: rows={len(common)} differing={len(diff)} actions base={aa:.0f} es={bb:.0f} ratio={bb/aa:.4f}" + (f" first_diff={diff[0]}" if diff else ""))
PY
done
rm -rf $T
