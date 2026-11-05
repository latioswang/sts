#!/usr/bin/env python3
"""Writes scenario plans from what reports/coverage.json says is still short, one file per class.

    python3 -I tools/coverage_plan.py --out plans --name cov1 --seed_start 8100

Each item goes to a seed of the class that owns it, in as many seeds as it's short, so a class's relics get tried with that
class's cards. Items no class owns go to Ironclad, whose choices are the best tuned. Then run each file with
`run_batch.sh mirror START COUNT --class CLASS --scenario_file FILE`.
"""
import argparse
import json
import math
import os
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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--coverage", default=os.path.join(REALGAME, "reports", "coverage.json"))
    parser.add_argument("--out", default=os.path.join(REALGAME, "plans"))
    parser.add_argument("--name", required=True, help="files are plan_<name>_<class>.tsv")
    parser.add_argument("--seed_start", type=int, required=True)
    args = parser.parse_args()
    with open(args.coverage) as f:
        items = json.load(f)["items"]
    for i, (cls, need) in enumerate(split_by_class(items).items()):
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
