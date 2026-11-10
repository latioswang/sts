#!/usr/bin/env bash
# Phase F44 (a) calibration (vs the F43 bests) (measurement only, used seeds): mean search actions per game of best + early_stop=1 es_frac=F sims=X
# on the first N seeds of the track's F43 slice, against the stored rows of the current best on the same seeds.
# usage: f43_calib.sh TRACK F X [N=200]   -> prints the actions ratio (target <= 1.01, closest to 1 on the 25-sims grid)
set -euo pipefail
cd "$(dirname "$0")/../.."
TR=$1; F=$2; X=$3; N=${4:-200}
case $TR in a0) ASC=0; S0=1380500; BASE=research/runs/f6a0_upg.fs1380500n2000.a0.tsv ;; a20) ASC=20; S0=1388500; BASE=research/runs/f43a20_sf2.fs1388500n2000.a20.tsv ;;
  a10) ASC=10; S0=1382500; BASE=research/runs/f43a10_sf.fs1382500n2000.a10.tsv ;; heart) ASC=0; S0=1378500; BASE=research/runs/f16heart.fs1378500n2000.a0.tsv ;; *) echo "bad track"; exit 1 ;; esac
PF=research/best_${TR}_params.txt; [ $TR = a0 ] && PF=research/best_params.txt
P=$(grep -v -E '^(sims|early_stop|es_frac)=' $PF | sed "s/^/--param /" | tr '\n' ' ')
OUT=$(mktemp)
/home/user/sts_lightspeed/build/fair_eval --agent fair --seed_start $S0 --count $N --asc $ASC --threads 4 $P --param early_stop=1 --param es_frac=$F --param sims=$X > $OUT
python3 - "$TR" "$F" "$X" "$BASE" "$OUT" "$S0" "$N" <<'PY'
import sys
tr, f, x, base, new, s0, n = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5], int(sys.argv[6]), int(sys.argv[7])
def rows(fn):
    out = {}
    for line in open(fn):
        p = line.split()
        if len(p) >= 12 and p[0].isdigit() and s0 <= int(p[0]) < s0 + n and p[9] != "2": out[int(p[0])] = (float(p[11]), int(p[1]))
    return out
A, B = rows(base), rows(new); c = sorted(set(A) & set(B))
a = sum(A[s][0] for s in c) / len(c); b = sum(B[s][0] for s in c) / len(c)
wa = sum(A[s][1] for s in c) / len(c); wb = sum(B[s][1] for s in c) / len(c)
print(f"F44 CALIB {tr} es_frac={f} sims={x}: games={len(c)} actions base={a:.0f} new={b:.0f} ratio={b/a:.4f} (wins {wa:.3f} -> {wb:.3f}, used seeds, info only)")
PY
rm -f $OUT
