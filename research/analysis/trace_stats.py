#!/usr/bin/env python3
"""Replay audit (F42 b): per-game summaries from FAIR_TRACE logs, and losses vs wins compared at the same point of the run.
usage: trace_stats.py DIR [--detail SEED]
For each game: result, death fight, HP entering it, deck size / composition at the act-2 boss (floor 33) or at death,
removals, upgrades, relics, potions and gold carried into the death fight, HP lost per fight by act."""
import glob, os, re, sys
from collections import Counter, defaultdict

START = Counter({"Strike": 5, "Defend": 4, "Bash": 1})

def parse(path):
    g = dict(seed=int(re.search(r"game_(\d+)", path).group(1)), fights=[], deck=Counter(START), relics=[], potions=[], removals=0,
             upgrades=0, shops=0, shop_gold_spent=0, rests=Counter(), snaps={})
    cur = None; screen = None; last_state = None
    for l in open(path):
        l = l.rstrip("\n")
        m = re.match(r"  \[(\w+)\] F(\d+) A(\d+) hp (\d+)/(\d+) gold (\d+) offers:(.*)", l)
        if m:
            screen = m[1]; last_state = dict(floor=int(m[2]), act=int(m[3]), hp=int(m[4]), maxhp=int(m[5]), gold=int(m[6]))
            continue
        if l.startswith("    = "):
            d = l[6:]
            for c in re.findall(r"\+card:([^ ]+(?: [^ +:-][^ ]*)*?)(?=(?: [+-]card:| \+relic:| potions:| gold| hp| maxhp| ->|$))", d):
                k = re.sub(r"x(\d+)$", "", c); g["deck"][k] += 1
            for c in re.findall(r"-card:([^ ]+(?: [^ +:-][^ ]*)*?)(?=(?: [+-]card:| \+relic:| potions:| gold| hp| maxhp| ->|$))", d):
                k = re.sub(r"x(\d+)$", "", c); g["deck"][k] -= 1
                if screen == "CARD_SELECT" and "+card:" not in d: g["removals"] += 1
            if "+card:" in d and "-card:" in d and screen == "CARD_SELECT": g["upgrades"] += 1
            for r in re.findall(r"\+relic:([A-Za-z' ]+?)(?= \+| potions| gold| hp| maxhp| ->|$)", d): g["relics"].append(r.strip())
            pm = re.search(r"potions:\[([^\]]*)\]", d)
            if pm: g["potions"] = [p for p in pm[1].split(",") if p]
            gm = re.search(r"gold(-\d+)", d)
            if gm and screen == "SHOP": g["shop_gold_spent"] += -int(gm[1])
            if screen == "REST":
                g["rests"]["rest (heal)" if " hp+" in d else "smith or nothing"] += 1
            continue
        m = re.match(r"  FIGHT (.+?) \| F(\d+) A(\d+) hp (\d+)/(\d+) gold (\d+)", l)
        if m:
            cur = dict(enc=m[1], floor=int(m[2]), act=int(m[3]), hp0=int(m[4]), max=int(m[5]), gold=int(m[6]),
                       deck=sum(v for v in g["deck"].values() if v > 0), potions=list(g["potions"]), turns=0)
            if cur["floor"] in (16, 33, 50) and cur["floor"] not in g["snaps"]:
                g["snaps"][cur["floor"]] = dict(deck=Counter({k: v for k, v in g["deck"].items() if v > 0}), relics=len(g["relics"]),
                                               potions=list(g["potions"]), gold=cur["gold"], hp=cur["hp0"], max=cur["max"])
            continue
        m = re.match(r"  END FIGHT turn (\d+) hp (\d+)/(\d+)( DIED)?", l)
        if m and cur:
            cur.update(turns=int(m[1]) + 1, hp1=int(m[2]), died=bool(m[4])); g["fights"].append(cur); cur = None
            continue
        m = re.match(r"RESULT (\w+) floor (\d+)", l)
        if m: g["win"] = m[1] == "WIN"; g["floor"] = int(m[2])
    g["death"] = next((f for f in g["fights"] if f.get("died")), None)
    return g

def deckstats(dk):
    n = sum(dk.values())
    s = sum(v for k, v in dk.items() if k.startswith("Strike")); d = sum(v for k, v in dk.items() if k.startswith("Defend"))
    up = sum(v for k, v in dk.items() if k.endswith("+"))
    return n, s, d, up

def main():
    D = sys.argv[1]
    G = [parse(f) for f in sorted(glob.glob(os.path.join(D, "game_*.txt")))]
    W = [g for g in G if g.get("win")]; L = [g for g in G if not g.get("win")]
    print(f"games {len(G)}: wins {len(W)}, losses {len(L)}")
    print("\nLosses by death fight (HP entering / max, turns, potions carried, gold):")
    by = defaultdict(list)
    for g in L:
        if g["death"]: by[g["death"]["enc"]].append(g)
    for enc, gs in sorted(by.items(), key=lambda kv: -len(kv[1])):
        hp = sum(g["death"]["hp0"] / g["death"]["max"] for g in gs) / len(gs)
        pot = sum(len(g["death"]["potions"]) for g in gs) / len(gs)
        gold = sum(g["death"]["gold"] for g in gs) / len(gs); tu = sum(g["death"]["turns"] for g in gs) / len(gs)
        print(f"  {enc:24s} n={len(gs):3d}  hp in {hp:.0%}  turns {tu:.1f}  potions carried {pot:.2f}  gold {gold:.0f}")
    print("\nAt the act-2 boss (floor 33): runs that died there vs runs that won the game")
    for name, gs in (("died at F33", [g for g in L if g["death"] and g["death"]["floor"] == 33]), ("won the game", W)):
        S = [g["snaps"][33] for g in gs if 33 in g["snaps"]]
        if not S: continue
        n = len(S); ds = [deckstats(s["deck"]) for s in S]
        print(f"  {name:13s} n={n:3d}  deck {sum(x[0] for x in ds)/n:.1f} cards (Strikes {sum(x[1] for x in ds)/n:.1f}, Defends {sum(x[2] for x in ds)/n:.1f}, upgraded {sum(x[3] for x in ds)/n:.1f})"
              f"  relics {sum(s['relics'] for s in S)/n:.1f}  potions {sum(len(s['potions']) for s in S)/n:.2f}  gold {sum(s['gold'] for s in S)/n:.0f}  hp in {sum(s['hp']/s['max'] for s in S)/n:.0%}")
    for name, gs in (("losses", L), ("wins", W)):
        if not gs: continue
        n = len(gs)
        print(f"  {name:6s}: removals {sum(g['removals'] for g in gs)/n:.2f}/run, shop gold spent {sum(g['shop_gold_spent'] for g in gs)/n:.0f}/run, rests {dict(sum((g['rests'] for g in gs), Counter()))}")
    print("\nCards in decks at F33 — most over-represented in runs that died there (vs runs that won), min 8 decks:")
    dl = [g["snaps"][33]["deck"] for g in L if g["death"] and g["death"]["floor"] == 33 and 33 in g["snaps"]]
    dw = [g["snaps"][33]["deck"] for g in W if 33 in g["snaps"]]
    if dl and dw:
        def frac(decks, c): return sum(1 for d in decks if any(k.rstrip("+") == c for k in d)) / len(decks)
        names = {k.rstrip("+") for d in dl + dw for k in d}
        rows = [(frac(dl, c) - frac(dw, c), c, frac(dl, c), frac(dw, c)) for c in names
                if sum(1 for d in dl + dw if any(k.rstrip("+") == c for k in d)) >= 8]
        for x in sorted(rows)[-8:][::-1]: print(f"    {x[1]:22s} in {x[2]:.0%} of F33 deaths vs {x[3]:.0%} of wins")
        print("  ... and most under-represented:")
        for x in sorted(rows)[:8]: print(f"    {x[1]:22s} in {x[2]:.0%} of F33 deaths vs {x[3]:.0%} of wins")

if __name__ == "__main__":
    main()
