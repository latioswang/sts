#!/usr/bin/env python3
"""Phase F35 held-out gate: path-choice models on an M=16 path set (fair_eval --branch_eval 12 16 --branch_kind 5).
For each decision, a model's pick = argmax over the legal next nodes of pm_<id>(row[:686]); scored on half B (meanB), against
the agent's own pick on half B, with a paired SE. Also: random@B, the honest oracle (pick on half A, score on half B), and the
map-DP value feature's own argmax (should equal own). Pre-registered arm rule: a track's best candidate is armed only if its
pick@B − own@B > +2 SE on that track's held-out set.
usage: f35_eval.py DIR [DIR ...] ID [ID ...]   (integer arguments are model ids)"""
import glob, os, sys
import numpy as np
Ds = [x for x in sys.argv[1:] if not x.isdigit()]; ids = [int(x) for x in sys.argv[1:] if x.isdigit()]
IN, W = 686, 694
decs = []
for f in sorted(f for D in Ds for f in glob.glob(os.path.join(D, "path_*.bin"))):
    a = np.fromfile(f, dtype=np.float32)
    if a.size == 0 or a.size % W: continue
    a = a.reshape(-1, W)
    if (a[:, IN + 6] < 0).any() or (a[:, IN + 7] < 0).any() or len(a) < 2 or not (a[:, IN + 5] > 0.5).any(): continue
    decs.append(a)
def load(i):
    tok = open(f"research/models/pm_{i}.txt").read().split(); md, nl = int(tok[0]), int(tok[1]); p = 3; L = []
    for _ in range(nl):
        ia, ob = int(tok[p]), int(tok[p + 1]); p += 2
        Wm = np.array(tok[p:p + ia * ob], float).reshape(ob, ia); p += ia * ob; bb = np.array(tok[p:p + ob], float); p += ob; L.append((Wm, bb))
    return L
def V(L, x):
    h = x[:, :IN].astype(np.float64)
    for l, (Wm, b) in enumerate(L):
        h = h @ Wm.T + b
        if l + 1 < len(L): h = np.maximum(h, 0)
    return h[:, 0]
n = len(decs)
print(f"F35 decisions={n} options={sum(len(d) for d in decs)} mean_options={np.mean([len(d) for d in decs]):.2f}")
if n == 0: sys.exit(0)
B = [d[:, IN + 7].astype(np.float64) for d in decs]; A = [d[:, IN + 6].astype(np.float64) for d in decs]
own = [int(np.argmax(d[:, IN + 5])) for d in decs]
ownB = np.array([b[o] for b, o in zip(B, own)])
def pr(name, v):
    dv = v - ownB
    print(f"F35 {name:14s} value@B={v.mean():.4f}  vs own@B: {dv.mean():+.4f} ± {dv.std(ddof=1) / np.sqrt(len(dv)):.4f} (paired SE, n={len(dv)})")
pr("own", ownB)
pr("random", np.array([b.mean() for b in B]))
pr("oracle_honest", np.array([b[int(np.argmax(a))] for a, b in zip(A, B)]))
dpk = [int(np.argmax(d[:, 685])) for d in decs]
print(f"F35 map-DP feature argmax == own in {np.mean([p == o for p, o in zip(dpk, own)]):.3f} of decisions")
for i in ids:
    L = load(i); pk = [int(np.argmax(V(L, d))) for d in decs]
    pr(f"pm_{i}", np.array([b[k] for b, k in zip(B, pk)]))
    print(f"F35 pm_{i} picks the agent's own option in {np.mean([p == o for p, o in zip(pk, own)]):.3f} of decisions")
