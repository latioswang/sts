#!/usr/bin/env bash
# Phase F19 calibration (measurement only; pre-registered in program.md F19): the Heart fight's share of search actions.
# f16heart + heart_mult=3 (sims 500) on the first 200 seeds of the used slice fs1297000n1000, vs the stored f16heart rows
# of the same seeds. H = (A3 - A1) / 2 = mean Heart-fight actions per game at 1x; neutral base sims X = 500 * A1 / (A1 + 2H),
# rounded down to a multiple of 5; heart_mult M = 1500 / X (Heart fight at 1,500 sims). Writes research/runs/f19_calib.txt.
set -euo pipefail
cd "$(dirname "$0")/../.."
A=$(sed 's/^/--param /' research/best_heart_params.txt | tr '\n' ' ')
python3 -u research/eval/run.py f19c_hm3 fs1297000n200 $A --param keys=1 --param chest_pol=1 --param heart_mult=3 --asc 0 --winmode heart | sed -n 1p
python3 - <<'PY'
import csv
def rows(p):
    out={}
    for l in open(p):
        f=l.rstrip('\n').split('\t')
        if len(f)>=12 and f[9]!='2': out[int(f[0])]=f
    return out
b=rows('research/runs/f16heart.fs1297000n1000.a0.tsv'); c=rows('research/runs/f19c_hm3.fs1297000n200.a0.tsv')
s=sorted(set(b)&set(c)); A1=sum(float(b[k][11]) for k in s)/len(s); A3=sum(float(c[k][11]) for k in s)/len(s)
H=(A3-A1)/2; X=int(500*A1/(A1+2*H)//5*5); M=round(1500/X,4)
a4=[k for k in s if int(b[k][3])>=4]
print(f"F19 CALIB n={len(s)} A1={A1:.0f} A3={A3:.0f} heart_share={H/A1:.3f} -> sims={X} heart_mult={M} (act4 games {len(a4)})")
open('research/runs/f19_calib.txt','w').write(f"sims={X}\nheart_mult={M}\n")
PY
