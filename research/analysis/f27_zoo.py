#!/usr/bin/env python3
"""Phase F27 (pre-registered in program.md F27): offline selection of a run-value model by the F26 decision-level metric,
with a split. Decisions from even seeds = selection half A, odd seeds = validation half B. Candidates: every
research/models/vr_<id>.txt that loads (input dim 666 or 670, last layer width 1) as a single model, plus the pairwise means
of the top-5 singles on A. Winner = highest mean picked-option value on A. Reports, for the winner and the top-5, the
value on A and the paired difference vs vr_171 on B (SE over B decisions).
usage: f27_zoo.py BRANCH_DIR"""
import glob, os, re, sys
import numpy as np
D = sys.argv[1]; W = 666 + 6
decs = []
for f in sorted(glob.glob(os.path.join(D, "branch_*.bin"))):
    a = np.fromfile(f, dtype=np.float32)
    if a.size == 0 or a.size % W: continue
    a = a.reshape(-1, W)
    if (a[:, 668] < 0).any() or (a[:, 669] < 1).any(): continue
    decs.append((int(re.search(r"branch_(\d+)", f).group(1)), a))
def load(path):
    tok = open(path).read().split()
    md, nl, tr = int(tok[0]), int(tok[1]), int(tok[2]); p = 3; L = []
    if md not in (666, 670) or tr != 1: return None
    for _ in range(nl):
        a, b = int(tok[p]), int(tok[p + 1]); p += 2
        Wm = np.array(tok[p:p + a * b], float).reshape(b, a); p += a * b; bb = np.array(tok[p:p + b], float); p += b; L.append((Wm, bb))
    if L[0][0].shape[1] != md or L[-1][0].shape[0] != 1: return None
    return md, L
def V(m, x):
    md, L = m; h = x[:, :666].astype(np.float64)
    if md > 666: h = np.hstack([h, np.zeros((len(h), md - 666))])
    for l, (Wm, b) in enumerate(L):
        h = h @ Wm.T + b
        if l + 1 < len(L): h = np.maximum(h, 0)
    return h[:, 0]
X = [d for _, d in decs]; means = [d[:, 668].astype(np.float64) for d in X]
isA = np.array([s % 2 == 0 for s, _ in decs])
models = {}
for p in glob.glob("research/models/vr_*.txt"):
    i = int(re.search(r"vr_(\d+)\.txt", p).group(1))
    try:
        m = load(p)
    except Exception:
        m = None
    if m: models[i] = m
vals = {i: [V(m, d) for d in X] for i, m in models.items()}           # per-decision V of each option
def picked(vlist): return np.array([mm[np.argmax(v)] for mm, v in zip(means, vlist)])
single = {i: picked(v) for i, v in vals.items()}
top5 = sorted(single, key=lambda i: -single[i][isA].mean())[:5]
cands = {f"vr_{i}": single[i] for i in single}
for a_ in range(len(top5)):
    for b_ in range(a_ + 1, len(top5)):
        i, j = top5[a_], top5[b_]
        cands[f"mean(vr_{i},vr_{j})"] = picked([(x + y) / 2 for x, y in zip(vals[i], vals[j])])
ref = single[171]
print(f"F27 decisions={len(X)} A={isA.sum()} B={(~isA).sum()} single models={len(single)} candidates={len(cands)}")
rank = sorted(cands, key=lambda k: -cands[k][isA].mean())
for k in rank[:8] + (["vr_171"] if "vr_171" not in rank[:8] else []):
    dB = cands[k][~isA] - ref[~isA]
    print(f"F27 {k:22s} A={cands[k][isA].mean():.4f}  B={cands[k][~isA].mean():.4f}  B-vs-vr_171 {dB.mean():+.4f} ± {dB.std(ddof=1)/np.sqrt(len(dB)):.4f}")
w = rank[0]; dB = cands[w][~isA] - ref[~isA]
print(f"F27 WINNER {w}: B-vs-vr_171 {dB.mean():+.4f} ± {dB.std(ddof=1)/np.sqrt(len(dB)):.4f}")
