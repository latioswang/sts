#!/usr/bin/env bash
# Phase F22 calibration (measurement only; pre-registered in program.md F22): boss fights' share of A20 search actions.
# f7a20 + boss_mult=2 (sims 500) on the first 500 seeds of the used slice fs1301000n1000 vs the stored f7a20 rows of the same
# seeds. B = A2 - A1 (approximate: trajectories after the first boss differ); neutral base sims X = 500 * A1 / (A1 + B),
# rounded down to a multiple of 5; boss_mult M = 1000 / X (boss fights at 1,000 sims). Writes research/runs/f22_calib.txt.
set -euo pipefail
cd "$(dirname "$0")/../.."
A=$(sed 's/^/--param /' research/best_a20_params.txt | tr '\n' ' ')
python3 -u research/eval/run.py f22c_bm2 fs1301000n500 $A --param boss_mult=2 --asc 20 --winmode act3 | sed -n 1p
python3 - <<'PY'
def rows(p):
    out={}
    for l in open(p):
        f=l.rstrip('\n').split('\t')
        if len(f)>=12 and f[9]!='2': out[int(f[0])]=f
    return out
b=rows('research/runs/f7a20.fs1301000n1000.a20.tsv'); c=rows('research/runs/f22c_bm2.fs1301000n500.a20.tsv')
s=sorted(set(b)&set(c)); A1=sum(float(b[k][11]) for k in s)/len(s); A2=sum(float(c[k][11]) for k in s)/len(s)
B=A2-A1; X=int(500*A1/(A1+B)//5*5); M=round(1000/X,4)
print(f"F22 CALIB n={len(s)} A1={A1:.0f} A2={A2:.0f} boss_share~={B/A1:.3f} -> sims={X} boss_mult={M}")
open('research/runs/f22_calib.txt','w').write(f"sims={X}\nboss_mult={M}\n")
PY
