#!/usr/bin/env python3
"""Score an exported run-value model (research/models/vr_<id>.txt, runtime format) on every run in a data dir, per state
(same metric as the trainers' holdout): RMSE and correlation with the final score. usage: score_vr.py MODEL_TXT DATA_DIR"""
import glob, os, sys
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "vf")); import runstore
DIM, EXTRA = 666, 4
tok = open(sys.argv[1]).read().split(); nl = int(tok[1]); p = 3; layers = []
for _ in range(nl):
    i, o = int(tok[p]), int(tok[p + 1]); p += 2
    W = np.array(tok[p:p + i * o], float).reshape(o, i); p += i * o; b = np.array(tok[p:p + o], float); p += o; layers.append((W, b))
fs = runstore.list_runs(sys.argv[2])
arr = np.concatenate([runstore.read(f) for f in fs]).reshape(-1, DIM + EXTRA)   # float32 rows; scored in chunks (memory)
y = np.minimum(arr[:, DIM + 1], 51).astype(np.float64) / 51 + arr[:, DIM]; pr = np.empty(len(arr))
for i in range(0, len(arr), 200000):
    a = arr[i:i + 200000, :DIM].astype(np.float64)
    for l, (W, b) in enumerate(layers):
        a = a @ W.T + b
        if l + 1 < len(layers): a = np.maximum(a, 0)
    pr[i:i + 200000] = a[:, 0]
print(f"SCORE {os.path.basename(sys.argv[1])} on {sys.argv[2]}: runs={len(fs)} states={len(y)} rmse={np.sqrt(((pr - y) ** 2).mean()):.4f} corr={np.corrcoef(pr, y)[0, 1]:.3f} state_mean={y.mean():.3f}")
