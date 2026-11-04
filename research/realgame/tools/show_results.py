"""Prints the mirror results in a compact form, one line per seed, plus a status count."""
import collections
import json
import sys

rows = [json.loads(line) for line in open(sys.argv[1], encoding="utf-8")]
counts = collections.Counter(r["status"] for r in rows)
for r in rows[-int(sys.argv[2]) if len(sys.argv) > 2 else 0:]:
    diffs = [(d["field"], d["sim"][:90], d["game"][:90]) for d in r["diffs"]][:3]
    print(r["seed"], r["seed_text"], r["status"], "f", r["floor"], "s", r["step"], r["where"], "after", r["last_action"][:50], diffs, r["message"][:260], round(r["seconds"]))
print(dict(counts))
