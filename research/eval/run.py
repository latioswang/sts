#!/usr/bin/env python3
"""FROZEN eval driver. Do not edit during the autoresearch loop.

usage:
  run.py NAME SPLIT [--agent fair|scum] [--asc A] [--param k=v ...] [--base NAME]
Writes research/runs/NAME.SPLIT.aA.tsv, prints summary, and if --base given,
a paired bootstrap comparison against research/runs/BASE.SPLIT.aA.tsv.
"""
import argparse, os, random, subprocess, sys, time, statistics as st

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.environ.get("FAIR_EVAL_BIN", "/home/user/sts_lightspeed/build/fair_eval")
SPLITS = {"dev": (100000, 100), "val": (200000, 300), "test": (900000, 500), "smoke": (100000, 20),
          "train": (300000, 500), "train2": (400000, 500), "train3": (500000, 500)}  # train*: data generation only, never for keep/discard
COLS = ["seed", "win", "floor", "act", "hp", "max_hp", "deck", "battles", "decisions", "aborted", "secs", "actions"]
REQUIRED = 11  # "actions" is an optional diagnostic column added in phase 2
MAX_FLOOR = 51  # A0 act-3 boss victory room


def load(path):
    rows = {}
    with open(path) as f:
        for line in f:
            p = line.split()
            if len(p) < REQUIRED:
                continue
            r = dict(zip(COLS, p))
            rows[int(r["seed"])] = {k: float(v) for k, v in r.items()}
    return rows


def ok(rows):
    """drop simulator crashes (aborted == 2); they are reported separately"""
    return {s: r for s, r in rows.items() if r["aborted"] != 2}


def score(r):
    return min(r["floor"], MAX_FLOOR) / MAX_FLOOR + r["win"]


def summary(rows):
    n = len(rows)
    w = sum(r["win"] for r in rows.values())
    fl = st.mean(r["floor"] for r in rows.values())
    sc = st.mean(score(r) for r in rows.values())
    se = st.pstdev([score(r) for r in rows.values()]) / n ** 0.5
    return n, w / n, fl, sc, se


def paired(a, b, iters=4000, seed=0):
    seeds = sorted(set(a) & set(b))
    d = [score(a[s]) - score(b[s]) for s in seeds]
    dw = [a[s]["win"] - b[s]["win"] for s in seeds]
    rng = random.Random(seed)
    n = len(d)
    boots = []
    for _ in range(iters):
        boots.append(sum(d[rng.randrange(n)] for _ in range(n)) / n)
    p_imp = sum(x > 0 for x in boots) / iters
    return n, st.mean(d), st.mean(dw), p_imp


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("name"); ap.add_argument("split")
    ap.add_argument("--agent", default="fair"); ap.add_argument("--asc", type=int, default=0)
    ap.add_argument("--param", action="append", default=[]); ap.add_argument("--base")
    ap.add_argument("--threads", type=int, default=4)
    a = ap.parse_args()
    start, count = SPLITS[a.split]
    os.makedirs(os.path.join(ROOT, "runs"), exist_ok=True)
    out = os.path.join(ROOT, "runs", f"{a.name}.{a.split}.a{a.asc}.tsv")
    cmd = [BIN, "--agent", a.agent, "--seed_start", str(start), "--count", str(count),
           "--asc", str(a.asc), "--threads", str(a.threads)]
    for kv in a.param:
        cmd += ["--param", kv]
    t0 = time.time()
    tmpdir = os.path.join(ROOT, "runs", ".inprogress")  # gitignored; moved into place when complete
    os.makedirs(tmpdir, exist_ok=True)
    tmp = os.path.join(tmpdir, os.path.basename(out))
    with open(tmp, "w") as f:
        subprocess.run(cmd, stdout=f, check=True)
    os.replace(tmp, out)
    allrows = load(out)
    rows = ok(allrows)
    crashes = len(allrows) - len(rows)
    n, wr, fl, sc, se = summary(rows)
    act4 = sum(1 for r in rows.values() if r["act"] >= 4) / max(1, n)
    print(f"{a.name}\t{a.split}\ta{a.asc}\tn={n}\tcrash={crashes}\twin={wr:.3f}\tfloor={fl:.2f}\tscore={sc:.4f}±{se:.4f}\tact4={act4:.3f}\t{time.time()-t0:.0f}s")
    if a.base:
        bpath = os.path.join(ROOT, "runs", f"{a.base}.{a.split}.a{a.asc}.tsv")
        n, dmean, dwin, p = paired(rows, ok(load(bpath)))
        print(f"vs {a.base}: n={n}\tdscore={dmean:+.4f}\tdwin={dwin:+.3f}\tP(improve)={p:.3f}")


if __name__ == "__main__":
    main()
