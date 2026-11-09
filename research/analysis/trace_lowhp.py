#!/usr/bin/env python3
"""Replay audit: map choices made at low HP. For each MAP step with HP below a threshold, what was offered and what was
taken, split by the game's result. usage: trace_lowhp.py DIR [threshold=0.4]"""
import glob, os, re, sys
from collections import Counter
D = sys.argv[1]; TH = float(sys.argv[2]) if len(sys.argv) > 2 else 0.4
stats = {True: Counter(), False: Counter()}; ex = []
for f in sorted(glob.glob(os.path.join(D, "game_*.txt"))):
    L = open(f).read().split("\n"); win = any(l.startswith("RESULT WIN") for l in L)
    for i, l in enumerate(L):
        m = re.match(r"  \[MAP\] F(\d+) A(\d+) hp (\d+)/(\d+) gold (\d+) offers:(.*)", l)
        if not m or i + 1 >= len(L): continue
        hp, mx = int(m[3]), int(m[4])
        if hp / mx >= TH: continue
        opts = re.findall(r"x\d+:(\w+)", m[6]); took = re.search(r"-> room (\w+)", L[i + 1])
        if len(set(opts)) < 2 or not took: continue
        took = took[1]; s = stats[win]; s["decisions"] += 1
        if "REST" in opts:
            s["rest offered"] += 1
            if took == "REST": s["took rest"] += 1
            elif took in ("MONSTER", "ELITE"): s["took a fight over a rest"] += 1; ex.append((os.path.basename(f), int(m[1]), f"{hp}/{mx}", opts, took, win))
        if took == "ELITE": s["took an elite at low HP"] += 1
        if took == "MONSTER" and any(o in ("EVENT", "SHOP", "TREASURE", "REST") for o in opts): s["took a fight over a non-fight room"] += 1
for w in (False, True):
    print(("WINS" if w else "LOSSES"), dict(stats[w]))
print(f"\nexamples (fight chosen over a rest, HP < {TH:.0%}):")
for x in ex[:15]: print("  ", *x)
