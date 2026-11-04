"""Compares fair_eval runs on two simulators, paired by seed.

Usage: compare_eval.py <folder with base.a0.tsv, fork.a0.tsv, base.a20.tsv and fork.a20.tsv>
"""
import math, os, statistics as st, sys
d = sys.argv[1]
def load(name):
    rows = {}
    for line in open(os.path.join(d, name)):
        p = line.split()
        if len(p) >= 11 and p[0].isdigit():
            rows[int(p[0])] = {"win": int(p[1]), "floor": int(p[2]), "act": int(p[3]), "hp": int(p[4]), "aborted": int(p[9]), "secs": float(p[10])}
    return rows
for a in (0, 20):
    b, f = load(f"base.a{a}.tsv"), load(f"fork.a{a}.tsv")
    common = sorted(set(b) & set(f))
    bw = sum(b[s]["win"] for s in common)
    fw = sum(f[s]["win"] for s in common)
    diff = [f[s]["win"] - b[s]["win"] for s in common]
    se = st.pstdev(diff) / math.sqrt(len(diff))
    print(f"A{a}: {len(common)} seeds | base wins {bw} ({bw/len(common):.1%}) floor {st.mean(b[s]['floor'] for s in common):.1f} | fork wins {fw} ({fw/len(common):.1%}) floor {st.mean(f[s]['floor'] for s in common):.1f} | paired {(fw-bw)/len(common):+.1%} +- {1.96*se:.1%} | same floor {sum(b[s]['floor']==f[s]['floor'] for s in common)} | aborted {sum(b[s]['aborted'] for s in common)}/{sum(f[s]['aborted'] for s in common)} | secs {sum(b[s]['secs'] for s in common):.0f}/{sum(f[s]['secs'] for s in common):.0f}")
