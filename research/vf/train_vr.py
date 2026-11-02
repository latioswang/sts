#!/usr/bin/env python3
"""Phase R trainer: V(run state) ≈ E[final score], score = min(final floor, 51)/51 + win.

Data: rv_log rows (run_<seed>.bin): 666 run features + [win, final floor, final act, run id], one row per map step.
The split is by run (a run's states are never in both train and holdout); --runs R keeps the first R runs of one
fixed shuffle, so smaller data scales are nested subsets. Model: 666 -> h (x layers) -> 1, ReLU, MSE on the score.
Exports research/models/vr_<id>.txt (format read by research/vf/vr_mlp.h).
usage: train_vr.py DATA_DIR --id N --runs R --hidden H [--layers 2] [--epochs 20] [--holdout 1000]
"""
import argparse, glob, os, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
DIM, EXTRA = 666, 4


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data_dir"); ap.add_argument("--id", type=int, required=True)
    ap.add_argument("--runs", type=int, required=True); ap.add_argument("--hidden", type=int, required=True)
    ap.add_argument("--layers", type=int, default=2); ap.add_argument("--epochs", type=int, default=20)
    ap.add_argument("--holdout", type=int, default=1000)
    a = ap.parse_args()
    import torch, torch.nn as nn
    torch.manual_seed(0); torch.set_num_threads(4)
    files = sorted(glob.glob(os.path.join(a.data_dir, "run_*.bin")))
    arr = np.concatenate([np.fromfile(f, dtype=np.float32) for f in files])
    arr = arr[: (arr.size // (DIM + EXTRA)) * (DIM + EXTRA)].reshape(-1, DIM + EXTRA)
    X = arr[:, :DIM]; win, floor, run = arr[:, DIM], arr[:, DIM + 1], arr[:, DIM + 3].astype(np.int64)
    y = (np.minimum(floor, 51) / 51 + win).astype(np.float32)
    runs = np.unique(run); rng = np.random.RandomState(12345); rng.shuffle(runs)
    hold = set(runs[-a.holdout:].tolist()); pool = runs[:-a.holdout][: a.runs]; keep = set(pool.tolist())
    tr = np.array([r in keep for r in run]); ho = np.array([r in hold for r in run])
    print(f"runs total={len(runs)} train runs={len(pool)} (requested {a.runs}) holdout runs={len(hold)}; rows train={tr.sum()} holdout={ho.sum()}; score mean {y.mean():.3f} sd {y.std():.3f}")
    Xt, yt, Xv, yv = map(torch.tensor, (X[tr], y[tr], X[ho], y[ho]))
    ymean, ysd = float(yt.mean()), float(yt.std() + 1e-6)
    dims = [DIM] + [a.hidden] * a.layers + [1]; mods = []
    for i in range(len(dims) - 1):
        mods.append(nn.Linear(dims[i], dims[i + 1]))
        if i < len(dims) - 2: mods.append(nn.ReLU())
    model = nn.Sequential(*mods); nparams = sum(p.numel() for p in model.parameters())
    opt = torch.optim.Adam(model.parameters(), lr=1e-3, weight_decay=1e-4)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, a.epochs)
    base = float(torch.sqrt(((yv - ymean) ** 2).mean())); best = (1e30, None, 0.0); t0 = time.time(); bs = 1024
    for ep in range(a.epochs):
        model.train(); p = torch.randperm(len(yt))
        for i in range(0, len(yt), bs):
            idx = p[i:i + bs]
            loss = ((model(Xt[idx]).squeeze(1) - (yt[idx] - ymean) / ysd) ** 2).mean()
            opt.zero_grad(); loss.backward(); opt.step()
        sched.step(); model.eval()
        with torch.no_grad():
            pv = model(Xv).squeeze(1) * ysd + ymean
            rmse = float(torch.sqrt(((pv - yv) ** 2).mean())); corr = float(np.corrcoef(pv.numpy(), yv.numpy())[0, 1])
        if rmse < best[0]: best = (rmse, [(m.weight.detach().numpy().copy(), m.bias.detach().numpy().copy()) for m in model if isinstance(m, nn.Linear)], corr)
        print(f"ep {ep+1:2d} holdout_rmse={rmse:.4f} corr={corr:.3f} (predict-mean {base:.4f}) {time.time()-t0:.0f}s")
    rmse, layers, corr = best
    layers[-1] = (layers[-1][0] * ysd, layers[-1][1] * ysd + ymean)
    out = os.path.join(ROOT, "models", f"vr_{a.id}.txt")
    with open(out, "w") as f:
        f.write(f"{DIM} {len(layers)} 1\n")
        for W, b in layers:
            f.write(f"{W.shape[1]} {W.shape[0]}\n" + " ".join(f"{v:.6g}" for v in W.ravel()) + "\n" + " ".join(f"{v:.6g}" for v in b) + "\n")
    open(os.path.join(ROOT, "models", f"vr_{a.id}.meta"), "w").write(f"{rmse:.6f} {corr:.6f} {len(pool)} {a.hidden} {a.layers} {nparams}\n")
    print(f"VR id={a.id} runs={len(pool)} hidden={a.hidden} layers={a.layers} params={nparams} holdout_rmse={rmse:.4f} holdout_corr={corr:.3f} predict_mean_rmse={base:.4f} exported {out}")


if __name__ == "__main__":
    main()
