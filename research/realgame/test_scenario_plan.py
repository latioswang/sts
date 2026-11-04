#!/usr/bin/env python3
"""Tests for scenario_plan.py. Run: python3 test_scenario_plan.py"""
import unittest

from scenario_plan import DEFECT_SEEDS, defect_scenarios, known_ids, plan_scenarios


class PlanTest(unittest.TestCase):
    def test_everything_is_used_exactly_once(self):
        relics = {f"R{i}" for i in range(12)}
        cards = {f"C{i}" for i in range(13)}
        potions = {f"P{i}" for i in range(5)}
        scenarios = plan_scenarios(relics, cards, potions)
        items = [item for scenario in scenarios for item in scenario.split(";")]
        self.assertEqual(len(items), len(set(items)))
        self.assertEqual(len(items), 30)

    def test_screen_relics_go_to_the_front_of_their_pool(self):
        scenarios = plan_scenarios({"Anchor", "Astrolabe"}, set(), set())
        self.assertEqual(scenarios, ["relic:Anchor;front:boss:Astrolabe"])

    def test_upgrade_marks_cards_only(self):
        scenarios = plan_scenarios({"Anchor"}, {"Clash"}, set(), upgrade=True)
        self.assertEqual(scenarios, ["relic:Anchor;card:Clash+1"])

    def test_nothing_left_means_no_seeds(self):
        self.assertEqual(plan_scenarios(set(), set(), set()), [])

    def test_known_ids_reads_a_table_of_one_enum(self):
        table = '{"Anchor", RelicId::ANCHOR},\n{"Clash", CardId::CLASH},\n'
        self.assertEqual(known_ids(table, "RelicId"), {"Anchor"})


if __name__ == "__main__":
    unittest.main()


def test_necronomicon_comes_after_the_cards():
    [scenario] = plan_scenarios({"Necronomicon", "Anchor"}, {"Bash"}, set())
    assert scenario.split(";") == ["relic:Anchor", "card:Bash", "relic:Necronomicon"]


def test_defect_plan_covers_every_blue_card_with_a_slot():
    cards = [c for group, _ in DEFECT_SEEDS for c in group]
    assert len(cards) == 75 and len(set(cards)) == 75
    scenarios = defect_scenarios()
    assert len(scenarios) == 2 * len(DEFECT_SEEDS)
    assert all(s.startswith("relic:PrismaticShard;") for s in scenarios)
    assert "card:Zap+1" in scenarios[len(DEFECT_SEEDS)]
