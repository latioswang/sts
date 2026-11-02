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
    a = ap.parse_args()
    files = sorted(glob.glob(os.path.join(a.data_dir, "run_*.bin")))
    arr = np.concatenate([np.fromfile(f, dtype=np.float32) for f in files])
    arr = arr[: (arr.size // (DIM + EXTRA)) * (DIM + EXTRA)].reshape(-1, DIM + EXTRA)
    X = arr[:, :DIM]; run = arr[:, DIM + 3].astype(np.int64)
    y = (np.minimum(arr[:, DIM + 1], 51) / 51 + arr[:, DIM]).astype(np.float32)
    runs = np.unique(run); rng = np.random.RandomState(12345); rng.shuffle(runs)
    hold = runs[-a.holdout:]; pool = runs[:-a.holdout][: a.runs]
    nval = max(200, len(pool) // 10); val, trn = pool[-nval:], pool[:-nval]
    order = np.argsort(run, kind="stable"); rs = run[order]
    def rows(ids):
        ids = np.sort(ids); lo = np.searchsorted(rs, ids, "left"); hi = np.searchsorted(rs, ids, "right")
        return [order[l:h] for l, h in zip(lo, hi)]
    Rt, Rv, Rh = rows(trn), rows(val), rows(hold)
    it = np.concatenate(Rt); iv = np.concatenate(Rv); ih = np.concatenate(Rh)
    mu = X[it].mean(0, dtype=np.float64); sd = X[it].std(0, dtype=np.float64); sd[sd < 1e-6] = 1.0
    ymean = float(y[it].mean()); ysd = float(y[it].std() + 1e-6)
    base = float(np.sqrt(((y[ih] - ymean) ** 2).mean()))
    print(f"runs total={len(runs)} train={len(trn)} val={len(val)} holdout={len(hold)}; rows train={len(it)} val={len(iv)} holdout={len(ih)}", flush=True)
    def std(idx):
        return ((X[idx].astype(np.float64) - mu) / sd)
    t0 = time.time()
    if a.ridge:
        S = np.zeros((DIM, DIM)); b = np.zeros(DIM)
        for i in range(0, len(it), 200000):
            Z = std(it[i:i + 200000]); S += Z.T @ Z; b += Z.T @ (y[it[i:i + 200000]] - ymean)
        S /= len(it); b /= len(it)
        def pred(w, idx):
            return np.concatenate([std(idx[i:i + 200000]) @ w for i in range(0, len(idx), 200000)]) + ymean
        best = None
        for lam in (1e-3, 1e-2, 1e-1, 1.0, 10.0):
            w = np.linalg.solve(S + lam * np.eye(DIM), b); r = float(np.sqrt(((pred(w, iv) - y[iv]) ** 2).mean()))
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
    for ep in range(80):
        model.train()
        idx = np.concatenate([r if len(r) <= a.k else g.choice(r, a.k, replace=False) for r in Rt]); g.shuffle(idx)
        for i in range(0, len(idx), 512):
            bi = idx[i:i + 512]
            xb = (torch.from_numpy(X[bi]) - mu_t) / sd_t; yb = (torch.from_numpy(y[bi]) - ymean) / ysd
            loss = ((model(xb).squeeze(1) - yb) ** 2).mean(); opt.zero_grad(); loss.backward(); opt.step()
        model.eval(); pv = predict(iv); r = float(np.sqrt(((pv - y[iv]) ** 2).mean()))
        print(f"ep {ep+1:2d} val_rmse={r:.4f} {time.time()-t0:.0f}s", flush=True)
        if r < best[0] - 1e-5:
            best = (r, [(m.weight.detach().numpy().astype(np.float64).copy(), m.bias.detach().numpy().astype(np.float64).copy()) for m in model if isinstance(m, nn.Linear)], ep); bad = 0
        else:
            bad += 1
            if bad >= 6: break
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
