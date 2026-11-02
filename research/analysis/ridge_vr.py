#!/usr/bin/env python3
"""Diagnostic for the Phase R grid (informational, not a keep decision): ridge regression on standardised run
features with the SAME run split as train_vr.py (np.unique run ids, RandomState(12345) shuffle, last 1000 = holdout,
first R = train). Streams files and keeps only moments, so it is memory-light. lambda is chosen on the last 500
train runs (fit on the rest), then refit on all R. Two row weightings: every row equal (as train_vr) and every run
equal (rows weighted 1/rows-in-run). usage: ridge_vr.py DATA_DIR R [R ...]"""
import glob, os, sys
import numpy as np
DIM, EXTRA = 666, 4
files = sorted(glob.glob(os.path.join(sys.argv[1], "run_*.bin")))
rid = {}
for f in files:
    rid[f] = int(np.fromfile(f, dtype=np.float32, count=DIM + EXTRA)[DIM + 3])
runs = np.unique(np.array(list(rid.values()))); rng = np.random.RandomState(12345); rng.shuffle(runs)
hold = set(runs[-1000:].tolist()); byrun = {}
for f, r in rid.items(): byrun.setdefault(r, []).append(f)

def load(r):
    a = np.concatenate([np.fromfile(f, dtype=np.float32) for f in byrun[r]])
    a = a.reshape(-1, DIM + EXTRA).astype(np.float64)
    return a[:, :DIM], np.minimum(a[:, DIM + 1], 51) / 51 + a[:, DIM]

def moments(rs, per_run):
    S = np.zeros((DIM + 1, DIM + 1)); b = np.zeros(DIM + 1); n = 0.0; yy = 0.0
    for r in rs:
        X, y = load(r); w = np.full(len(y), 1.0 / len(y) if per_run else 1.0)
        Z = np.hstack([X, np.ones((len(y), 1))])
        S += (Z * w[:, None]).T @ Z; b += (Z * w[:, None]).T @ y; n += w.sum(); yy += (w * y * y).sum()
    return S, b, n

def fit(S, b, n, lam):
    mu = S[:DIM, DIM] / n; var = np.diag(S)[:DIM] / n - mu ** 2; sd = np.sqrt(np.maximum(var, 1e-12))
    # standardised design: z = (x - mu)/sd; solve in standardised space with penalty lam*n on non-intercept terms
    C = S[:DIM, :DIM] / n - np.outer(mu, mu); C = C / np.outer(sd, sd)
    cy = (b[:DIM] / n - mu * b[DIM] / n) / sd
    w = np.linalg.solve(C + lam * np.eye(DIM), cy)
    beta = w / sd; c = b[DIM] / n - beta @ mu
    return beta, c

def score(rs, beta, c, per_run):
    se = 0.0; n = 0.0; P = []; Y = []
    for r in rs:
        X, y = load(r); p = X @ beta + c; w = 1.0 / len(y) if per_run else 1.0
        se += w * ((p - y) ** 2).sum(); n += w * len(y); P.append(p); Y.append(y)
    P = np.concatenate(P); Y = np.concatenate(Y)
    return np.sqrt(se / n), np.corrcoef(P, Y)[0, 1]

hrs = [r for r in runs if r in hold]
for R in map(int, sys.argv[2:]):
    pool = runs[:-1000][:R]; A, B = pool[:-500], pool[-500:]
    for per_run in (False, True):
        SA, bA, nA = moments(A, per_run)
        best = None
        for lam in (1e-4, 1e-3, 1e-2, 1e-1, 1.0, 10.0):
            beta, c = fit(SA, bA, nA, lam); rm, _ = score(B, beta, c, per_run)
            if best is None or rm < best[0]: best = (rm, lam)
        SB, bB, nB = moments(B, per_run)
        beta, c = fit(SA + SB, bA + bB, nA + nB, best[1])
        rm, co = score(hrs, beta, c, False)            # holdout scored per row, like train_vr
        ymean = (bA[DIM] + bB[DIM]) / (nA + nB)
        base = score(hrs, np.zeros(DIM), ymean, False)[0]
        print(f"RIDGE runs={R} weighting={'per-run' if per_run else 'per-row'} lambda={best[1]:g} holdout_rmse={rm:.4f} holdout_corr={co:.3f} predict_mean_rmse={base:.4f}", flush=True)
