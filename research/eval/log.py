#!/usr/bin/env python3
"""Append a results.tsv row from run files. usage: log.py NAME SPLIT STATUS "desc" [--base BEST] [--asc A]"""
import argparse, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from run import load, ok, summary, paired, ROOT
ap = argparse.ArgumentParser()
ap.add_argument("name"); ap.add_argument("split"); ap.add_argument("status"); ap.add_argument("desc")
ap.add_argument("--base"); ap.add_argument("--asc", type=int, default=0)
ap.add_argument("--winmode", default="act3")
a = ap.parse_args()
f = lambda n: os.path.join(ROOT, "runs", f"{n}.{a.split}.a{a.asc}.tsv")
allr = load(f(a.name), a.winmode); r = ok(allr)
n, wr, fl, sc, se = summary(r)
d, p = "", ""
if a.base:
    _, dm, dw, pi = paired(r, ok(load(f(a.base), a.winmode))); d, p = f"{dm:+.4f}", f"{pi:.3f}"
commit = subprocess.run(["git", "rev-parse", "--short", "HEAD"], capture_output=True, text=True, cwd=ROOT).stdout.strip()
_simf = os.path.join(ROOT, "eval", "sim.txt")  # simulator tag (loop switched to the fidelity fork 2026-11-06); untagged rows = legacy
if os.path.exists(_simf) and open(_simf).read().strip() not in ("", "legacy"):
    a.desc = f"{a.desc} [sim={open(_simf).read().strip()}]"
row = [commit, f"{a.split}.a{a.asc}", str(n), str(len(allr) - n), f"{wr:.3f}", f"{fl:.2f}", f"{sc:.4f}", d, p, a.status, f"{a.name}: {a.desc}"]
open(os.environ.get("RESULTS_TSV") or os.path.join(ROOT, "results.tsv"), "a").write("\t".join(row) + "\n")  # workers log to research/results.d/<name>.tsv
print("\t".join(row))
