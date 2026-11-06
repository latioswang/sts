#!/usr/bin/env python3
"""Writes scenario plans from what reports/coverage.json says is still short, one file per class.

    python3 -I tools/coverage_plan.py --out plans --name cov1 --seed_start 8100

Each item goes to a seed of the class that owns it, in as many seeds as it's short, so a class's relics get tried with that
class's cards. Items no class owns go to Ironclad, whose choices are the best tuned. Then run each file with
`run_batch.sh mirror START COUNT --class CLASS --scenario_file FILE`.

With --mix N it writes N passes of everything each class can hold instead, shuffled, whatever is short. Most differences in
the first such runs came from relics and cards that rarely meet, so this looks for those.
"""
import argparse
import json
import os
import random
import sys

REALGAME = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, REALGAME)  # python3 -I leaves the script's folder off the path
from scenario_plan import plan_scenarios  # noqa: E402

RELIC_RUNS, POTION_USES = 5, 5
CLASSES = ("ironclad", "silent", "defect", "watcher")
CLASS_SEED_STEP = 100
RESULT_IDS = {"ironclad": "IRONCLAD", "silent": "THE_SILENT", "defect": "DEFECT", "watcher": "WATCHER"}
# coverage.json doesn't say who owns a potion. The other class potions are already at their targets.
POTION_OWNERS = {"Ambrosia": "watcher", "GhostInAJar": "silent"}
# Below these card scores a card goes in a third or second seed. A single seed adds a few plays at most.
CARD_THREE_SEEDS_BELOW, CARD_TWO_SEEDS_BELOW = 0.5, 0.9
GIVEN_CATEGORIES = ("relics", "cards", "potions")
STARTING_RELICS = frozenset({"Burning Blood", "Ring of the Snake", "Cracked Core", "PureWater"})
# Relics only their own event (or Neow) gives. Held before that event, they make a state no real run reaches, and the mirror stops
# there (see Known gaps in reports/simulator_fixes.md), so the shuffled passes leave them out.
EVENT_ONLY_RELICS = frozenset({"Golden Idol", "Bloody Idol", "WarpedTongs", "NeowsBlessing"})
OTHER_CLASS_POTIONS = {c: frozenset(p for p, owner in POTION_OWNERS.items() if owner != c) for c in CLASSES}
MIX_RELICS_PER_SEED, MIX_CARDS_PER_SEED, MIX_POTIONS_PER_SEED = 6, 6, 2


def seeds_needed(category: str, item: dict) -> int:
    if category == "relics":
        return max(0, RELIC_RUNS - item["runs"])
    if category == "potions":
        return max(0, POTION_USES - item["uses"])
    score = item["score"]
    if score >= 1.0:
        return 0
    return 3 if score < CARD_THREE_SEEDS_BELOW else 2 if score < CARD_TWO_SEEDS_BELOW else 1


def owner_of(category: str, item_id: str, item: dict) -> str | None:
    """The class whose seeds get this item, or None if it isn't something to give."""
    if item.get("kind") == "unplayable":
        return None  # curses and statuses come by themselves soon enough
    owner = POTION_OWNERS.get(item_id, item.get("class", "any")) if category == "potions" else item.get("class", "any")
    return owner if owner in CLASSES else "ironclad"


def split_by_class(items: dict) -> dict[str, dict[str, dict[str, int]]]:
    """{class: {category: {item: seeds needed}}}"""
    needs = {c: {category: {} for category in GIVEN_CATEGORIES} for c in CLASSES}
    for category in GIVEN_CATEGORIES:
        for item_id, item in items[category].items():
            count, owner = seeds_needed(category, item), owner_of(category, item_id, item)
            if count and owner:
                needs[owner][category][item_id] = count
    return needs


def scenarios_for(need: dict[str, dict[str, int]]) -> list[str]:
    """One scenario per seed. The cards are upgraded, since that's what most short cards lack."""
    rounds = max((n for category in GIVEN_CATEGORIES for n in need[category].values()), default=0)
    scenarios: list[str] = []
    for i in range(rounds):
        relics, cards, potions = ([item for item, n in need[category].items() if n > i] for category in GIVEN_CATEGORIES)
        scenarios.extend(plan_scenarios(relics, cards, potions, upgrade=True))
    return scenarios


def what_a_class_holds(items: dict, cls: str) -> tuple[list[str], list[str], list[str]]:
    relics = [r for r, item in items["relics"].items()
              if item.get("class", "any") in (cls, "any") and r not in STARTING_RELICS and r not in EVENT_ONLY_RELICS]
    cards = [c for c, item in items["cards"].items() if item.get("kind") != "unplayable" and item.get("class") in (cls, "colorless")]
    potions = [p for p in items["potions"] if p not in OTHER_CLASS_POTIONS[cls]]
    return relics, cards, potions


def mix_scenarios(items: dict, cls: str, shuffle_seed: int, relics_per_seed: int = MIX_RELICS_PER_SEED,
                  cards_per_seed: int = MIX_CARDS_PER_SEED, potions_per_seed: int = MIX_POTIONS_PER_SEED) -> list[str]:
    """Everything the class can hold, in a shuffled order, a few items of each kind to a seed."""
    rng = random.Random(f"{cls}:{shuffle_seed}")
    relics, cards, potions = what_a_class_holds(items, cls)
    for group in (relics, cards, potions):
        group.sort()
        rng.shuffle(group)
    seeds = max(len(relics) // relics_per_seed, len(cards) // cards_per_seed, len(potions) // potions_per_seed) + 1
    scenarios: list[str] = []
    for i in range(seeds):
        scenarios.extend(plan_scenarios(relics[i * relics_per_seed:(i + 1) * relics_per_seed],
                                        cards[i * cards_per_seed:(i + 1) * cards_per_seed],
                                        potions[i * potions_per_seed:(i + 1) * potions_per_seed],
                                        relics_per_seed=relics_per_seed, cards_per_seed=cards_per_seed,
                                        potions_per_seed=potions_per_seed, upgrade=i % 2 == 0))
    return scenarios


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--coverage", default=os.path.join(REALGAME, "reports", "coverage.json"))
    parser.add_argument("--out", default=os.path.join(REALGAME, "plans"))
    parser.add_argument("--name", required=True, help="files are plan_<name>_<class>.tsv")
    parser.add_argument("--seed_start", type=int, required=True)
    parser.add_argument("--mix", type=int, default=0, help="this many shuffled passes of everything, not what's short")
    parser.add_argument("--first_pass", type=int, default=0, help="with --mix, the shuffle to start from, to get new ones")
    args = parser.parse_args()
    with open(args.coverage) as f:
        items = json.load(f)["items"]
    for i, (cls, need) in enumerate(split_by_class(items).items()):
        if args.mix:
            scenarios = [s for n in range(args.first_pass, args.first_pass + args.mix) for s in mix_scenarios(items, cls, n)]
        else:
            scenarios = scenarios_for(need)
        start = args.seed_start + i * CLASS_SEED_STEP
        if len(scenarios) > CLASS_SEED_STEP:
            sys.exit(f"{cls}: {len(scenarios)} seeds won't fit before the next class's")
        path = os.path.join(args.out, f"plan_{args.name}_{cls}.tsv")
        with open(path, "w") as f:
            f.writelines(f"{start + n}\t{scenario}\n" for n, scenario in enumerate(scenarios))
        print(f"{RESULT_IDS[cls]} {start} {len(scenarios)} {path}")


if __name__ == "__main__":
    main()
