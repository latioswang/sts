#!/usr/bin/env python3
"""Writes a --scenario_file for sts_bridge mirror: each seed gets a few of the things mirror runs haven't reached yet.

    scenario_plan.py --logs logs --jar ~/.sts_realgame/desktop-1.0.jar --javap path/to/javap \\
        --seed_start 2000 -o logs/plan.tsv

Relics that open a screen when obtained go to the front of their pool, so they arrive through a normal reward.
"""
import argparse
import glob
import os
import re
import sys

from coverage_report import empty_seen, ids_in_jar, read_transcript

# Relics of other characters and the game's unused test relics. Ironclad can't find them in a normal run.
UNREACHABLE_RELICS = {
    "Cables", "Cracked Core", "DataDisk", "Damaru", "Emotion Chip", "FrozenCore", "GoldenEye", "HolyWater", "HoveringKite",
    "Inserter", "Melange", "Ninja Scroll", "Nuclear Battery", "Paper Crane", "PureWater", "Ring of the Serpent",
    "Ring of the Snake", "Runic Capacitor", "Snake Skull", "SsserpentHead", "Symbiotic Virus", "TeardropLocket",
    "The Specimen", "Tingsha", "Tough Bandages", "TwistedFunnel", "VioletLotus", "WristBlade", "Yang", "Yin", "CloakClasp",
    "Dark Core", "Derp Rock", "Discerning Monocle", "Dodecahedron", "Red Circlet", "Test 1", "Test 3", "Test 4", "Test 5", "Test 6",
}
UNREACHABLE_POTIONS = {
    "BottledMiracle", "CunningPotion", "EssenceOfDarkness", "Poison Potion", "PotionOfCapacity", "FocusPotion", "Ambrosia",
    "StancePotion", "GhostInAJar", "Potion Slot",
}
# It offers other classes' cards, which end a mirror run, so it gets a seed of its own instead of sharing one.
UNPLAYABLE_RELICS = {"PrismaticShard"}
# The character starts with it.
STARTING_RELICS = {"Burning Blood"}
# Its curse arrives through a screen effect, so in the game it lands after the scenario's cards.
RELICS_AFTER_CARDS = {"Necronomicon"}
# Tier of each relic whose equip effect opens a screen or replaces another relic.
SCREEN_RELICS = {
    "Astrolabe": "boss", "Black Blood": "boss", "Calling Bell": "boss", "Empty Cage": "boss", "Pandora's Box": "boss", "Tiny House": "boss",
    "Bottled Flame": "uncommon", "Bottled Lightning": "uncommon", "Bottled Tornado": "uncommon",
    "Cauldron": "shop", "DollysMirror": "shop", "Orrery": "shop",
}


def known_ids(table_text, enum):
    return set(re.findall(r'\{"([^"]+)", ' + enum + r"::\w+\}", table_text))


def take(items, count):
    """Splits a list into its first count items and the rest, without changing it."""
    return items[:count], items[count:]


def plan_scenarios(relics, cards, potions, relics_per_seed=5, cards_per_seed=6, potions_per_seed=2, upgrade=False):
    """Returns one scenario string per seed, enough to use every relic, card and potion once."""
    direct = sorted(r for r in relics if r not in SCREEN_RELICS)
    fronted = sorted(r for r in relics if r in SCREEN_RELICS)
    cards = sorted(cards)
    potions = sorted(potions)
    scenarios = []
    while direct or fronted or cards or potions:
        some_relics, direct = take(direct, relics_per_seed)
        some_fronted, fronted = take(fronted, 1)
        some_cards, cards = take(cards, cards_per_seed)
        some_potions, potions = take(potions, potions_per_seed)
        items = [f"relic:{r}" for r in some_relics if r not in RELICS_AFTER_CARDS]
        items += [f"front:{SCREEN_RELICS[r]}:{r}" for r in some_fronted]
        items += [f"card:{c}{'+1' if upgrade else ''}" for c in some_cards]
        items += [f"potion:{p}" for p in some_potions]
        items += [f"relic:{r}" for r in some_relics if r in RELICS_AFTER_CARDS]
        scenarios.append(";".join(items))
    return scenarios


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--logs", required=True, help="folder searched recursively for transcripts")
    parser.add_argument("--jar", required=True)
    parser.add_argument("--javap", default="javap")
    parser.add_argument("--seed_start", type=int, required=True)
    parser.add_argument("--upgrade", action="store_true", help="give the cards upgraded")
    parser.add_argument("--all", action="store_true", help="include things that have already come up")
    parser.add_argument("-o", "--output", required=True)
    args = parser.parse_args()

    seen = empty_seen()
    for path in glob.glob(os.path.join(args.logs, "**", "transcript-*.jsonl"), recursive=True):
        with open(path, encoding="utf-8") as transcript:
            read_transcript(transcript, seen)
    universe = ids_in_jar(args.jar, args.javap)
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "GameIdTables.inc"), encoding="utf-8") as table_file:
        table = table_file.read()

    relics = universe["relics"] - UNREACHABLE_RELICS - UNPLAYABLE_RELICS - STARTING_RELICS
    cards = universe["cards_played"] | universe["cards_seen"]
    potions = universe["potions_seen"] - UNREACHABLE_POTIONS
    if not args.all:
        relics -= set(seen["relics"])
        cards -= set(seen["cards_played"])
        potions -= set(seen["potions_used"])
    for name, wanted, enum in (("relic", relics, "RelicId"), ("potion", potions, "Potion")):
        unknown = wanted - known_ids(table, enum)
        if unknown:
            print(f"warning: the bridge has no id for {name}s: {sorted(unknown)}", file=sys.stderr)
        wanted &= known_ids(table, enum)

    scenarios = plan_scenarios(relics, cards, potions, upgrade=args.upgrade)
    with open(args.output, "w", encoding="utf-8") as output:
        for i, scenario in enumerate(scenarios):
            output.write(f"{args.seed_start + i}\t{scenario}\n")
    print(f"{len(scenarios)} seeds from {args.seed_start}: {len(relics)} relics, {len(cards)} cards, {len(potions)} potions")
    return 0


if __name__ == "__main__":
    sys.exit(main())
