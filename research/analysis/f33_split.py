#!/usr/bin/env python3
"""Phase F33: split-half decision analysis (eval_main --branch_eval K M --branch_kind 1|2).
Each split_<seed>.bin holds one decision: per option 666 features + [opt, n, mean, ok, floor, own, meanA, meanB], where
meanA / meanB are the playout means over m < M/2 and m >= M/2 (independent halves, same seeds for every option).
Every pick is SCORED ON THE HALF IT WAS NOT CHOSEN ON, so the oracle is honest:
  oracle_honest = pick argmax meanA, score meanB (and the mirror; averaged) = the value of choosing by M/2 playouts
  oracle_insample = max meanA (the biased F26-style oracle, for comparison); its excess over oracle_honest = selection noise
Models / own / random are scored on the mean of both halves' scores of their pick ((meanA + meanB) / 2 = mean when ok
splits evenly) — they do not look at either half — and also on meanB alone for the like-for-like comparison with the
honest oracle. Paired differences vs the agent's own pick, with paired SEs.
usage: f33_split.py DIR [DIR ...] [MODEL_ID ...]   (arguments that are integers are model ids)"""
import glob, os, sys
import numpy as np
Ds = [x for x in sys.argv[1:] if not x.isdigit()]; ids = [int(x) for x in sys.argv[1:] if x.isdigit()]
W = 666 + 8
decs = []
for f in sorted(f for D in Ds for f in glob.glob(os.path.join(D, "split_*.bin"))):
    a = np.fromfile(f, dtype=np.float32)
    if a.size == 0 or a.size % W: continue
    a = a.reshape(-1, W)
    if (a[:, 672] < 0).any() or (a[:, 673] < 0).any() or len(a) < 2: continue   # an option with an empty half
    if not (a[:, 671] > 0.5).any(): continue
    decs.append(a)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
def load(i):
    tok = open(f"research/models/vr_{i}.txt").read().split(); md, nl = int(tok[0]), int(tok[1]); p = 3; L = []
    for _ in range(nl):
        a, b = int(tok[p]), int(tok[p + 1]); p += 2
        Wm = np.array(tok[p:p + a * b], float).reshape(b, a); p += a * b; bb = np.array(tok[p:p + b], float); p += b; L.append((Wm, bb))
    return md, L
def V(model, x):
    md, L = model
    h = x[:, :666].astype(np.float64)
    if md > 666: h = np.hstack([h, np.zeros((len(h), md - 666))])
    for l, (Wm, b) in enumerate(L):
        h = h @ Wm.T + b
        if l + 1 < len(L): h = np.maximum(h, 0)
    return h[:, 0]
n = len(decs)
if n == 0: print("F33 decisions=0"); sys.exit(0)
A = [d[:, 672].astype(np.float64) for d in decs]; B = [d[:, 673].astype(np.float64) for d in decs]
own = [int(np.argmax(d[:, 671])) for d in decs]
print(f"F33 decisions={n} options={sum(len(d) for d in decs)} mean_options={np.mean([len(d) for d in decs]):.2f} "
      f"floors {np.percentile([d[0, 670] for d in decs], [10, 50, 90]).round(0).tolist()}")
both = lambda pick: np.array([(a[k] + b[k]) / 2 for a, b, k in zip(A, B, pick)])
onB = lambda pick: np.array([b[k] for b, k in zip(B, pick)])
rows = {}
rows["own"] = (both(own), onB(own))
rows["random"] = (np.array([(a.mean() + b.mean()) / 2 for a, b in zip(A, B)]), np.array([b.mean() for b in B]))
for i in ids:
    m = load(i); pk = [int(np.argmax(V(m, d))) for d in decs]
    rows[f"vr_{i}"] = (both(pk), onB(pk))
    print(f"F33 vr_{i} picks the agent's own option in {np.mean([p == o for p, o in zip(pk, own)]):.3f} of decisions")
hon = np.array([(b[int(np.argmax(a))] + a[int(np.argmax(b))]) / 2 for a, b in zip(A, B)])
hon_b = np.array([b[int(np.argmax(a))] for a, b in zip(A, B)])
ins = np.array([a.max() for a in A])
def pr(name, v, ref):
    dv = v - ref
    print(f"F33 {name:16s} value={v.mean():.4f}  vs own: {dv.mean():+.4f} ± {dv.std(ddof=1) / np.sqrt(len(dv)):.4f} (paired SE, n={len(dv)})")
for k, (vb, vB) in rows.items(): pr(k, vb, rows["own"][0])
pr("oracle_honest", hon, rows["own"][0])
print("F33 -- like-for-like on half B only (honest oracle picks on A) --")
for k, (vb, vB) in rows.items(): pr(k + "@B", vB, rows["own"][1])
pr("oracle_honest@B", hon_b, rows["own"][1])
pr("oracle_insample", ins, rows["own"][1])
agree = np.mean([int(np.argmax(a)) == int(np.argmax(b)) for a, b in zip(A, B)])
print(f"F33 half-A and half-B pick the same option in {agree:.3f} of decisions; own = argmax A in "
      f"{np.mean([int(np.argmax(a)) == o for a, o in zip(A, own)]):.3f}")
