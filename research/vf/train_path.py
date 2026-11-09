#!/usr/bin/env python3
"""Phase F35 (pre-registered in program.md F35): a learned path-choice model from map decisions.

Each decision (one path_<seed>.bin from fair_eval --branch_eval K M --branch_kind 5) holds, per legal next map node, a row of
666 run features of the current (pre-move) state + 20 path features (destination room one-hot, min/max room-type counts over
the paths to the boss row, log #paths, the agent's own map-DP value / 10) + 8 tail floats [opt, n, mean, ok, floor, own, meanA,
meanB]; mean = the playout mean from that option.
Model: MLP 686 -> H -> H -> 1 (ReLU), fresh init (seed 0). Loss per decision = mean_o ((V(o) - mean_o V) - (y_o - mean_o y))^2
(within-decision centred regression, as train_dec.py). Early stopping: 10% of the decisions (seed hash) held out; criterion =
mean playout value of the option the model ranks first; patience 8, best state kept. Export: research/models/pm_<id>.txt in
the vr_mlp text format (dim nlayers 1, then per layer: in out, W row-major [out][in], b) + .meta
(val_pick val_pick_dp n_train_decisions hidden).
F38: --keys reads pathk_<seed>.bin (fair_eval --branch_kind 6): 666 run features + 4 key features + 20 path features + 8 tail
floats per row (input 690), labelled with the Heart score.
usage: train_path.py --id N [--hidden 64] [--keys] DIR [DIR ...]"""
import argparse, glob, os, re
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIM, PF, TAIL, MAXO = 666, 20, 8, 7
IN = DIM + PF


PREFIX = "path_"


def set_keys(on):
    global DIM, IN, PREFIX
    if on: DIM, PREFIX = 666 + 4, "pathk_"
    IN = DIM + PF


def load(dirs):
    X, Y, M, D, S = [], [], [], [], []
    for d in dirs:
        for f in sorted(glob.glob(os.path.join(d, PREFIX + "*.bin"))):
            a = np.fromfile(f, dtype=np.float32)
            if a.size == 0 or a.size % (IN + TAIL): continue
            a = a.reshape(-1, IN + TAIL)
            if (a[:, IN + 2] < 0).any() or (a[:, IN + 3] < 1).any() or len(a) < 2 or len(a) > MAXO: continue
            n = len(a)
            x = np.zeros((MAXO, IN), np.float32); y = np.zeros(MAXO, np.float32); m = np.zeros(MAXO, np.float32)
            x[:n] = a[:, :IN]; y[:n] = a[:, IN + 2]; m[:n] = 1
            X.append(x); Y.append(y); M.append(m); D.append(x[:, DIM + 19].copy()); S.append(int(re.search(PREFIX + r"(\d+)", f).group(1)))
    return np.stack(X), np.stack(Y), np.stack(M), np.stack(D), np.array(S)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+"); ap.add_argument("--id", type=int, required=True)
    ap.add_argument("--hidden", type=int, default=64); ap.add_argument("--epochs", type=int, default=60)
    ap.add_argument("--patience", type=int, default=8); ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--batch", type=int, default=256); ap.add_argument("--keys", action="store_true")
    a = ap.parse_args()
    set_keys(a.keys)
    import torch, torch.nn as nn
    torch.manual_seed(0); torch.set_num_threads(4)
    X, Y, M, Dp, S = load(a.dirs)
    val = (S * 2654435761 % 2**32) % 10 == 0
    print(f"PATH data: decisions={len(S)} train={(~val).sum()} val={val.sum()} options={int(M.sum())}")
    H = a.hidden
    net = nn.Sequential(nn.Linear(IN, H), nn.ReLU(), nn.Linear(H, H), nn.ReLU(), nn.Linear(H, 1))
    Xt, Yt, Mt, Dt = torch.tensor(X), torch.tensor(Y), torch.tensor(M), torch.tensor(Dp)

    def pick_value(V, idx):
        Vm = V[idx].masked_fill(Mt[idx] == 0, -1e9)
        return Yt[idx].gather(1, Vm.argmax(1, keepdim=True)).mean().item()

    vi = torch.tensor(np.where(val)[0]); ti = torch.tensor(np.where(~val)[0])
    dp_val = pick_value(Dt, vi)
    print(f"PATH map-DP (own) pick on val: {dp_val:.4f}")
    opt = torch.optim.Adam(net.parameters(), lr=a.lr)
    best, best_state, bad = -1e9, None, 0
    g = torch.Generator().manual_seed(1)
    for ep in range(a.epochs):
        net.train(); perm = ti[torch.randperm(len(ti), generator=g)]; tot = 0.0
        for s in range(0, len(perm), a.batch):
            b = perm[s:s + a.batch]; m = Mt[b]; n = m.sum(1, keepdim=True)
            v = net(Xt[b].reshape(-1, IN)).reshape(len(b), MAXO)
            vc = v - (v * m).sum(1, keepdim=True) / n; yc = Yt[b] - (Yt[b] * m).sum(1, keepdim=True) / n
            loss = ((((vc - yc) ** 2) * m).sum(1) / n.squeeze(1)).mean()
            opt.zero_grad(); loss.backward(); opt.step(); tot += loss.item() * len(b)
        net.eval()
        with torch.no_grad():
            V = net(Xt.reshape(-1, IN)).reshape(len(S), MAXO)
        pv = pick_value(V, vi)
        print(f"PATH ep {ep:2d} loss={tot / len(ti):.5f} val_pick={pv:.4f}")
        if pv > best + 1e-9: best, best_state, bad = pv, {k: v.clone() for k, v in net.state_dict().items()}, 0
        else:
            bad += 1
            if bad >= a.patience: break
    net.load_state_dict(best_state)
    lins = [m for m in net if isinstance(m, nn.Linear)]
    out = os.path.join(ROOT, "models", f"pm_{a.id}.txt")
    with open(out, "w") as f:
        f.write(f"{IN} {len(lins)} 1\n")
        for lin in lins:
            W = lin.weight.data.numpy(); bb = lin.bias.data.numpy()
            f.write(f"{W.shape[1]} {W.shape[0]}\n" + " ".join(f"{v:.6g}" for v in W.ravel()) + "\n" + " ".join(f"{v:.6g}" for v in bb) + "\n")
    open(os.path.join(ROOT, "models", f"pm_{a.id}.meta"), "w").write(f"{best:.6f} {dp_val:.6f} {(~val).sum()} {H}\n")
    print(f"PATH id={a.id} val_pick={best:.4f} (map-DP {dp_val:.4f}) exported {out}")


if __name__ == "__main__":
    main()
