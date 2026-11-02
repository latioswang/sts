#!/usr/bin/env python3
"""Fit the deck-value model's 8 head weights to the real objective (run score = min(floor,51)/51 + win).

Uses A0 datasets NOT used to train model 1 (bg1b, bg1k3: 1,000 runs of the A0 agent), the model's predicted heads
at each logged decision's base state (deck as it is), and each run's final score from its games.tsv.
Reports run-grouped 5-fold cross-validated correlation of: the hand-set formula (bfValue), a fitted linear
combination, and each single head. usage: fit_bf_heads.py [model_id=1]
"""
import glob, os, sys
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MID = int(sys.argv[1]) if len(sys.argv) > 1 else 1
HEADS = ["boss_win", "boss_hp", "hall_win", "hall_hp", "boss_dmg", "next_win", "next_hp", "next_dmg"]
HAND = np.array([1.0, 0.5, 0.5, 0.25, 0.5, 0.5, 0.25, 1.0])

def load_model(n):
    tok = open(os.path.join(ROOT, "models", f"bf_g{n}.txt")).read().split()
    dim, h, o, trained = map(int, tok[:4]); v = np.array(tok[4:], dtype=np.float64); i = 0
    W1 = v[i:i + h * dim].reshape(h, dim); i += h * dim
    B1 = v[i:i + h]; i += h
    W2 = v[i:i + o * h].reshape(o, h); i += o * h
    B2 = v[i:i + o]
    return dim, lambda X: 1 / (1 + np.exp(-(np.maximum(X @ W1.T + B1, 0) @ W2.T + B2)))

dim, predict = load_model(MID)
X, runs = [], []
outcome = {}
for ds in ["bg1b", "bg1k3"]:
    for line in open(os.path.join(ROOT, "runs", f"{ds}.train.a0.tsv")):
        f = line.split("\t")
        if not f[0].isdigit(): continue
        seed = int(f[0]); agent = (seed * 0x9E3779B97F4A7C15 + 12345) % (1 << 64)
        outcome[agent % 10000000] = min(float(f[2]), 51) / 51 + float(f[1])
    for fn in glob.glob(os.path.join(ROOT, "vf", "data", ds, "chunk_*", "bg_*.bin")):
        a = np.fromfile(fn, dtype=np.float32)
        a = a[: (a.size // (dim + 12)) * (dim + 12)].reshape(-1, dim + 12)
        a = a[a[:, dim + 11] == 1.0]  # base rows only: the deck as it is at each logged decision
        if len(a): X.append(a[:, :dim]); runs.append(a[:, dim + 8].astype(np.int64))
X = np.concatenate(X); runs = np.concatenate(runs)
y = np.array([outcome.get(int(r), np.nan) for r in runs]); ok = np.isfinite(y)
X, runs, y = X[ok], runs[ok], y[ok]
H = predict(X.astype(np.float64))
print(f"model bf_g{MID}: {len(y)} base decisions from {len(np.unique(runs))} runs; run score mean {y.mean():.3f} sd {y.std():.3f}")

uniq = np.unique(runs); rng = np.random.RandomState(0); rng.shuffle(uniq)
fold = {r: i % 5 for i, r in enumerate(uniq)}; F = np.array([fold[r] for r in runs])
def cv_corr(feat_fn, fit):
    pred = np.zeros(len(y))
    for k in range(5):
        tr, te = F != k, F == k
        if fit:
            A = np.c_[H[tr], np.ones(tr.sum())]; w = np.linalg.lstsq(A, y[tr], rcond=None)[0]
            pred[te] = np.c_[H[te], np.ones(te.sum())] @ w
        else:
            pred[te] = feat_fn(H[te])
    return np.corrcoef(pred, y)[0, 1]
print(f"hand-set formula (bfValue)      CV corr with run score: {cv_corr(lambda h: h @ HAND, False):+.3f}")
print(f"fitted linear combination       CV corr with run score: {cv_corr(None, True):+.3f}")
for j, n in enumerate(HEADS): print(f"  single head {n:9s}            corr: {np.corrcoef(H[:, j], y)[0, 1]:+.3f}")
w = np.linalg.lstsq(np.c_[H, np.ones(len(y))], y, rcond=None)[0][:8]
print("fitted weights (all data), scaled so boss_win = 1:", " ".join(f"{n}={v / w[0]:+.2f}" for n, v in zip(HEADS, w)))
print("hand weights:                                    ", " ".join(f"{n}={v:+.2f}" for n, v in zip(HEADS, HAND)))
print("head correlation matrix (redundancy):"); C = np.corrcoef(H.T)
for j, n in enumerate(HEADS): print(f"  {n:9s} " + " ".join(f"{c:+.2f}" for c in C[j]))
