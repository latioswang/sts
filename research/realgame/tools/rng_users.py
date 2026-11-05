"""Lists every game class that touches each dungeon RNG stream. Reads the jar's constant pools only.

Usage: rng_users.py <desktop jar> [stream ...]
"""
import sys
import zipfile

jar = sys.argv[1]
streams = sys.argv[2:] or ["aiRng", "monsterHpRng", "shuffleRng", "cardRandomRng", "miscRng", "potionRng", "relicRng", "treasureRng", "merchantRng", "eventRng", "monsterRng", "mapRng", "cardRng"]
found = {s: [] for s in streams}
with zipfile.ZipFile(jar) as z:
    for name in z.namelist():
        if not name.startswith("com/megacrit/cardcrawl/") or not name.endswith(".class"):
            continue
        data = z.read(name)
        for s in streams:
            # A CONSTANT_Utf8 entry: tag 1, a two-byte length, then the bytes.
            if b"\x01" + len(s).to_bytes(2, "big") + s.encode() in data:
                found[s].append(name[len("com/megacrit/cardcrawl/"):-len(".class")])
for s in streams:
    print(f"== {s} ({len(found[s])})")
    for c in found[s]:
        print("  ", c)
