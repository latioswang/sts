"""Prints a seed's last transcript from a floor on: each command, then the state it led to."""
import glob, json, os, sys

logs, marker, floor, count = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
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
    if g["floor"] != floor or "combat_state" not in g:
        continue
    c = g["combat_state"]
    name = lambda x: x["id"] + ("+" * x["upgrades"]) + "/" + str(x["cost"])
    out.append(f">> {last} t{c['turn']} hp{g['current_hp']} b{c['player']['block']} e{c['player']['energy']} {[name(x) for x in c['hand']]}\n"
               f"     mon {[(m['id'], m['current_hp'], m['block'], m['intent'], [(p['id'], p['amount']) for p in m['powers']]) for m in c['monsters']]}\n"
               f"     pow {[(p['id'], p['amount']) for p in c['player']['powers']]}\n"
               f"     draw {len(c['draw_pile'])} disc {[x['id'] for x in c['discard_pile']][-6:]} exh {[x['id'] for x in c['exhaust_pile']][-6:]}")
print(len(out))
print("\n".join(out[-count:]))
print([x["id"] for x in g["relics"]])
print([x["id"] for x in g["deck"]])
