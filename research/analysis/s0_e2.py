#!/usr/bin/env python3
"""SOTA stage 0, E2 analysis: boss-arrival re-fights (research/s0/e2/refight_*.tsv).
Pre-registered reading (PLAN.md rev 2 §3): if the top quartile of arrival states wins < 76% at the act-2 boss
(floor 33) with the fair pilot, no drafting policy reaches 25% A20 with this pilot (76% is the act-2-boss pass rate T1
needs on average [computed in PLAN_redteam / pA_review from d1 §2]). Quartiles by entry HP fraction and by deck proxy."""
import glob, statistics as st, sys
rows = []
for f in glob.glob(sys.argv[1] if len(sys.argv) > 1 else "research/s0/e2/refight_*.tsv"):
    for line in open(f):
        p = line.split("\t")
        if len(p) < 14: continue
        seed, floor, enc, hp, mhp, deck, pots, rel = map(int, p[:8])
        nF, wF, hF, nC, wC, hC = float(p[8]), float(p[9]), float(p[10]), float(p[11]), float(p[12]), float(p[13])
        rows.append(dict(seed=seed, floor=floor, hpf=hp / max(1, mhp), deck=deck, pots=pots, rel=rel,
                         fair=wF / nF if nF else None, clair=wC / nC if nC else None, nC=nC))
print(f"arrivals: {len(rows)} from {len({r['seed'] for r in rows})} games")
def summ(sub, label):
    f = [r["fair"] for r in sub if r["fair"] is not None]; c = [r["clair"] for r in sub if r["clair"] is not None]
    cn = sum(r["nC"] for r in sub)
    print(f"  {label:34s} n={len(sub):4d}  fair re-fight win {st.mean(f) if f else float('nan'):.3f}  "
          f"clairvoyant {st.mean(c) if c else float('nan'):.3f} (fights {cn:.0f})")
for fl, name in ((16, "act-1 boss"), (33, "act-2 boss"), (50, "act-3 boss 1"), (51, "act-3 boss 2")):
    sub = [r for r in rows if r["floor"] == fl]
    if not sub: continue
    print(f"floor {fl} ({name})")
    summ(sub, "all arrivals")
    for key, lab in (("hpf", "entry HP fraction"), ("rel", "relic count"), ("fair", "fair re-fight win rate (in-sample)")):
        s = sorted(sub, key=lambda r: r[key]); q = len(s) // 4
        if q: summ(s[-q:], f"top quartile by {lab}"); summ(s[:q], f"bottom quartile by {lab}")
a2 = [r for r in rows if r["floor"] == 33]
if a2:
    s = sorted(a2, key=lambda r: r["hpf"]); top = s[-(len(s) // 4):]
    v = st.mean(r["fair"] for r in top)
    print(f"\nPRE-REGISTERED E2 READING (act-2 boss, top quartile by entry HP, fair pilot): {v:.3f} "
          f"{'< 0.76 -> no drafting policy reaches 25% with this pilot' if v < 0.76 else '>= 0.76 -> the pilot can carry strong arrivals'}")
