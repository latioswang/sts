#!/usr/bin/env python3
"""Print the keep-chain and all experiments from research/results.tsv as markdown."""
import csv, os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
rows = list(csv.DictReader(open(os.path.join(ROOT, "results.tsv")), delimiter="\t"))
print("| # | split | status | win | floor | score | Δscore vs best | P(improve) | experiment |")
print("|---|---|---|---|---|---|---|---|---|")
for i, r in enumerate(rows, 1):
    print(f"| {i} | {r['split']} | {r['status']} | {float(r['winrate']):.1%} | {r['mean_floor']} | {r['score']} | "
          f"{r['dscore_vs_best'] or '—'} | {r['p_improve'] or '—'} | {r['description']} |")
