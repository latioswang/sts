#!/usr/bin/env bash
# Phase F40 (b) calibration (measurement only, used seeds): mean search actions per game of best + early_stop=1 + sims=X on the
# first N seeds of the track's F39 slice, against the stored base rows (best at sims 500, no early stop) for the same seeds.
# usage: f40_calib.sh TRACK X [N=200]   -> prints the actions ratio (target <= 1.01, as close to 1 as the 25-sims grid allows)
set -euo pipefail
cd "$(dirname "$0")/../.."
TR=$1; X=$2; N=${3:-200}
case $TR in a20) ASC=20; S0=1345500; BASE=research/runs/f35a20.fs1345500n1000.a20.tsv ;; a10) ASC=10; S0=1346500; BASE=research/runs/f35a10.fs1346500n1000.a10.tsv ;;
  heart) ASC=0; S0=1347500; BASE=research/runs/f16heart.fs1347500n1000.a0.tsv ;; *) echo "bad track"; exit 1 ;; esac
P=$(sed 's/^/--param /' research/best_${TR}_params.txt | tr '\n' ' ')
OUT=$(mktemp)
/home/user/sts_lightspeed/build/fair_eval --agent fair --seed_start $S0 --count $N --asc $ASC --threads 4 $P --param early_stop=1 --param sims=$X > $OUT
python3 - "$TR" "$X" "$BASE" "$OUT" "$S0" "$N" <<'PY'
import sys
tr, x, base, new, s0, n = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], int(sys.argv[5]), int(sys.argv[6])
def rows(f):
    out = {}
    for line in open(f):
        p = line.split()
        if len(p) >= 12 and p[0].isdigit() and s0 <= int(p[0]) < s0 + n and p[9] != "2": out[int(p[0])] = float(p[11])
    return out
A, B = rows(base), rows(new); c = sorted(set(A) & set(B))
a = sum(A[s] for s in c) / len(c); b = sum(B[s] for s in c) / len(c)
print(f"F40 CALIB {tr} sims={x} early_stop=1: games={len(c)} actions base={a:.0f} new={b:.0f} ratio={b/a:.4f}")
PY
rm -f $OUT
