#!/usr/bin/env python3
"""Tests for coverage_plan.py. Run with python3 tools/test_coverage_plan.py."""
import unittest

from coverage_plan import scenarios_for, seeds_needed, split_by_class


def card(cls: str, score: float) -> dict:
    return {"class": cls, "kind": "card", "score": score}


class SeedsNeededTest(unittest.TestCase):
    def test_a_relic_needs_a_seed_for_each_run_it_is_short(self) -> None:
        self.assertEqual(seeds_needed("relics", {"runs": 3, "score": 0.6}), 2)
        self.assertEqual(seeds_needed("relics", {"runs": 5, "score": 1.0}), 0)

    def test_a_potion_likewise(self) -> None:
        self.assertEqual(seeds_needed("potions", {"uses": 4, "score": 0.8}), 1)

    def test_a_card_needs_more_seeds_the_further_it_is_from_its_target(self) -> None:
        self.assertEqual(seeds_needed("cards", card("silent", 1.0)), 0)
        self.assertEqual(seeds_needed("cards", card("silent", 0.95)), 1)
        self.assertEqual(seeds_needed("cards", card("silent", 0.7)), 2)
        self.assertEqual(seeds_needed("cards", card("silent", 0.0)), 3)


class SplitTest(unittest.TestCase):
    def setUp(self) -> None:
        self.items = {
            "relics": {"FrozenCore": {"runs": 0, "score": 0.0, "class": "defect"}, "Anchor": {"runs": 9, "score": 1.0, "class": "any"},
                       "Orrery": {"runs": 4, "score": 0.8, "class": "any"}},
            "cards": {"Reflex": card("silent", 0.0), "Bandage Up": card("colorless", 0.66), "Injury": {"class": "curse", "kind": "unplayable", "score": 0.0}},
            "potions": {"Ambrosia": {"uses": 3, "score": 0.6, "class": "watcher"}},
        }

    def test_class_items_go_to_their_class_and_the_rest_to_ironclad(self) -> None:
        needs = split_by_class(self.items)
        self.assertEqual(needs["defect"]["relics"], {"FrozenCore": 5})
        self.assertEqual(needs["ironclad"]["relics"], {"Orrery": 1})
        self.assertEqual(needs["silent"]["cards"], {"Reflex": 3})
        self.assertEqual(needs["ironclad"]["cards"], {"Bandage Up": 2})
        self.assertEqual(needs["watcher"]["potions"], {"Ambrosia": 2})

    def test_curses_are_not_given_on_purpose(self) -> None:
        needs = split_by_class(self.items)
        self.assertFalse(any("Injury" in n["cards"] for n in needs.values()))


class ScenariosTest(unittest.TestCase):
    def test_an_item_appears_in_as_many_seeds_as_it_needs(self) -> None:
        scenarios = scenarios_for({"relics": {"FrozenCore": 3, "Runic Capacitor": 1}, "cards": {"Consume": 2}, "potions": {}})
        self.assertEqual(len(scenarios), 3)
        self.assertEqual(sum("relic:FrozenCore" in s for s in scenarios), 3)
        self.assertEqual(sum("relic:Runic Capacitor" in s for s in scenarios), 1)
        self.assertEqual(sum("card:Consume+1" in s for s in scenarios), 2)

    def test_a_relic_that_opens_a_screen_comes_through_its_pool(self) -> None:
        scenarios = scenarios_for({"relics": {"Orrery": 1}, "cards": {}, "potions": {}})
        self.assertEqual(scenarios, ["front:shop:Orrery"])

    def test_nothing_short_means_no_seeds(self) -> None:
        self.assertEqual(scenarios_for({"relics": {}, "cards": {}, "potions": {}}), [])


if __name__ == "__main__":
    unittest.main()
