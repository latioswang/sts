#!/usr/bin/env python3
"""Pooled paired comparison over several splits (pre-registered extension of a near-miss).
usage: pooled.py NAME BASE ASC split[,split...]   e.g. pooled.py potsave wea1p1 0 val,dev
NAME and BASE must have run files for every split (research/runs/NAME.split.aASC.tsv; the dev runs
are named NAME_dev when produced by the queue, so both spellings are tried)."""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "eval"))
from run import load, ok, paired, summary, ROOT  # noqa: E402

name, base, asc, splits = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4].split(",")

def rows(n, split):
    for cand in (f"{n}.{split}.a{asc}.tsv", f"{n}_{split}.{split}.a{asc}.tsv"):
        p = os.path.join(ROOT, "runs", cand)
        if os.path.exists(p):
            return ok(load(p))
    raise SystemExit(f"missing run file for {n} on {split}")

A, B = {}, {}
for sp in splits:
    A.update(rows(name, sp)); B.update(rows(base, sp))
n, d, dw, p = paired(A, B)
na, wa, fa, sa, _ = summary(A); nb, wb, fb, sb, _ = summary(B)
print(f"pooled {','.join(splits)}: n={n}  {name} win={wa:.3f} floor={fa:.2f}  {base} win={wb:.3f} floor={fb:.2f}  dscore={d:+.4f} dwin={dw:+.3f} P(improve)={p:.3f}")
print("KEEP" if d > 0 and p >= 0.9 else "no")
