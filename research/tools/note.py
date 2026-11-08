#!/usr/bin/env python3
"""Print ready-to-paste result fragments from results.tsv, so notes never contain typed numbers.
usage: note.py NAME [NAME...]   ->  `NAME` 27.0% win, floor 35.4, +0.065 vs base, P=0.981 → keep
"""
import sys, os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
rows = {}
import glob
files = [os.path.join(ROOT, "results.tsv")] + sorted(glob.glob(os.path.join(ROOT, "results.d", "*.tsv")))
for line in (l for p in files for l in open(p)):
    f = line.rstrip("\n").split("\t")
    if len(f) < 11 or not f[2].isdigit():
        continue
    name = f[10].split(":", 1)[0]
    rows[name] = f  # last occurrence wins
for name in sys.argv[1:]:
    f = rows.get(name)
    if not f:
        print(f"`{name}`: not in results.tsv"); continue
    win = float(f[4]) * 100; floor = float(f[5]); d = f[7]; p = f[8]; status = f[9]
    cmp_ = f" {float(d):+.3f} vs base, P={float(p):.3f}" if d and p else ""
    print(f"`{name}` {win:.1f}% win, floor {floor:.1f},{cmp_} → {status}")
