#!/usr/bin/env python3
"""Phase R trainer v2 (pre-registered 2026-11-02 16:07 UTC after the v1 grid's first four cells all lost to predicting
the mean). Fixes the v1 failure: v1 fed every logged state (~160 per run, all sharing one label) every epoch on raw
features and picked the epoch on the holdout itself, so a net could memorise run -> score from epoch 1.
v2: same run split as train_vr.py (np.unique run ids, RandomState(12345) shuffle, last 1000 runs = holdout, first R =
pool); the last 10% of the pool (>= 200 runs) is a validation split for early stopping / lambda; inputs standardised
with train-split moments (folded into the exported first layer, so research/vf/vr_mlp.h is unchanged); each epoch
samples K rows per run; early stopping on validation RMSE (patience 6, <= 80 epochs); the holdout is scored once.
--ridge: closed-form ridge on standardised features (lambda from {1e-3..10} on validation), exported as one linear layer.
usage: train_vr2.py DATA_DIR --id N --runs R (--hidden H [--layers 2] [--k 16] | --ridge)
meta: rmse corr train_runs hidden layers params predict_mean_rmse"""
import argparse, glob, os, time
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
DIM, EXTRA = 666, 4


def export(a, layers, rmse, corr, ntrain, hidden, nlay, nparams, base):
    out = os.path.join(ROOT, "models", f"vr_{a.id}.txt")
    with open(out, "w") as f:
        f.write(f"{DIM} {len(layers)} 1\n")
        for W, b in layers:
            f.write(f"{W.shape[1]} {W.shape[0]}\n" + " ".join(f"{v:.6g}" for v in W.ravel()) + "\n" + " ".join(f"{v:.6g}" for v in b) + "\n")
    open(os.path.join(ROOT, "models", f"vr_{a.id}.meta"), "w").write(f"{rmse:.6f} {corr:.6f} {ntrain} {hidden} {nlay} {nparams} {base:.6f}\n")
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data_dir"); ap.add_argument("--id", type=int, required=True); ap.add_argument("--runs", type=int, required=True)
    ap.add_argument("--hidden", type=int, default=0); ap.add_argument("--layers", type=int, default=2); ap.add_argument("--k", type=int, default=16)
    ap.add_argument("--ridge", action="store_true"); ap.add_argument("--holdout", type=int, default=1000)
    ap.add_argument("--state-weights", action="store_true",
                    help="R2 fix: weight training states so the objective matches the per-state holdout (states of a run\n                    sampled k of n count n/k each; without it, per-run sampling under-weights long, winning runs by ~0.125)")
    ap.add_argument("--patience", type=int, default=6, help="early-stopping patience in epochs (default 6)")
    ap.add_argument("--rows-per-run", type=int, default=0,
                    help="R3: keep at most this many states per training run at load (deterministic, RandomState(run id)); memory")
    ap.add_argument("--train-dirs", nargs="*", default=None,
                    help="Phase R2: training runs come from these (subsampled) dirs; DATA_DIR supplies only the holdout")
    a = ap.parse_args()
    files = sorted(glob.glob(os.path.join(a.data_dir, "run_*.bin")))
    if a.train_dirs:
        # holdout = the same 1,000 runs of DATA_DIR as without the flag (np.unique ids, RandomState(12345) shuffle);
        # training pool = DATA_DIR's other runs first (same order as before), then every run of the train dirs in
        # RandomState(23456) order; only holdout files of DATA_DIR and the train dirs' files are loaded.
        def run_ids(fs, d):   # cached file -> run id map (a scan of ~2e5 files takes ~25 min on this disk); same values
            cp = os.path.join(d, "ids_cache.tsv"); m = {}
            if os.path.exists(cp):
                m = {os.path.join(d, a): int(b) for a, b in (l.split("\t") for l in open(cp))}
            if not all(f in m for f in fs):
                m = {f: int(np.fromfile(f, dtype=np.float32, count=DIM + EXTRA)[DIM + 3]) for f in fs}
                try:
                    open(cp + ".tmp", "w").write("".join(f"{os.path.basename(f)}\t{r}\n" for f, r in m.items())); os.replace(cp + ".tmp", cp)
                except OSError:
                    pass
            return m
        ids = run_ids(files, a.data_dir)
        runs0 = np.unique(np.array(list(ids.values()))); rng = np.random.RandomState(12345); rng.shuffle(runs0)
        hold = runs0[-a.holdout:]; hs = set(hold.tolist())
        tfiles = []
        for d in a.train_dirs:
            fs = sorted(glob.glob(os.path.join(d, "run_*.bin"))); m = run_ids(fs, d)
            tfiles += [f for f in fs if m[f] not in hs]   # holdout runs only from DATA_DIR
        tfiles.sort()
        sel = [f for f in files if ids[f] in hs] + tfiles      # preallocate: np.concatenate of a list peaks at 2x (≈ 17 GB at 10^5 runs)
        R = DIM + EXTRA; kmax = a.rows_per_run
        tset = set(tfiles)   # built once (it was rebuilt per file: O(n^2), a 40-min hang at 2e5 files)
        tot = sum((os.path.getsize(f) // 4) if (kmax <= 0 or f not in tset) else min(os.path.getsize(f) // 4, kmax * R) for f in sel)
        arr = np.empty(tot, dtype=np.float32); o = 0
        for f in sel:
            v = np.fromfile(f, dtype=np.float32)
            if kmax > 0 and f in tset and v.size > kmax * R:
                m = v.reshape(-1, R); rid = int(m[0, DIM + 3])
                v = m[np.sort(np.random.RandomState(rid % (2 ** 32) + 1).choice(len(m), kmax, replace=False))].ravel()
            arr[o:o + v.size] = v; o += v.size
        arr = arr[:o]
        arr = arr[: (arr.size // (DIM + EXTRA)) * (DIM + EXTRA)].reshape(-1, DIM + EXTRA)
        X = arr[:, :DIM]; run = arr[:, DIM + 3].astype(np.int64)
        y = (np.minimum(arr[:, DIM + 1], 51) / 51 + arr[:, DIM]).astype(np.float32)
        have = set(np.unique(run).tolist()) - hs
        first = [r for r in runs0[:-a.holdout].tolist() if r in have]; fs = set(first)
        rest = np.array(sorted(have - fs), dtype=np.int64); np.random.RandomState(23456).shuffle(rest)
        runs = np.concatenate([np.array(first, dtype=np.int64), rest, hold]); pool = runs[: len(runs) - a.holdout][: a.runs]
    else:
        arr = np.concatenate([np.fromfile(f, dtype=np.float32) for f in files])
        arr = arr[: (arr.size // (DIM + EXTRA)) * (DIM + EXTRA)].reshape(-1, DIM + EXTRA)
        X = arr[:, :DIM]; run = arr[:, DIM + 3].astype(np.int64)
        y = (np.minimum(arr[:, DIM + 1], 51) / 51 + arr[:, DIM]).astype(np.float32)
        runs = np.unique(run); rng = np.random.RandomState(12345); rng.shuffle(runs)
        hold = runs[-a.holdout:]; pool = runs[:-a.holdout][: a.runs]
    nval = max(200, len(pool) // 10); val, trn = pool[-nval:], pool[:-nval]
    nrun = {}                                   # original states per run (counts.tsv of subsampled dirs; else rows present)
    for d in (a.train_dirs or []):
        cp = os.path.join(d, "counts.tsv")
        if os.path.exists(cp):
            for line in open(cp):
                r_, n_ = line.split(); nrun[int(r_)] = int(n_)
        elif a.state_weights:
            raise SystemExit(f"--state-weights needs {cp} (re-run subsample_runs.py)")
    order = np.argsort(run, kind="stable"); rs = run[order]
    def rows(ids):
        ids = np.sort(ids); lo = np.searchsorted(rs, ids, "left"); hi = np.searchsorted(rs, ids, "right")
        return [order[l:h] for l, h in zip(lo, hi)]
    Rt, Rv, Rh = rows(trn), rows(val), rows(hold)
    it = np.concatenate(Rt); iv = np.concatenate(Rv); ih = np.concatenate(Rh)
    wrow = np.ones(len(run), dtype=np.float64)    # per-row weight: n_original / n_present (1 for full runs)
    if a.state_weights:
        for rr in Rt + Rv:
            n0 = nrun.get(int(run[rr[0]]), len(rr)); wrow[rr] = n0 / len(rr)
    s1 = np.zeros(DIM); s2 = np.zeros(DIM); W = 0.0   # chunked (weighted) moments: X[it].std(dtype=float64) OOMs at 9,000 runs
    for i in range(0, len(it), 200000):
        c = X[it[i:i + 200000]].astype(np.float64); w = wrow[it[i:i + 200000]][:, None]
        s1 += (w * c).sum(0); s2 += (w * c * c).sum(0); W += float(w.sum())
    mu = s1 / W; sd = np.sqrt(np.maximum(s2 / W - mu ** 2, 0.0)); sd[sd < 1e-6] = 1.0
    ymean = float(np.average(y[it], weights=wrow[it])); ysd = float(np.sqrt(np.average((y[it] - ymean) ** 2, weights=wrow[it])) + 1e-6)
    base = float(np.sqrt(((y[ih] - ymean) ** 2).mean()))
    print(f"runs total={len(runs)} train={len(trn)} val={len(val)} holdout={len(hold)}; rows train={len(it)} val={len(iv)} holdout={len(ih)}", flush=True)
    def std(idx):
        return ((X[idx].astype(np.float64) - mu) / sd)
    t0 = time.time()
    if a.ridge:
        S = np.zeros((DIM, DIM)); b = np.zeros(DIM)
        for i in range(0, len(it), 200000):
            ch = it[i:i + 200000]; Z = std(ch); w = wrow[ch][:, None]
            S += (Z * w).T @ Z; b += (Z * w).T @ (y[ch] - ymean)
        S /= wrow[it].sum(); b /= wrow[it].sum()
        def pred(w, idx):
            return np.concatenate([std(idx[i:i + 200000]) @ w for i in range(0, len(idx), 200000)]) + ymean
        best = None
        for lam in (1e-3, 1e-2, 1e-1, 1.0, 10.0):
            w = np.linalg.solve(S + lam * np.eye(DIM), b); r = float(np.sqrt(np.average((pred(w, iv) - y[iv]) ** 2, weights=wrow[iv])))
            print(f"lambda {lam:g} val_rmse={r:.4f}", flush=True)
            if best is None or r < best[0]: best = (r, lam, w)
        w = best[2]; ph = pred(w, ih)
        rmse = float(np.sqrt(((ph - y[ih]) ** 2).mean())); corr = float(np.corrcoef(ph, y[ih])[0, 1])
        W1 = (w / sd)[None, :]; b1 = np.array([ymean - float((w / sd) @ mu)])
        out = export(a, [(W1, b1)], rmse, corr, len(trn), 0, 0, DIM + 1, base)
        print(f"VR2 id={a.id} runs={len(trn)} ridge lambda={best[1]:g} holdout_rmse={rmse:.4f} holdout_corr={corr:.3f} predict_mean_rmse={base:.4f} {time.time()-t0:.0f}s exported {out}")
        return
    import torch, torch.nn as nn
    torch.manual_seed(0); torch.set_num_threads(4); g = np.random.RandomState(7)
    dims = [DIM] + [a.hidden] * a.layers + [1]; mods = []
    for i in range(len(dims) - 1):
        mods.append(nn.Linear(dims[i], dims[i + 1]))
        if i < len(dims) - 2: mods.append(nn.ReLU())
    model = nn.Sequential(*mods); nparams = sum(p.numel() for p in model.parameters())
    opt = torch.optim.Adam(model.parameters(), lr=1e-3, weight_decay=1e-4)
    mu_t = torch.tensor(mu, dtype=torch.float32); sd_t = torch.tensor(sd, dtype=torch.float32)
    def predict(idx):
        out = []
        with torch.no_grad():
            for i in range(0, len(idx), 100000):
                xb = (torch.from_numpy(X[idx[i:i + 100000]]) - mu_t) / sd_t
                out.append(model(xb).squeeze(1).numpy() * ysd + ymean)
        return np.concatenate(out)
    best = (1e30, None, -1); bad = 0
    wnet = np.ones(len(run), dtype=np.float32)   # per sampled state: n_original / n_sampled of its run (--state-weights)
    if a.state_weights:
        for rr in Rt:
            wnet[rr] = wrow[rr] * len(rr) / min(len(rr), a.k)
    for ep in range(80):
        model.train()
        idx = np.concatenate([r if len(r) <= a.k else g.choice(r, a.k, replace=False) for r in Rt]); g.shuffle(idx)
        for i in range(0, len(idx), 512):
            bi = idx[i:i + 512]
            xb = (torch.from_numpy(X[bi]) - mu_t) / sd_t; yb = (torch.from_numpy(y[bi]) - ymean) / ysd
            wb = torch.from_numpy(wnet[bi])
            loss = (wb * (model(xb).squeeze(1) - yb) ** 2).sum() / wb.sum(); opt.zero_grad(); loss.backward(); opt.step()
        model.eval(); pv = predict(iv); r = float(np.sqrt(np.average((pv - y[iv]) ** 2, weights=wrow[iv])))
        print(f"ep {ep+1:2d} val_rmse={r:.4f} {time.time()-t0:.0f}s", flush=True)
        if r < best[0] - 1e-5:
            best = (r, [(m.weight.detach().numpy().astype(np.float64).copy(), m.bias.detach().numpy().astype(np.float64).copy()) for m in model if isinstance(m, nn.Linear)], ep); bad = 0
        else:
            bad += 1
            if bad >= a.patience: break
    with torch.no_grad():
        for m, (W, b) in zip([m for m in model if isinstance(m, nn.Linear)], best[1]):
            m.weight.copy_(torch.tensor(W, dtype=torch.float32)); m.bias.copy_(torch.tensor(b, dtype=torch.float32))
    model.eval(); ph = predict(ih)
    rmse = float(np.sqrt(((ph - y[ih]) ** 2).mean())); corr = float(np.corrcoef(ph, y[ih])[0, 1])
    layers = best[1]
    W1, b1 = layers[0]; layers[0] = (W1 / sd[None, :], b1 - (W1 / sd[None, :]) @ mu)          # fold standardisation in
    layers[-1] = (layers[-1][0] * ysd, layers[-1][1] * ysd + ymean)
    out = export(a, layers, rmse, corr, len(trn), a.hidden, a.layers, nparams, base)
    print(f"VR2 id={a.id} runs={len(trn)} hidden={a.hidden} layers={a.layers} params={nparams} best_epoch={best[2]+1} holdout_rmse={rmse:.4f} holdout_corr={corr:.3f} predict_mean_rmse={base:.4f} {time.time()-t0:.0f}s exported {out}")


if __name__ == "__main__":
    main()
