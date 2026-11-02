#!/usr/bin/env python3
"""Evaluate Phase S leaf models on the Phase S2 leaf-state holdout (same split as train_sv.py --leaf):
does a root-trained net transfer to the states the search actually evaluates? usage: cross_eval_sv.py ID [ID...]"""
import glob, os, sys
import numpy as np
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); DIM = 204
def load_model(i):
    tok = open(os.path.join(ROOT, "models", f"sv_{i}.txt")).read().split(); dim, nl, _ = map(int, tok[:3]); p = 3; L = []
    for _ in range(nl):
        a, b = int(tok[p]), int(tok[p + 1]); p += 2
        W = np.array(tok[p:p + a * b], dtype=np.float64).reshape(b, a); p += a * b
        B = np.array(tok[p:p + b], dtype=np.float64); p += b; L.append((W, B))
    def f(X):
        h = X
        for k, (W, B) in enumerate(L):
            h = h @ W.T + B
            if k + 1 < len(L): h = np.maximum(h, 0)
        return h[:, 0]
    return f
files = sorted(glob.glob(os.path.join(ROOT, "vf", "data", "leafdata", "all", "leaf_*.bin")))
arr = np.concatenate([np.fromfile(f, dtype=np.float32) for f in files]); arr = arr[: (arr.size // (DIM + 2)) * (DIM + 2)].reshape(-1, DIM + 2)
X, y = arr[:, :DIM].astype(np.float64), arr[:, DIM].astype(np.float64)
ok = np.isfinite(X).all(1) & np.isfinite(y) & (y != 0); X, y = X[ok], y[ok]
perm = np.random.RandomState(12345).permutation(len(y)); nv = min(max(2000, min(len(y) // 10, 50000)), len(y) // 5); hold = perm[-nv:]
print(f"leaf holdout: {len(hold)} states; predict-mean RMSE {np.sqrt(((y[hold] - y[hold].mean()) ** 2).mean()):.2f}")
for i in sys.argv[1:]:
    p = load_model(i)(X[hold]); print(f"sv_{i}: RMSE on leaf states {np.sqrt(((p - y[hold]) ** 2).mean()):.2f}  bias {np.mean(p - y[hold]):+.2f}  corr {np.corrcoef(p, y[hold])[0, 1]:.3f}")
