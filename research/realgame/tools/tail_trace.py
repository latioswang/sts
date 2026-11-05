"""Prints a seed's last states from its newest transcript, skipping waits. Usage: tail_trace.py <logs> <seed text> <count>"""
import glob, json, os, sys

logs, marker, count = sys.argv[1], sys.argv[2], int(sys.argv[3])
rows = None
for f in sorted(glob.glob(logs + "/transcript-*.jsonl"), key=os.path.getmtime)[::-1]:
    rs = [json.loads(l) for l in open(f)]
    ids = [i for i, r in enumerate(rs) if r["dir"] == "client" and r["line"].startswith("START") and marker in r["line"]]
    if ids:
        rows = rs[ids[-1]:]
        nxt = [i for i, r in enumerate(rows) if i and r["dir"] == "client" and r["line"].startswith("START")]
        rows = rows[:nxt[0]] if nxt else rows
        break
last = None
out = []
g = None
for r in rows:
    if r["dir"] == "client":
        last = r["line"]
        continue
    try:
        g = json.loads(r["line"])["game_state"]
    except (ValueError, KeyError):
        continue
    if last.startswith("WAIT"):
        continue
    ss = g.get("screen_state", {})
    c = g.get("combat_state")
    mon = [(m["id"], m["current_hp"]) for m in c["monsters"]] if c else ""
    out.append(f"{last[:22]:22} f{g['floor']} {g['screen_type'][:12]:12} {g.get('room_type','')[:12]:12} hp{g['current_hp']}/{g['max_hp']} g{g['gold']} "
               f"{ss.get('event_id','')} {[o.get('label','')[:30] for o in ss.get('options', [])]} {[x['id'] for x in ss.get('cards', [])][:5]} "
               f"{[x.get('reward_type') for x in ss.get('rewards', [])]} {mon}")
print("\n".join(out[-count:]))
print([x["id"] for x in g["relics"]])
print([p["id"] for p in g["potions"]])
