#!/usr/bin/env python3
"""Stage 0 / E3 (research/breakthrough/PLAN.md section 3): a per-card value table built from the fork (branch) data
already on disk, scored on held-out decisions against the agent's own card-reward picks.

Data (read-only):
  branch_<seed>.bin  (FairAgent::branchEval, F26/F28/F31): one card-reward decision per file; per option one row of
      rv::DIM=666 run features of the state AFTER the option + tail [o, n, mean, ok, floor, own]; options 0..n-1 = the
      offered cards, option n = skip; mean = mean run score (min(floor,51)/51 + win) over M=8 playouts on shared seeds.
  split_<seed>.bin   (FairAgent::branchEval2 kind 1, F33): same, tail [o, n, mean, ok, floor, own, meanA, meanB] with
      M=16 and meanA / meanB the means over playouts m<8 / m>=8 (independent halves).
Option identity: the card-count block of the features (offset 24+66+192=282, 384 CardIds, +0.34 per copy, +0.5 per
upgraded copy) of option o minus that of the skip row = exactly the taken card (and its upgraded flag).
Split: by seed, held-out iff zlib.crc32(str(seed).encode()) % 5 == 0; tables are built on train seeds only.
Pickers on held-out decisions: own (the flagged option), table argmax (ties -> own, then lowest index), random (mean of
options, context), oracle. Branch files carry only the M=8 mean, so there the table and own are scored on the full mean
(neither looks at any held-out playout) and only the biased in-sample oracle (max of means) exists; the F33 split files
(seeds disjoint from all branch seeds) give the honest split-half oracle (pick on half A, score on half B, and mirror).
usage: nice -n 10 python3 research/analysis/s0_e3_card_table.py   (paths are fixed below; run from /home/user/sts)"""
import glob, os, re, sys, zlib
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "vf", "data")
DIM, CARD0, NCARD = 666, 24 + 66 + 192, 384
TRACKS = {
    "A20": [f"branch_f28_w{i}" for i in range(7)],
    "A10": [f"branch_f31_w{i}" for i in range(1, 9)] + ["branch_f31_w7b", "branch_f31_hold"],
}
SPLIT = {"A20": ["f33_a20card_local"], "A10": ["f33_a10card_w1", "f33_a10card_w2"]}
SKIP = -1


def card_names():
    src = "/home/user/sts_lightspeed_fid/include/constants/Cards.h"
    txt = open(src).read()
    body = txt[txt.index("enum class CardId"):]
    body = body[body.index("{") + 1:body.index("}")]
    names = [re.sub(r"=.*", "", t).strip() for t in body.split(",")]
    return [n for n in names if n]


NAMES = card_names()


def nm(k):
    if k == SKIP: return "SKIP"
    return NAMES[k] if 0 <= k < len(NAMES) else f"id{k}"


def heldout(seed): return zlib.crc32(str(seed).encode()) % 5 == 0


STATS = {"files": 0, "empty": 0, "badsize": 0, "invalid": 0, "anomaly": 0, "kept": 0}


def load(dirs, split):
    W = DIM + (8 if split else 6)
    pat = "split_*.bin" if split else "branch_*.bin"
    out, seen = [], set()
    for d in dirs:
        for f in sorted(glob.glob(os.path.join(DATA, d, pat))):
            STATS["files"] += 1
            seed = int(re.search(r"_(\d+)\.bin$", f).group(1))
            a = np.fromfile(f, dtype=np.float32)
            if a.size == 0: STATS["empty"] += 1; continue
            if a.size % W: STATS["badsize"] += 1; continue
            a = a.reshape(-1, W)
            t = a[:, DIM:]
            if (t[:, 2] < 0).any() or (t[:, 3] < 1).any() or len(a) < 2: STATS["invalid"] += 1; continue
            if split and ((t[:, 6] < 0).any() or (t[:, 7] < 0).any()): STATS["invalid"] += 1; continue
            if not (t[:, 5] > 0.5).any(): STATS["invalid"] += 1; continue
            n = int(t[0, 1])
            if len(a) != n + 1 or int(t[-1, 0]) != n: STATS["anomaly"] += 1; continue
            base = a[n, CARD0:CARD0 + NCARD]
            keys, upg, bad = [], [], False
            for o in range(n):
                dd = a[o, CARD0:CARD0 + NCARD] - base
                nz = np.where(np.abs(dd) > 1e-3)[0]
                if len(nz) != 1: bad = True; break
                v = dd[nz[0]]
                if abs(v - 0.34) < 1e-3: u = 0
                elif abs(v - 0.5) < 1e-3: u = 1
                else: bad = True; break
                keys.append(int(nz[0])); upg.append(u)
            if bad: STATS["anomaly"] += 1; continue
            if seed in seen: STATS["anomaly"] += 1; continue
            seen.add(seed)
            keys.append(SKIP); upg.append(0)
            STATS["kept"] += 1
            out.append(dict(seed=seed, hold=heldout(seed), floor=int(t[0, 4]), act=int(round(a[n, 1] * 3)),
                            asc=int(round(a[n, 20] * 20)), keys=np.array(keys), upg=np.array(upg),
                            y=t[:, 2].astype(np.float64), own=int(np.argmax(t[:, 5])),
                            yA=t[:, 6].astype(np.float64) if split else None,
                            yB=t[:, 7].astype(np.float64) if split else None))
    return out


# ---------- tables (built on train decisions only) ----------
def eb(groups, prior):
    """groups: key -> array of advantages; prior: key -> prior mean. Returns key -> shrunk mean, tau2, s2.
    Normal-normal empirical Bayes by moments: s2 = pooled within-key variance, tau2 = mean((m_k - prior_k)^2 - s2/n_k)
    over keys with n_k >= 5, floored at 0; estimate = prior + (m_k - prior) * tau2 / (tau2 + s2 / n_k)."""
    ks = list(groups)
    ss = sum(((g - g.mean()) ** 2).sum() for g in groups.values()); df = sum(len(g) - 1 for g in groups.values())
    s2 = ss / max(df, 1)
    dev = [(groups[k].mean() - prior(k)) ** 2 - s2 / len(groups[k]) for k in ks if len(groups[k]) >= 5]
    tau2 = max(0.0, float(np.mean(dev))) if dev else 0.0
    est = {}
    for k in ks:
        g = groups[k]; p = prior(k)
        est[k] = p + (g.mean() - p) * (tau2 / (tau2 + s2 / len(g)) if tau2 > 0 else 0.0)
    return est, tau2, s2


def build(train):
    rows = []  # (card, act, upg, adv_mean, adv_skip)
    for d in train:
        y = d["y"]; m = y.mean(); ysk = y[-1]
        for k, u, v in zip(d["keys"], d["upg"], y):
            rows.append((int(k), d["act"], int(u), v - m, v - ysk))
    rows_by = lambda f, col: {}
    T = {}
    # (a) card id, advantage over the decision mean, shrunk toward 0
    g = {}
    for k, a, u, am, sk in rows: g.setdefault(k, []).append(am)
    g = {k: np.array(v) for k, v in g.items()}
    card, tau, s2 = eb(g, lambda k: 0.0)
    T["card"] = (lambda k, a, u: card.get(k, 0.0)); T["_card"] = (card, g, tau, s2)
    # (a') card x act, shrunk toward the card-level estimate
    ga = {}
    for k, a, u, am, sk in rows: ga.setdefault((k, a), []).append(am)
    ga = {k: np.array(v) for k, v in ga.items()}
    cact, tau_a, _ = eb(ga, lambda ka: card.get(ka[0], 0.0))
    T["card_act"] = (lambda k, a, u: cact.get((k, a), card.get(k, 0.0))); T["_act"] = tau_a
    # (b) card x upgraded flag, shrunk toward the card-level estimate
    gu = {}
    for k, a, u, am, sk in rows: gu.setdefault((k, u), []).append(am)
    gu = {k: np.array(v) for k, v in gu.items()}
    cupg, tau_u, _ = eb(gu, lambda ku: card.get(ku[0], 0.0))
    T["card_upg"] = (lambda k, a, u: cupg.get((k, u), card.get(k, 0.0))); T["_upg"] = (tau_u, gu, cupg)
    # (c) card id, advantage over SKIP in the same decision, shrunk toward 0; skip = 0 by definition
    gs = {}
    for k, a, u, am, sk in rows:
        if k != SKIP: gs.setdefault(k, []).append(sk)
    gs = {k: np.array(v) for k, v in gs.items()}
    vsk, tau_s, _ = eb(gs, lambda k: 0.0)
    T["card_vs_skip"] = (lambda k, a, u: 0.0 if k == SKIP else vsk.get(k, 0.0)); T["_skip"] = tau_s
    # (d) fixed prior count 20 (no EB), card level, for robustness
    fx = {k: v.sum() / (len(v) + 20.0) for k, v in g.items()}
    T["card_k20"] = (lambda k, a, u: fx.get(k, 0.0))
    return T


def pick(d, f):
    v = np.array([f(int(k), d["act"], int(u)) for k, u in zip(d["keys"], d["upg"])])
    best = v.max(); cand = np.where(v >= best - 1e-12)[0]
    return d["own"] if d["own"] in cand else int(cand[0])


def paired(v, ref):
    dv = v - ref
    return dv.mean(), dv.std(ddof=1) / np.sqrt(len(dv)), len(dv)


PICKERS = ["card", "card_act", "card_upg", "card_vs_skip", "card_k20"]


def evaluate(test, T, split, out):
    own_i = [d["own"] for d in test]
    if not split:
        Y = [d["y"] for d in test]
        own = np.array([y[i] for y, i in zip(Y, own_i)])
        out.append(f"  own value={own.mean():.4f} (n={len(own)})")
        res = {}
        for p in PICKERS:
            pk = [pick(d, T[p]) for d in test]
            v = np.array([y[i] for y, i in zip(Y, pk)])
            m, se, n = paired(v, own); res[p] = (m, se)
            out.append(f"  table[{p:12s}] vs own: {m:+.4f} ± {se:.4f}  ({m / se if se > 0 else 0:+.2f} SE)  "
                       f"agrees with own {np.mean([a == b for a, b in zip(pk, own_i)]):.3f}")
        r = np.array([y.mean() for y in Y]); m, se, n = paired(r, own)
        out.append(f"  random              vs own: {m:+.4f} ± {se:.4f}")
        o = np.array([y.max() for y in Y]); m, se, n = paired(o, own)
        out.append(f"  oracle_insample     vs own: {m:+.4f} ± {se:.4f}   (BIASED upward: picks and scores on the same 8 playouts)")
        return res
    A = [d["yA"] for d in test]; B = [d["yB"] for d in test]
    both = lambda pk: np.array([(a[k] + b[k]) / 2 for a, b, k in zip(A, B, pk)])
    onB = lambda pk: np.array([b[k] for b, k in zip(B, pk)])
    own_b, own_B = both(own_i), onB(own_i)
    out.append(f"  own value(A+B)/2={own_b.mean():.4f}  own@B={own_B.mean():.4f} (n={len(own_b)})")
    res = {}
    for p in PICKERS:
        pk = [pick(d, T[p]) for d in test]
        m, se, n = paired(both(pk), own_b); m2, se2, _ = paired(onB(pk), own_B); res[p] = (m, se)
        out.append(f"  table[{p:12s}] vs own: {m:+.4f} ± {se:.4f} ({m / se if se > 0 else 0:+.2f} SE)   @B: {m2:+.4f} ± {se2:.4f}  "
                   f"agrees with own {np.mean([a == b for a, b in zip(pk, own_i)]):.3f}")
    m, se, _ = paired(np.array([(a.mean() + b.mean()) / 2 for a, b in zip(A, B)]), own_b)
    out.append(f"  random              vs own: {m:+.4f} ± {se:.4f}")
    hon = np.array([(b[int(np.argmax(a))] + a[int(np.argmax(b))]) / 2 for a, b in zip(A, B)])
    m, se, _ = paired(hon, own_b); hb = np.array([b[int(np.argmax(a))] for a, b in zip(A, B)]); m2, se2, _ = paired(hb, own_B)
    out.append(f"  oracle_honest       vs own: {m:+.4f} ± {se:.4f}   @B: {m2:+.4f} ± {se2:.4f}   (pick on one half, score on the other)")
    ins = np.array([a.max() for a in A]); m, se, _ = paired(ins, own_B)
    out.append(f"  oracle_insample@A   vs own@B: {m:+.4f} ± {se:.4f}   (biased, for reference)")
    return res


def main():
    out = []
    pr = lambda s="": out.append(s)
    summary = {}
    for tr in ["A20", "A10"]:
        decs = load(TRACKS[tr], False)
        tr_, ho = [d for d in decs if not d["hold"]], [d for d in decs if d["hold"]]
        nopt = np.array([len(d["y"]) for d in decs])
        pr(f"== {tr}  branch data: dirs {', '.join(TRACKS[tr])}")
        pr(f"  decisions={len(decs)} train={len(tr_)} held-out={len(ho)}  options/decision mean={nopt.mean():.3f} "
           f"dist={dict(zip(*np.unique(nopt, return_counts=True)))}  asc={sorted(set(d['asc'] for d in decs))}")
        acts = dict(zip(*np.unique([d["act"] for d in decs], return_counts=True)))
        pr(f"  decisions by act={acts}  floor p10/50/90={np.percentile([d['floor'] for d in decs], [10, 50, 90]).tolist()}  "
           f"own=skip in {np.mean([d['keys'][d['own']] == SKIP for d in decs]):.3f}  upgraded offers={np.mean(np.concatenate([d['upg'][:-1] for d in decs])):.3f}")
        T = build(tr_)
        card, g, tau, s2 = T["_card"]
        pr(f"  EB card level: keys={len(card)} tau={np.sqrt(tau):.4f} s(per option)={np.sqrt(s2):.4f}; act tau={np.sqrt(T['_act']):.4f}; "
           f"upg tau={np.sqrt(T['_upg'][0]):.4f}; vs-skip tau={np.sqrt(T['_skip']):.4f}")
        top = sorted(card, key=lambda k: -card[k])
        fmt = lambda k: f"{nm(k)} {card[k]:+.4f} (n={len(g[k])}, raw {g[k].mean():+.4f})"
        pr("  top 12: " + "; ".join(fmt(k) for k in top[:12]))
        pr("  bottom 8: " + "; ".join(fmt(k) for k in top[-8:]))
        pr(f"  SKIP: {fmt(SKIP)}")
        pk = [pick(d, T["card"]) for d in tr_]
        m, se, _ = paired(np.array([d["y"][k] for d, k in zip(tr_, pk)]), np.array([d["y"][d["own"]] for d in tr_]))
        pr(f"  in-sample (train decisions, table fitted on them; biased up): table[card] vs own {m:+.4f} ± {se:.4f}")
        pr(f"  -- held-out branch decisions (crc32(seed)%5==0), scored on the full M=8 mean (no picker sees these playouts) --")
        res = evaluate(ho, T, False, out)
        pr(f"  -- F33 split-half card decisions ({', '.join(SPLIT[tr])}; seeds disjoint from all branch seeds), M=16 --")
        sp = load(SPLIT[tr], True)
        pr(f"  decisions={len(sp)} options/decision mean={np.mean([len(d['y']) for d in sp]):.3f} "
           f"(crc32-held-out subset {sum(d['hold'] for d in sp)})")
        res2 = evaluate(sp, T, True, out)
        summary[tr] = (res, res2, len(ho), len(sp))
        pr()
    pr("== Verdict (pre-registered: table must beat own by >= 2 SE on either track; primary = table[card])")
    for tr, (r1, r2, n1, n2) in summary.items():
        for lab, r in (("branch held-out", r1), ("F33 split", r2)):
            m, se = r["card"]
            best = max(PICKERS, key=lambda p: r[p][0] / r[p][1] if r[p][1] > 0 else -9)
            pr(f"  {tr} {lab:15s}: table[card] {m:+.4f} ± {se:.4f} ({m / se:+.2f} SE) -> {'BEATS' if m >= 2 * se else 'does not beat'} own; "
               f"best variant {best} {r[best][0]:+.4f} ± {r[best][1]:.4f} ({r[best][0] / r[best][1]:+.2f} SE)")
    pr(f"files={STATS}")
    print("\n".join(out))


if __name__ == "__main__":
    main()
