#!/usr/bin/env python3
"""Phase F26: decision-level evaluation of run-value models from branch playouts (eval_main --branch_eval).
Each branch_<seed>.bin holds one decision: per option a row of rv::DIM (666) features + [opt, n, mean_score, n_ok, floor, own].
For each model, its pick = argmax V(option state); decision value = the playout mean of the picked option. Reports the mean over
decisions, and paired differences vs the reference model (default vr_171) with a paired SE. Also: the agent's own pick, a
uniform-random pick (mean over options) and the oracle (max option mean; biased upward by playout noise).
usage: f26_branch.py DIR MODEL_ID [MODEL_ID ...]   (first id = reference)"""
import glob, os, sys
import numpy as np
D, ids = sys.argv[1], [int(x) for x in sys.argv[2:]]
W = 666 + 6
decs = []
for f in sorted(glob.glob(os.path.join(D, "branch_*.bin"))):
    a = np.fromfile(f, dtype=np.float32)
    if a.size == 0 or a.size % W: continue
    a = a.reshape(-1, W)
    if (a[:, 668] < 0).any() or (a[:, 669] < 1).any(): continue   # an option with no finished playout
    decs.append(a)
def load(i):
    tok = open(f"research/models/vr_{i}.txt").read().split(); md, nl = int(tok[0]), int(tok[1]); p = 3; L = []
    for _ in range(nl):
        a, b = int(tok[p]), int(tok[p + 1]); p += 2
        Wm = np.array(tok[p:p + a * b], float).reshape(b, a); p += a * b; bb = np.array(tok[p:p + b], float); p += b; L.append((Wm, bb))
    return md, L
def V(model, x):
    md, L = model
    h = x[:, :666].astype(np.float64)
    if md > 666: h = np.hstack([h, np.zeros((len(h), md - 666))])
    for l, (Wm, b) in enumerate(L):
        h = h @ Wm.T + b
        if l + 1 < len(L): h = np.maximum(h, 0)
    return h[:, 0]
print(f"F26 decisions={len(decs)} options={sum(len(d) for d in decs)} mean_options={np.mean([len(d) for d in decs]):.2f} "
      f"floors {np.percentile([d[0,670] for d in decs],[10,50,90]).round(0).tolist()}")
means = [d[:, 668].astype(np.float64) for d in decs]
base = {"random": np.array([m.mean() for m in means]), "oracle": np.array([m.max() for m in means]),
        "own": np.array([m[d[:, 671] > 0.5][0] if (d[:, 671] > 0.5).any() else np.nan for m, d in zip(means, decs)])}
res = {}
for i in ids:
    mdl = load(i); res[f"vr_{i}"] = np.array([m[np.argmax(V(mdl, d))] for m, d in zip(means, decs)])
ref = f"vr_{ids[0]}"
for k, v in list(base.items()) + list(res.items()):
    ok = ~np.isnan(v); dv = v[ok] - res[ref][ok]
    agree = "" if k in ("random", "oracle") else ""
    print(f"F26 {k:8s} value={np.nanmean(v):.4f}  vs {ref}: {dv.mean():+.4f} ± {dv.std(ddof=1) / np.sqrt(ok.sum()):.4f} (paired SE, n={ok.sum()})")
for i in ids:
    mdl = load(i)
    same_own = np.mean([np.argmax(V(mdl, d)) == int(np.argmax(d[:, 671])) for d in decs if (d[:, 671] > 0.5).any()])
    print(f"F26 vr_{i} picks the agent's own option in {same_own:.3f} of decisions")
