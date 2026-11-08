#!/usr/bin/env python3
"""Phase F28 (pre-registered in program.md F28): a decision-trained run value from interventional branch data.

Each decision (one branch_<seed>.bin from fair_eval --branch_eval) holds, per option, the option state's 666 run features
and y = the mean score of M playouts from that state (same playout seeds for every option of the decision).
Model: the MLP of research/models/vr_<init>.txt (same shape), initialised from it.
Loss per decision = mean_o ((V(o) - mean_o V) - (y_o - mean_o y))^2          (within-decision centred regression)
                  + lam * mean_o (V(o) - V_init(o))^2                          (anchor to the init model's level)
Early stopping: 10% of the decisions (seed hash) held out; criterion = mean playout value of the option the model ranks
first (the F26 metric); patience 5 epochs, best state kept. Exported as research/models/vr_<id>.txt (+ .meta:
val_pick_value val_pick_value_init n_train_decisions hidden n_layers n_params lam).
usage: train_dec.py --id 250 --lam 0.1 [--init 171] DIR [DIR ...]   (DIRs hold plain branch_*.bin files)"""
import argparse, glob, os, re
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIM, TAIL, MAXO = 666, 6, 8


def load_model(i):
    tok = open(os.path.join(ROOT, "models", f"vr_{i}.txt")).read().split()
    md, nl = int(tok[0]), int(tok[1]); p = 3; L = []
    assert md == DIM, "init model must take the 666 v1 features"
    for _ in range(nl):
        a, b = int(tok[p]), int(tok[p + 1]); p += 2
        W = np.array(tok[p:p + a * b], np.float32).reshape(b, a); p += a * b
        bb = np.array(tok[p:p + b], np.float32); p += b; L.append((W, bb))
    return L


def load_decisions(dirs):
    X, Y, M, S = [], [], [], []
    for d in dirs:
        for f in sorted(glob.glob(os.path.join(d, "branch_*.bin"))):
            a = np.fromfile(f, dtype=np.float32)
            if a.size == 0 or a.size % (DIM + TAIL): continue
            a = a.reshape(-1, DIM + TAIL)
            if (a[:, DIM + 2] < 0).any() or (a[:, DIM + 3] < 1).any() or len(a) < 2 or len(a) > MAXO: continue
            x = np.zeros((MAXO, DIM), np.float32); y = np.zeros(MAXO, np.float32); m = np.zeros(MAXO, np.float32)
            n = len(a); x[:n] = a[:, :DIM]; y[:n] = a[:, DIM + 2]; m[:n] = 1
            X.append(x); Y.append(y); M.append(m); S.append(int(re.search(r"branch_(\d+)", f).group(1)))
    return np.stack(X), np.stack(Y), np.stack(M), np.array(S)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+"); ap.add_argument("--id", type=int, required=True)
    ap.add_argument("--lam", type=float, required=True); ap.add_argument("--init", type=int, default=171)
    ap.add_argument("--epochs", type=int, default=40); ap.add_argument("--patience", type=int, default=5)
    ap.add_argument("--lr", type=float, default=3e-4); ap.add_argument("--batch", type=int, default=256)
    a = ap.parse_args()
    import torch, torch.nn as nn
    torch.manual_seed(0); torch.set_num_threads(4)
    X, Y, M, S = load_decisions(a.dirs)
    val = (S * 2654435761 % 2**32) % 10 == 0
    print(f"DEC data: decisions={len(S)} train={(~val).sum()} val={val.sum()} options={int(M.sum())}")
    L0 = load_model(a.init)
    mods = []
    for k, (W, b) in enumerate(L0):
        lin = nn.Linear(W.shape[1], W.shape[0]); lin.weight.data = torch.tensor(W); lin.bias.data = torch.tensor(b)
        mods.append(lin)
        if k + 1 < len(L0): mods.append(nn.ReLU())
    net = nn.Sequential(*mods)
    Xt, Yt, Mt = torch.tensor(X), torch.tensor(Y), torch.tensor(M)
    with torch.no_grad():
        V0 = net(Xt.reshape(-1, DIM)).reshape(len(S), MAXO)

    def pick_value(V, idx):
        Vm = V[idx].masked_fill(Mt[idx] == 0, -1e9)
        return Yt[idx].gather(1, Vm.argmax(1, keepdim=True)).mean().item()

    vi = torch.tensor(np.where(val)[0]); ti = torch.tensor(np.where(~val)[0])
    base_val = pick_value(V0, vi)
    opt = torch.optim.Adam(net.parameters(), lr=a.lr)
    best, best_state, bad = pick_value(V0, vi), {k: v.clone() for k, v in net.state_dict().items()}, 0
    print(f"DEC init vr_{a.init}: val pick value {base_val:.4f}")
    g = torch.Generator().manual_seed(1)
    for ep in range(a.epochs):
        net.train(); perm = ti[torch.randperm(len(ti), generator=g)]; tot = 0.0
        for s in range(0, len(perm), a.batch):
            b = perm[s:s + a.batch]; m = Mt[b]; n = m.sum(1, keepdim=True)
            v = net(Xt[b].reshape(-1, DIM)).reshape(len(b), MAXO)
            vc = v - (v * m).sum(1, keepdim=True) / n; yc = Yt[b] - (Yt[b] * m).sum(1, keepdim=True) / n
            loss = ((((vc - yc) ** 2) * m).sum(1) / n.squeeze(1)).mean() + a.lam * ((((v - V0[b]) ** 2) * m).sum(1) / n.squeeze(1)).mean()
            opt.zero_grad(); loss.backward(); opt.step(); tot += loss.item() * len(b)
        net.eval()
        with torch.no_grad():
            V = net(Xt.reshape(-1, DIM)).reshape(len(S), MAXO)
        pv = pick_value(V, vi)
        print(f"DEC ep {ep:2d} loss={tot / len(ti):.5f} val_pick={pv:.4f}")
        if pv > best + 1e-9: best, best_state, bad = pv, {k: v.clone() for k, v in net.state_dict().items()}, 0
        else:
            bad += 1
            if bad >= a.patience: break
    net.load_state_dict(best_state)
    lins = [m for m in net if isinstance(m, nn.Linear)]
    out = os.path.join(ROOT, "models", f"vr_{a.id}.txt")
    with open(out, "w") as f:
        f.write(f"{DIM} {len(lins)} 1\n")
        for lin in lins:
            W = lin.weight.data.numpy(); bb = lin.bias.data.numpy()
            f.write(f"{W.shape[1]} {W.shape[0]}\n" + " ".join(f"{v:.6g}" for v in W.ravel()) + "\n" + " ".join(f"{v:.6g}" for v in bb) + "\n")
    nparams = sum(p.numel() for p in net.parameters())
    open(os.path.join(ROOT, "models", f"vr_{a.id}.meta"), "w").write(
        f"{best:.6f} {base_val:.6f} {(~val).sum()} {lins[0].weight.shape[0]} {len(lins)} {nparams} {a.lam}\n")
    print(f"DEC id={a.id} lam={a.lam} val_pick={best:.4f} (init {base_val:.4f}) exported {out}")


if __name__ == "__main__":
    main()
