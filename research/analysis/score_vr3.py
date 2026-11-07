#!/usr/bin/env python3
"""Phase F14: score_vr.py for mixed widths. The data dir's row width is 670 iff it holds a DIM670 marker file (else 666);
the model's input width is its header dim. A 666 model on 670 rows uses the 666-feature prefix (the v1 layout); a 670
model on 666 rows gets 4 zero key features. VR_WINMODE=heart as in score_vr.py. usage: score_vr3.py MODEL_TXT DATA_DIR"""
import os, sys
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "vf")); import runstore
EXTRA = 4
tok = open(sys.argv[1]).read().split(); MD = int(tok[0]); nl = int(tok[1]); p = 3; layers = []
for _ in range(nl):
    i, o = int(tok[p]), int(tok[p + 1]); p += 2
    W = np.array(tok[p:p + i * o], float).reshape(o, i); p += i * o; b = np.array(tok[p:p + o], float); p += o; layers.append((W, b))
W0 = 670 if os.path.exists(os.path.join(sys.argv[2], "DIM670")) else 666
fs = runstore.list_runs(sys.argv[2])
arr = np.concatenate([runstore.read(f) for f in fs]); arr = arr[: (arr.size // (W0 + EXTRA)) * (W0 + EXTRA)].reshape(-1, W0 + EXTRA)
heart = os.environ.get("VR_WINMODE", "act3") == "heart"
y = np.minimum(arr[:, W0 + 1], 51).astype(np.float64) / 51 + arr[:, W0] * ((arr[:, W0 + 2] >= 4) if heart else 1.0); pr = np.empty(len(arr))
for i in range(0, len(arr), 200000):
    x = arr[i:i + 200000, :W0].astype(np.float64)
    if MD < W0: x = x[:, :MD]
    elif MD > W0: x = np.hstack([x, np.zeros((len(x), MD - W0))])
    a = x
    for l, (W, b) in enumerate(layers):
        a = a @ W.T + b
        if l + 1 < len(layers): a = np.maximum(a, 0)
    pr[i:i + 200000] = a[:, 0]
print(f"SCORE {os.path.basename(sys.argv[1])} on {sys.argv[2]}: runs={len(fs)} states={len(y)} rmse={np.sqrt(((pr - y) ** 2).mean()):.4f} corr={np.corrcoef(pr, y)[0, 1]:.3f} state_mean={y.mean():.3f}")
