#!/usr/bin/env python3
"""Replay audit page (F42 b): embeds FAIR_TRACE game logs and the loss/win statistics into one HTML page.
usage: build_audit.py OUT.html STATS_DIR GAME_DIR [GAME_DIR ...]
STATS_DIR: traces used for the summary tables; GAME_DIRs: traces embedded for step-by-step viewing."""
import glob, html, json, os, re, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "analysis"))
from trace_stats import parse, deckstats  # noqa: E402

out, stats_dir, game_dirs = sys.argv[1], sys.argv[2], sys.argv[3:]
R = os.path.join(os.path.dirname(__file__), "..")

# ---- statistics over the sample
G = [parse(f) for f in sorted(glob.glob(os.path.join(stats_dir, "game_*.txt")))]
W = [g for g in G if g.get("win")]; L = [g for g in G if not g.get("win")]
by = {}
for g in L:
    if g["death"]: by.setdefault(g["death"]["enc"], []).append(g)
deaths = []
for enc, gs in sorted(by.items(), key=lambda kv: -len(kv[1]))[:12]:
    deaths.append(dict(enc=enc, n=len(gs), floor=gs[0]["death"]["floor"],
                       hp=round(100 * sum(g["death"]["hp0"] / g["death"]["max"] for g in gs) / len(gs)),
                       turns=round(sum(g["death"]["turns"] for g in gs) / len(gs), 1),
                       pots=round(sum(len(g["death"]["potions"]) for g in gs) / len(gs), 1),
                       gold=round(sum(g["death"]["gold"] for g in gs) / len(gs))))
def f33(gs):
    S = [g["snaps"][33] for g in gs if 33 in g["snaps"]]; n = len(S); ds = [deckstats(s["deck"]) for s in S]
    return dict(n=n, deck=round(sum(x[0] for x in ds) / n, 1), strikes=round(sum(x[1] for x in ds) / n, 1),
                defends=round(sum(x[2] for x in ds) / n, 1), up=round(sum(x[3] for x in ds) / n, 1),
                relics=round(sum(s["relics"] for s in S) / n, 1), pots=round(sum(len(s["potions"]) for s in S) / n, 2),
                gold=round(sum(s["gold"] for s in S) / n), hp=round(100 * sum(s["hp"] / s["max"] for s in S) / n))
cmp = dict(died=f33([g for g in L if g["death"] and g["death"]["floor"] == 33]), won=f33(W))

# act split of all losses on the full slice
rows = [l.split("\t") for l in open(os.path.join(R, "runs", "f40a10_es550.fs1366500n4000.a10.tsv")) if l.split("\t")[0].isdigit()]
loss = [r for r in rows if r[1] == "0" and r[9] != "2"]
acts = [sum(1 for r in loss if min(int(r[2]) // 17, 2) == a) for a in range(3)]

# ---- games to view
games = []
for D in game_dirs:
    for f in sorted(glob.glob(os.path.join(D, "game_*.txt"))):
        g = parse(f)
        games.append(dict(seed=g["seed"], win=bool(g.get("win")), floor=g.get("floor"),
                          death=g["death"]["enc"] if g["death"] else None, text=open(f).read()))

NOTES = {
    "1366518|Automaton": "Turn 4: 34 damage incoming (Flail 9×2 after the boss's block, plus two orb beams of 8). The agent spends its energy on Uppercut+, Blood for Blood and Pommel Strike+ and ends the turn with two Shrug It Off and Second Wind+ in hand: 52 → 17 HP. It never targets the orbs. On turn 6, Hyper Beam (43) plus the beams kill it. Racing a 320-HP boss with ≈ 40 damage a turn — a judgment the search may get wrong at 500 simulations.",
    "1366510|Collector": "Corruption on turn 0 makes every skill cost 0 and exhaust; the agent then keeps its Defends (playing them would exhaust them). On turn 1 it drinks four potions (Colorless, Dexterity, Entropic Brew, Elixir) before the Collector has attacked.",
    "1366521|Awakened One": "Arrives at full HP (94/94) with 466 gold: the act-3 path had no shop. Fusion Hammer (no upgrading at rest sites) and Sozu (no potions) are among its boss relics.",
    "1366512|Snake Plant": "Enters the fight at 24/104 HP.",
}
data = dict(deaths=deaths, cmp=cmp, acts=acts, nloss=len(loss), ngames=len(rows), sample=dict(n=len(G), w=len(W), l=len(L)), games=games, notes=NOTES)

page = open(os.path.join(R, "progress", "audit_template.html")).read().replace("/*DATA*/null", json.dumps(data))
open(out, "w").write(page)
print(f"audit page written: {len(page):,} bytes; {len(games)} games embedded; stats on {len(G)} traces")
