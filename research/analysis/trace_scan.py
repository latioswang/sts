#!/usr/bin/env python3
"""Replay audit: scan FAIR_TRACE game logs for suspicious combat patterns.
usage: trace_scan.py DIR [DIR ...]   prints counts and examples per pattern"""
import glob, os, re, sys
from collections import Counter, defaultdict
DEF = {"Defend", "Defend+", "Shrug It Off", "Shrug It Off+", "Flame Barrier", "Flame Barrier+", "True Grit", "True Grit+",
       "Ghostly Armor", "Ghostly Armor+", "Impervious", "Impervious+", "Power Through", "Power Through+", "Entrench", "Entrench+",
       "Sentinel", "Sentinel+", "Second Wind", "Second Wind+"}
pat = re.compile(r"^\s+t(\d+) hp (\d+)/(\d+) blk (\d+) e (\d+) \| hand: (.*?)((?: \| .*)?)$")
hits = defaultdict(list); fights = Counter()
for D in sys.argv[1:]:
    for f in sorted(glob.glob(os.path.join(D, "game_*.txt"))):
        L = open(f).read().split("\n"); fight = None
        for i, l in enumerate(L):
            if l.startswith("  FIGHT "): fight = l.split("|")[0][8:].strip(); fights[fight] += 1
            m = pat.match(l)
            if not m or i + 1 >= len(L) or "end turn" not in L[i + 1]: continue
            t, hp, mx, blk, e, hand, mons = int(m[1]), int(m[2]), int(m[3]), int(m[4]), int(m[5]), m[6], m[7]
            cards = re.findall(r"([A-Za-z' +-]+?)\((-?\d+)\)", hand)
            inc = 0
            for part in mons.split(" | ")[1:]:
                mm = re.search(r"-> \S+ (\d+)(?:x(\d+))?$", part.strip())
                if mm: inc += int(mm[1]) * int(mm[2] or 1)
            playable_def = [c for c, cost in cards if c.strip() in DEF and 0 <= int(cost) <= e]
            if inc > blk and playable_def:
                hits["end turn: unblocked damage coming, energy left, block card in hand unplayed"].append((os.path.basename(f), fight, t, f"hp {hp} blk {blk} e {e} incoming {inc} hand: {hand}"))
            playable = [c for c, cost in cards if 0 <= int(cost) <= e and c.strip() not in ("Ascender's Bane",)]
            if e >= 1 and playable and inc == 0:
                hits["end turn: energy and playable cards left, nothing incoming"].append((os.path.basename(f), fight, t, f"e {e} hand: {hand}"))
for k, v in hits.items():
    print(f"== {k}: {len(v)}")
    for x in v[:12]: print("   ", *x)
