#!/usr/bin/env python3
"""Bootstrapped value training (Phase B, AlphaZero-style targets).

Target for a decision state = the search's own root mean value (v2 pol logs), which is
action-conditional and within-turn sensitive — unlike the shared final battle outcome that
made the first learned leaf useless. Exports the same vf_weights.inc layout.

usage: train_vf_boot.py DATA_DIR [--out research/cpp/vf_weights.inc] [--hidden 64] [--epochs 25]
"""
import argparse, os, sys, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from train_vf import export, DIM  # noqa: E402
import train_policy as tp  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data_dir")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(HERE), "cpp", "vf_weights.inc"))
    ap.add_argument("--hidden", type=int, default=64)
    ap.add_argument("--epochs", type=int, default=25)
    a = ap.parse_args()
    import torch, torch.nn as nn
    torch.manual_seed(0)
    states, toks, ups, tfs, vis, dec, qs, roots = tp.load(a.data_dir, with_values=True)
    X = np.stack(states).astype(np.float32); y = np.array(roots, np.float32)
    ok = np.isfinite(X).all(1) & np.isfinite(y) & (y != 0)
    X, y = X[ok], y[ok]
    print(f"decisions={len(y)} target(root mean) mean={y.mean():.2f} sd={y.std():.2f}")
    rng = np.random.RandomState(0); perm = rng.permutation(len(y)); nv = len(y) // 5
    vi, ti = perm[:nv], perm[nv:]
    Xt, yt, Xv, yv = map(torch.tensor, (X[ti], y[ti], X[vi], y[vi]))
    ymean, ysd = float(yt.mean()), float(yt.std() + 1e-6)
    model = nn.Sequential(nn.Linear(DIM, a.hidden), nn.ReLU(), nn.Linear(a.hidden, 1))
    opt = torch.optim.Adam(model.parameters(), lr=2e-3, weight_decay=1e-5)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, a.epochs)
    base = float(torch.sqrt(((yv - ymean) ** 2).mean())); t0 = time.time(); bs = 1024
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
        print(f"ep {ep+1:2d} val_rmse={rmse:.2f} (predict-mean {base:.2f}) {time.time()-t0:.0f}s")
    W1 = model[0].weight.detach().numpy(); b1 = model[0].bias.detach().numpy()
    W2 = model[2].weight.detach().numpy().ravel() * ysd; b2 = float(model[2].bias.detach().numpy()[0] * ysd + ymean)
    export(a.out, W1, b1, W2, b2, trained=1)
    print("exported", a.out)


if __name__ == "__main__":
    main()
