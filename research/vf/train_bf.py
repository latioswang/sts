#!/usr/bin/env python3
"""Phase C v3: interventional deck value f(run state) -> [P(win vs act boss), boss HP left,
P(win vs hallway fight), hallway HP left], trained on simulator fights played from logged run states
(bf_log=1 rows: RV_DIM features + [boss_win, boss_hp, hall_win, hall_hp, run_id, decision, kind, is_base]).

Every decision contributes a base row and one row per hypothetical option (card added / card upgraded /
card removed), all played with the same fight seeds, so the paired differences are low-noise targets:
loss = BCE(absolute labels) + lam * MSE(predicted option-minus-base difference, observed difference).
Validation is grouped by run. Exports research/cpp/bf_weights.inc (placeholder unless the boss-win head
beats the base rate and the paired differences correlate positively on held-out runs).
usage: train_bf.py DATA_DIR --out research/models/bf_g<n>.txt [--hidden 128] [--epochs 30] [--lam 2.0]
"""
import argparse, glob, os, time
import numpy as np

corr_dmg = {}

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DIM = 666
# two row formats: v0 "bf_*.bin" = 4 labels [boss_win boss_hp hall_win hall_hp] + 4 ids (EXTRA 8);
# v1 "bg_*.bin" = 8 labels [.. boss_dmg next_win next_hp next_dmg] + 4 ids (EXTRA 12). O = number of heads.
HEAD_NAMES = ["boss_win", "boss_hp", "hall_win", "hall_hp", "boss_dmg", "next_win", "next_hp", "next_dmg"]
O = 4
EXTRA = 8


def load(data_dir):
    # data_dir may be a comma-separated list of directories (datasets from several ascensions)
    global O, EXTRA
    dirs = data_dir.split(",")
    v1 = sorted(f for d in dirs for f in glob.glob(os.path.join(d, "**", "bg_*.bin"), recursive=True))
    v0 = sorted(f for d in dirs for f in glob.glob(os.path.join(d, "**", "bf_*.bin"), recursive=True))
    if v1 and v0:
        raise SystemExit("mixed v0/v1 datasets are not supported")
    files = v1 or v0
    O, EXTRA = (8, 12) if v1 else (4, 8)
    arrs = []
    for fi, f in enumerate(files):  # per-file truncation to whole rows: a game killed mid-write must not shift later rows
        x = np.fromfile(f, dtype=np.float32)
        n = x.size // (DIM + EXTRA)
        if n:
            blk = x[: n * (DIM + EXTRA)].reshape(n, DIM + EXTRA).copy()
            # one file = one game: the run id is the file index. The stored id (seed-derived) collides when two
            # datasets reuse the same train seeds (bg1 + bg_heart), which cross-paired rows of different games
            blk[:, DIM + O] = fi
            arrs.append(blk)
    a = np.concatenate(arrs) if arrs else np.zeros((0, DIM + EXTRA), np.float32)
    a = a[np.isfinite(a).all(1)]
    X = a[:, :DIM]; Y = a[:, DIM:DIM + O]
    run = a[:, DIM + O].astype(np.int64)
    dec = a[:, DIM + O + 1].astype(np.int64)
    kind = a[:, DIM + O + 2].astype(np.int64); is_base = a[:, DIM + O + 3] > 0.5
    print(f"format {'v1' if v1 else 'v0'}: {len(files)} files, {O} heads")
    return X, Y, run, dec, kind, is_base


def export(path, W1, b1, W2, b2, trained=1):
    """model text file: 'dim h o trained' then W1[h*dim] B1[h] W2[o*h] B2[o] (research/vf/bf_mlp.h loads it)"""
    for arr in (W1, b1, W2, b2):
        if not np.isfinite(arr).all():
            raise SystemExit("refusing to export non-finite weights")
    H, D = W1.shape
    with open(path, "w") as f:
        f.write(f"{D} {H} {len(b2)} {trained}\n")
        for arr in (W1, b1, W2, b2):
            f.write(" ".join(f"{v:.6g}" for v in np.asarray(arr).ravel()) + "\n")


def placeholder(path, hidden):
    export(path, np.zeros((1, DIM), np.float32), np.zeros(1, np.float32), np.zeros((O, 1), np.float32),
           np.zeros(O, np.float32), trained=0)
    print("UNTRAINED placeholder written", path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data_dir")
    ap.add_argument("--out", required=True, help="model file, e.g. research/models/bf_g2.txt")
    ap.add_argument("--hidden", type=int, default=128)
    ap.add_argument("--epochs", type=int, default=30)
    ap.add_argument("--lam", type=float, default=2.0)
    ap.add_argument("--wd", type=float, default=1e-4)
    ap.add_argument("--dropout", type=float, default=0.1)
    ap.add_argument("--placeholder", action="store_true")
    a = ap.parse_args()
    if a.placeholder:
        placeholder(a.out, a.hidden); return

    import torch, torch.nn as nn, torch.nn.functional as F
    torch.manual_seed(0)
    X, Y, run, dec, kind, is_base = load(a.data_dir)
    n = len(Y)
    asc = X[:, 20] * 20.0
    print("ascension mix: " + str({a: int((np.round(asc) == a).sum()) for a in (0, 10, 20)}))
    print(f"rows={n} runs={len(np.unique(run))} decisions={len(np.unique(run * 100000 + dec))} "
          f"kinds(card/upg/rem)={[int((kind == k).sum()) for k in (0, 1, 2)]} base_rows={int(is_base.sum())}")
    print("label means: " + " ".join(f"{HEAD_NAMES[j]}={Y[Y[:, j] >= 0, j].mean():.3f}" for j in range(O)))
    # pairing: each hypothetical row -> its decision's base row
    key = run.astype(np.int64) * 100000 + dec
    base_of = {}
    for i in np.nonzero(is_base)[0]:
        base_of[key[i]] = i
    hyp_idx = np.array([i for i in range(n) if not is_base[i] and key[i] in base_of], dtype=np.int64)
    hyp_base = np.array([base_of[key[i]] for i in hyp_idx], dtype=np.int64)
    print(f"pairs={len(hyp_idx)}")
    assert int(is_base.sum()) == len(np.unique(key)), "base rows != decisions: run/decision keys collide"
    # run-grouped split
    rng = np.random.RandomState(0); ur = np.unique(run); rng.shuffle(ur)
    val_runs = set(ur[: max(1, len(ur) // 5)].tolist())
    vmask = np.array([r in val_runs for r in run])
    hv = vmask[hyp_idx]  # pair is validation iff its rows are
    Xt = torch.tensor(X); Yt = torch.tensor(Y)
    M = (Yt >= 0).float()  # per-head validity mask (-1 = fight not played)
    model = nn.Sequential(nn.Dropout(a.dropout), nn.Linear(DIM, a.hidden), nn.ReLU(), nn.Linear(a.hidden, O))
    opt = torch.optim.Adam(model.parameters(), lr=1e-3, weight_decay=a.wd)

    def abs_loss(idx):
        z = model(Xt[idx]); y = Yt[idx].clamp(0, 1); m = M[idx]
        return (F.binary_cross_entropy_with_logits(z, y, reduction="none") * m).sum(1).mean()

    def pair_loss(pi):  # pi indexes into hyp_idx
        h = torch.tensor(hyp_idx[pi]); b = torch.tensor(hyp_base[pi])
        dp = torch.sigmoid(model(Xt[h])) - torch.sigmoid(model(Xt[b]))
        do = Yt[h] - Yt[b]
        m = M[h] * M[b]
        return (((dp - do) ** 2) * m).sum(1).mean()

    def evaluate():
        model.eval()
        with torch.no_grad():
            iv = torch.tensor(np.nonzero(vmask)[0])
            z = model(Xt[iv]); y = Yt[iv]
            bce_boss = float(F.binary_cross_entropy_with_logits(z[:, 0], y[:, 0]))
            p = float(y[:, 0].mean()); base_bce = -(p * np.log(p + 1e-9) + (1 - p) * np.log(1 - p + 1e-9))
            hm = y[:, 2] >= 0
            bce_hall = float(F.binary_cross_entropy_with_logits(z[hm, 2], y[hm, 2])) if hm.any() else float("nan")
            ph = float(y[hm, 2].mean()) if hm.any() else 0.5
            base_hall = -(ph * np.log(ph + 1e-9) + (1 - ph) * np.log(1 - ph + 1e-9))
            pv = np.nonzero(hv)[0]
            h = torch.tensor(hyp_idx[pv]); b = torch.tensor(hyp_base[pv])
            DP = (torch.sigmoid(model(Xt[h])) - torch.sigmoid(model(Xt[b]))).numpy()
            DO = (Yt[h] - Yt[b]).numpy()
            dp, do = DP[:, 0], DO[:, 0]
            nz = do != 0
            corr = float(np.corrcoef(dp, do)[0, 1]) if len(dp) > 2 and dp.std() > 0 else float("nan")
            sign = float(((dp[nz] > 0) == (do[nz] > 0)).mean()) if nz.any() else float("nan")
            # continuous heads (g1): paired correlation of predicted vs observed damage-fraction differences
            global corr_dmg
            corr_dmg = {}
            for j in (4, 7):
                if j < O:
                    ok_ = (DO[:, j] > -1.5) & (Yt[h][:, j].numpy() >= 0) & (Yt[b][:, j].numpy() >= 0)
                    corr_dmg[HEAD_NAMES[j]] = float(np.corrcoef(DP[ok_, j], DO[ok_, j])[0, 1]) if ok_.sum() > 2 and DP[ok_, j].std() > 0 else float("nan")
        model.train()
        return bce_boss, base_bce, bce_hall, base_hall, corr, sign, int(nz.sum())

    tr_rows = np.nonzero(~vmask)[0]; tr_pairs = np.nonzero(~hv)[0]
    bs = 512; t0 = time.time(); best = (1e9, None, -1)
    for ep in range(a.epochs):
        perm = np.random.permutation(tr_rows); pperm = np.random.permutation(tr_pairs)
        nb = max(1, len(perm) // bs); pbs = max(1, len(pperm) // nb)
        for i in range(nb):
            idx = torch.tensor(perm[i * bs:(i + 1) * bs]); pi = pperm[i * pbs:(i + 1) * pbs]
            loss = abs_loss(idx) + (a.lam * pair_loss(pi) if len(pi) else 0)
            opt.zero_grad(); loss.backward(); opt.step()
        bce_boss, base_bce, bce_hall, base_hall, corr, sign, nnz = evaluate()
        cd = corr_dmg.get("next_dmg", corr_dmg.get("boss_dmg", float("nan")))
        rank_corr = cd if cd == cd else corr  # g1: select on the continuous head's paired correlation
        score = bce_boss - 0.5 * rank_corr  # model selection: calibrated and ranks options correctly
        extra = " ".join(f"pair_corr_{k}={v:.3f}" for k, v in corr_dmg.items())
        print(f"ep {ep+1:2d} val boss_bce={bce_boss:.4f} (base {base_bce:.4f}) hall_bce={bce_hall:.4f} (base {base_hall:.4f}) "
              f"pair_corr={corr:.3f} pair_sign_acc={sign:.3f} (n={nnz}) {extra} {time.time()-t0:.0f}s")
        if score < best[0]:
            best = (score, {k: v.detach().clone() for k, v in model.state_dict().items()}, ep + 1, bce_boss, base_bce, rank_corr)
    _, state, ep, bce_boss, base_bce, corr = best
    if state is None or bce_boss >= base_bce - 0.005 or not (corr > 0.02):
        print(f"model fails the gate (boss_bce {bce_boss:.4f} vs base {base_bce:.4f}, pair_corr {corr:.3f}); writing placeholder")
        placeholder(a.out, a.hidden); return
    model.load_state_dict(state)
    export(a.out, model[1].weight.detach().numpy(), model[1].bias.detach().numpy(),
           model[3].weight.detach().numpy(), model[3].bias.detach().numpy(), trained=1)
    print(f"exported {a.out} (epoch {ep}, boss_bce {bce_boss:.4f} vs base {base_bce:.4f}, pair_corr {corr:.3f})")


if __name__ == "__main__":
    main()
