#!/usr/bin/env python3
"""Phase S trainer: leaf value models of chosen size on a nested subset of bootstrapped targets.

Data: pf_log v2 policy logs (pol_<seed>.bin) — one state per decision with the search's root mean value
(AlphaZero-style bootstrapped target, the Phase B target that beat final outcomes). Rows are shuffled once
with a fixed seed and the first --rows are used, so smaller data scales are nested subsets of larger ones.
Model: DIM -> hidden -> hidden -> 1 (ReLU), standardised regression on the root value. Exports the text
format read by research/vf/sv_mlp.h.

usage: train_sv.py DATA_DIR --id N --rows R --hidden H [--layers 2] [--epochs 20] [--out research/models/sv_N.txt]
"""
import argparse, os, sys, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import train_policy as tp  # noqa: E402
from train_vf import DIM  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data_dir"); ap.add_argument("--id", type=int, required=True)
    ap.add_argument("--rows", type=int, required=True); ap.add_argument("--hidden", type=int, required=True)
    ap.add_argument("--layers", type=int, default=2); ap.add_argument("--epochs", type=int, default=20)
    ap.add_argument("--out", default=None)
    a = ap.parse_args()
    out = a.out or os.path.join(os.path.dirname(HERE), "models", f"sv_{a.id}.txt")
    import torch, torch.nn as nn
    torch.manual_seed(0); torch.set_num_threads(4)
    states, toks, ups, tfs, vis, dec, qs, roots = tp.load(a.data_dir, with_values=True)
    X = np.stack(states).astype(np.float32); y = np.array(roots, np.float32)
    ok = np.isfinite(X).all(1) & np.isfinite(y) & (y != 0)
    X, y = X[ok], y[ok]
    rng = np.random.RandomState(12345); perm = rng.permutation(len(y))   # one fixed shuffle for every scale
    nv = max(2000, min(len(y) // 10, 50000))                               # held-out block: the LAST nv of the shuffle
    hold = perm[-nv:]; pool = perm[:-nv]
    tr = pool[:a.rows]
    print(f"decisions total={len(y)} train={len(tr)} (requested {a.rows}) holdout={len(hold)} target mean={y.mean():.2f} sd={y.std():.2f}")
    Xt, yt = torch.tensor(X[tr]), torch.tensor(y[tr]); Xv, yv = torch.tensor(X[hold]), torch.tensor(y[hold])
    ymean, ysd = float(yt.mean()), float(yt.std() + 1e-6)
    dims = [DIM] + [a.hidden] * a.layers + [1]
    mods = []
    for i in range(len(dims) - 1):
        mods.append(nn.Linear(dims[i], dims[i + 1]))
        if i < len(dims) - 2: mods.append(nn.ReLU())
    model = nn.Sequential(*mods)
    nparams = sum(p.numel() for p in model.parameters())
    opt = torch.optim.Adam(model.parameters(), lr=1e-3, weight_decay=1e-5)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, a.epochs)
    base = float(torch.sqrt(((yv - ymean) ** 2).mean())); t0 = time.time(); bs = 1024
    best = (1e30, None)
    for ep in range(a.epochs):
        model.train(); p = torch.randperm(len(yt))
        for i in range(0, len(yt), bs):
            idx = p[i:i + bs]
            loss = ((model(Xt[idx]).squeeze(1) - (yt[idx] - ymean) / ysd) ** 2).mean()
            opt.zero_grad(); loss.backward(); opt.step()
        sched.step(); model.eval()
        with torch.no_grad():
            pv = model(Xv).squeeze(1) * ysd + ymean
            rmse = float(torch.sqrt(((pv - yv) ** 2).mean()))
        if rmse < best[0]: best = (rmse, [(m.weight.detach().numpy().copy(), m.bias.detach().numpy().copy()) for m in model if isinstance(m, nn.Linear)])
        print(f"ep {ep+1:2d} holdout_rmse={rmse:.2f} (predict-mean {base:.2f}) {time.time()-t0:.0f}s")
    rmse, layers = best
    # fold the standardisation into the last layer
    layers[-1] = (layers[-1][0] * ysd, layers[-1][1] * ysd + ymean)
    for W, b in layers:
        if not (np.isfinite(W).all() and np.isfinite(b).all()): raise SystemExit("non-finite weights")
    with open(out, "w") as f:
        f.write(f"{DIM} {len(layers)} 1\n")
        for W, b in layers:
            f.write(f"{W.shape[1]} {W.shape[0]}\n")
            f.write(" ".join(f"{v:.6g}" for v in W.ravel()) + "\n")
            f.write(" ".join(f"{v:.6g}" for v in b) + "\n")
    print(f"SV id={a.id} rows={len(tr)} hidden={a.hidden} layers={a.layers} params={nparams} holdout_rmse={rmse:.3f} predict_mean_rmse={base:.3f} exported {out}")


if __name__ == "__main__":
    main()
