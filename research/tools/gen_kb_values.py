#!/usr/bin/env python3
"""Generate research/cpp/kb_card_values.inc from the repo KB (data/ironclad_cards.json)."""
import json, os, re
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TIER = {"S+": 10, "S": 9.5, "S-": 9, "A+": 8.5, "A": 8, "A-": 7.5, "B+": 7, "B": 6.5, "B-": 6,
        "C+": 5, "C": 4.5, "C-": 4, "D+": 3, "D": 2.5, "D-": 2, "E+": 1, "E": 0.5, "F": 0}
SPECIAL = {"Strike": "STRIKE_RED", "Defend": "DEFEND_RED"}
cards = json.load(open(os.path.join(ROOT, "data/ironclad_cards.json")))
enum = set(re.findall(r"^\s+([A-Z0-9_]+),", open("/home/user/sts_lightspeed/include/constants/Cards.h").read(), re.M))
out = []
for c in cards:
    n = SPECIAL.get(c["name"], re.sub(r"[^A-Z0-9]+", "_", c["name"].upper()).strip("_"))
    if n not in enum:
        print("MISSING", c["name"], n); continue
    out.append(f'case CardId::{n}: return {TIER[c["tier"]]};  // {c["tier"]}')
open(os.path.join(ROOT, "research/cpp/kb_card_values.inc"), "w").write("\n".join(out) + "\n")
print(len(out), "cards")
