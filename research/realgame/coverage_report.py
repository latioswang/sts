#!/usr/bin/env python3
"""Shows what mirror or live runs have exercised on the real game, read from the bridge's transcripts.

A mirror batch with no differences says little about a card nobody played. This lists which cards were played,
which relics, potions, monsters, powers and events appeared, and with --jar what the game has that never came up.

    coverage_report.py logs/transcript-*.jsonl [--jar ~/.sts_realgame/desktop-1.0.jar --javap path/to/javap]
"""
import argparse
import json
import re
import subprocess
import sys
import zipfile

CATEGORIES = ("cards_played", "cards_seen", "relics", "potions_used", "potions_seen", "monsters", "powers", "events")

# Which classes in the game's jar make up each category, for the list of what never came up.
JAR_PREFIXES = {
    "cards_played": ("cards/red/", "cards/colorless/"),
    "cards_seen": ("cards/curses/",),
    "relics": ("relics/",),
    "potions_seen": ("potions/",),
    "monsters": ("monsters/exordium/", "monsters/city/", "monsters/beyond/", "monsters/ending/"),
    "events": ("events/exordium/", "events/city/", "events/beyond/", "events/shrines/"),
}


def empty_seen():
    return {category: {} for category in CATEGORIES}


def add(seen, category, key):
    if key:
        seen[category][key] = seen[category].get(key, 0) + 1


def note_state(seen, state):
    for relic in state.get("relics", []):
        add(seen, "relics", relic.get("id"))
    for potion in state.get("potions", []):
        if potion.get("id") != "Potion Slot":
            add(seen, "potions_seen", potion.get("id"))
    for card in state.get("deck", []):
        add(seen, "cards_seen", card.get("id"))
    if state.get("screen_type") == "EVENT":
        add(seen, "events", state.get("screen_state", {}).get("event_id"))
    combat = state.get("combat_state")
    if not combat:
        return
    for monster in combat.get("monsters", []):
        add(seen, "monsters", monster.get("id"))
        for power in monster.get("powers", []):
            add(seen, "powers", power.get("id"))
    for power in combat.get("player", {}).get("powers", []):
        add(seen, "powers", power.get("id"))
    for pile in ("hand", "draw_pile", "discard_pile", "exhaust_pile"):
        for card in combat.get(pile, []):
            add(seen, "cards_seen", card.get("id"))


def note_command(seen, command, state):
    """A PLAY or POTION command names a slot in the state the game showed just before it."""
    if not state:
        return
    parts = command.split()
    if not parts:
        return
    if parts[0] == "PLAY" and len(parts) > 1 and parts[1].isdigit():
        hand = state.get("combat_state", {}).get("hand", [])
        index = int(parts[1]) - 1
        if 0 <= index < len(hand):
            add(seen, "cards_played", hand[index].get("id"))
    elif parts[0] == "POTION" and len(parts) > 2 and parts[1].upper() == "USE" and parts[2].isdigit():
        potions = state.get("potions", [])
        index = int(parts[2])
        if 0 <= index < len(potions):
            add(seen, "potions_used", potions[index].get("id"))


def read_transcript(lines, seen):
    state = None
    for line in lines:
        try:
            row = json.loads(line)
        except json.JSONDecodeError:
            continue
        if row.get("dir") == "game":
            try:
                message = json.loads(row.get("line", ""))
            except json.JSONDecodeError:
                continue
            state = message.get("game_state") or state
            if message.get("game_state"):
                note_state(seen, message["game_state"])
        else:
            note_command(seen, row.get("line", ""), state)


def ids_in_jar(jar, javap):
    """Returns {category: set of ids} from each class's public ID constant."""
    with zipfile.ZipFile(jar) as archive:
        names = [n for n in archive.namelist() if n.endswith(".class") and "$" not in n]
    root = "com/megacrit/cardcrawl/"
    universe = {}
    for category, prefixes in JAR_PREFIXES.items():
        classes = [n[:-6].replace("/", ".") for n in names if n.startswith(tuple(root + p for p in prefixes))]
        output = subprocess.run([javap, "-constants", "-p", "-cp", jar, *classes], capture_output=True, text=True, check=True).stdout
        universe[category] = set(re.findall(r'static final java\.lang\.String (?:POTION_)?ID = "([^"]+)"', output))
    return universe


def report(seen, universe=None):
    lines = []
    for category in CATEGORIES:
        lines.append(f"{category}: {len(seen[category])} seen")
        if universe and category in universe:
            missing = sorted(universe[category] - set(seen[category]))
            lines.append(f"  never: {', '.join(missing) if missing else '(none)'}")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("transcripts", nargs="+")
    parser.add_argument("--jar")
    parser.add_argument("--javap", default="javap")
    args = parser.parse_args()
    seen = empty_seen()
    for path in args.transcripts:
        with open(path, encoding="utf-8") as transcript:
            read_transcript(transcript, seen)
    universe = ids_in_jar(args.jar, args.javap) if args.jar else None
    print(report(seen, universe))
    return 0


if __name__ == "__main__":
    sys.exit(main())
