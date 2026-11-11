#!/usr/bin/env python3
"""Phase F52 streaming trainer for leaf value models (data larger than RAM).

Same model, loss, standardisation and export format as train_sv.py --leaf (DIM -> hidden x layers -> 1, ReLU,
standardised MSE on the mean-of-K-rollouts target, Adam 1e-3, weight decay 1e-5, cosine schedule, best-holdout
epoch exported for research/vf/sv_mlp.h). Differences, all forced by size:
  * rows stream from file groups (a packed runstore shard = one group; raw leaf_*.bin dirs are grouped by 250
    files); each epoch visits the groups in a seeded random order, BUF groups at a time, rows shuffled within the
    buffer;
  * the holdout is by game: files whose crc32(basename) % 100 == 0 (capped at --hold_rows rows, deterministic);
  * target mean/sd come from the training rows of the first --stat_groups groups (in fixed sorted order).
--compare MODEL.txt scores an exported model on the same holdout (the reproduction check).

usage: train_sv_stream.py SRC [SRC ...] --id N --hidden H [--layers 2] [--epochs 8] [--buf 8] [--compare M.txt]
"""
import argparse, glob, os, sys, time, zlib
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import runstore  # noqa: E402
from train_vf import DIM  # noqa: E402

W = DIM + 2


def groups_of(src, per=250):
    files = runstore.list_files(src)
    files = [f for f in files if os.path.basename(f).startswith("leaf_")]
    by = {}
    for n, f in enumerate(files):
        d, e = runstore._loc(f)
        key = (src, e[0]) if d is not None else (src, n // per)
        by.setdefault(key, []).append(f)
    return [by[k] for k in sorted(by)]


def is_hold(f):
    return zlib.crc32(os.path.basename(f).encode()) % 100 == 0


def load(files):
    parts = [runstore.read(f) for f in files]
    a = np.concatenate(parts) if parts else np.zeros(0, np.float32)
    a = a[: (a.size // W) * W].reshape(-1, W)
    X, y = a[:, :DIM], a[:, DIM]
    ok = np.isfinite(X).all(1) & np.isfinite(y) & (y != 0)
    return np.ascontiguousarray(X[ok]), np.ascontiguousarray(y[ok])


def forward_txt(path, X):
    tok = open(path).read().split()
    din, nl = int(tok[0]), int(tok[1]); i = 3; h = X.astype(np.float64)
    for k in range(nl):
        ni, no = int(tok[i]), int(tok[i + 1]); i += 2
        Wm = np.array(tok[i:i + ni * no], float).reshape(no, ni); i += ni * no
        b = np.array(tok[i:i + no], float); i += no
        h = h @ Wm.T + b
        if k < nl - 1: h = np.maximum(h, 0)
    return h[:, 0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src", nargs="+"); ap.add_argument("--id", type=int, required=True)
    ap.add_argument("--hidden", type=int, required=True); ap.add_argument("--layers", type=int, default=2)
    ap.add_argument("--epochs", type=int, default=8); ap.add_argument("--buf", type=int, default=8)
    ap.add_argument("--hold_rows", type=int, default=200000); ap.add_argument("--stat_groups", type=int, default=4)
    ap.add_argument("--compare", default=None); ap.add_argument("--out", default=None)
    ap.add_argument("--max_groups", type=int, default=0, help="use only the first N groups (sorted) - for nested scale tests")
    a = ap.parse_args()
    out = a.out or os.path.join(os.path.dirname(HERE), "models", f"sv_{a.id}.txt")
    import torch, torch.nn as nn
    torch.manual_seed(0); torch.set_num_threads(4)
    groups = [g for s in a.src for g in groups_of(s)]
    if a.max_groups: groups = groups[: a.max_groups]
    hold_files = [f for g in groups for f in g if is_hold(f)]
    train_groups = [[f for f in g if not is_hold(f)] for g in groups]
    train_groups = [g for g in train_groups if g]
    Xv, yv = load(hold_files)
    if len(yv) > a.hold_rows:
        sel = np.random.RandomState(7).choice(len(yv), a.hold_rows, replace=False); sel.sort(); Xv, yv = Xv[sel], yv[sel]
    ys = np.concatenate([load(g)[1] for g in train_groups[: a.stat_groups]])
    ymean, ysd = float(ys.mean()), float(ys.std() + 1e-6)
    print(f"groups={len(groups)} files={sum(len(g) for g in groups)} hold_files={len(hold_files)} holdout_rows={len(yv)} "
          f"target mean={ymean:.2f} sd={ysd:.2f} (from {a.stat_groups} groups)", flush=True)
    if a.compare:
        pv = forward_txt(a.compare, Xv)
        print(f"COMPARE {a.compare} holdout_rmse={float(np.sqrt(((pv - yv) ** 2).mean())):.3f}", flush=True)
    dims = [DIM] + [a.hidden] * a.layers + [1]
    mods = []
    for i in range(len(dims) - 1):
        mods.append(nn.Linear(dims[i], dims[i + 1]))
        if i < len(dims) - 2: mods.append(nn.ReLU())
    model = nn.Sequential(*mods)
    nparams = sum(p.numel() for p in model.parameters())
    opt = torch.optim.Adam(model.parameters(), lr=1e-3, weight_decay=1e-5)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, a.epochs)
    Xvt, yvt = torch.tensor(Xv), torch.tensor(yv)
    base = float(torch.sqrt(((yvt - ymean) ** 2).mean())); t0 = time.time(); bs = 1024
    best = (1e30, None); rng = np.random.RandomState(12345); ntr = 0
    for ep in range(a.epochs):
        model.train(); order = rng.permutation(len(train_groups)); ntr = 0
        for s in range(0, len(order), a.buf):
            X, y = load([f for k in order[s:s + a.buf] for f in train_groups[k]])
            ntr += len(y)
            Xt, yt = torch.tensor(X), torch.tensor((y - ymean) / ysd)
            p = torch.randperm(len(yt))
            for i in range(0, len(yt), bs):
                idx = p[i:i + bs]
                loss = ((model(Xt[idx]).squeeze(1) - yt[idx]) ** 2).mean()
                opt.zero_grad(); loss.backward(); opt.step()
        sched.step(); model.eval()
        with torch.no_grad():
            pv = model(Xvt).squeeze(1) * ysd + ymean
            rmse = float(torch.sqrt(((pv - yvt) ** 2).mean()))
        if rmse < best[0]: best = (rmse, [(m.weight.detach().numpy().copy(), m.bias.detach().numpy().copy()) for m in model if isinstance(m, nn.Linear)])
        print(f"ep {ep+1:2d} train_rows={ntr} holdout_rmse={rmse:.3f} (predict-mean {base:.3f}) {time.time()-t0:.0f}s", flush=True)
    rmse, layers = best
    layers[-1] = (layers[-1][0] * ysd, layers[-1][1] * ysd + ymean)
    for Wm, b in layers:
        if not (np.isfinite(Wm).all() and np.isfinite(b).all()): raise SystemExit("non-finite weights")
    with open(out, "w") as f:
        f.write(f"{DIM} {len(layers)} 1\n")
        for Wm, b in layers:
            f.write(f"{Wm.shape[1]} {Wm.shape[0]}\n")
            f.write(" ".join(f"{v:.6g}" for v in Wm.ravel()) + "\n")
            f.write(" ".join(f"{v:.6g}" for v in b) + "\n")
    pv2 = forward_txt(out, Xv); rmse_txt = float(np.sqrt(((pv2 - yv) ** 2).mean()))
    open(os.path.join(os.path.dirname(out), f"sv_{a.id}.meta"), "w").write(f"{rmse:.6f} {base:.6f} {ntr} {a.hidden} {a.layers} {nparams} 1\n")
    print(f"SV id={a.id} rows={ntr} hidden={a.hidden} layers={a.layers} params={nparams} holdout_rmse={rmse:.3f} "
          f"exported_rmse={rmse_txt:.3f} predict_mean_rmse={base:.3f} exported {out}", flush=True)


if __name__ == "__main__":
    main()
