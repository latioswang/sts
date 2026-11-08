#!/usr/bin/env python3
"""Tests for export_traces.py and run_regression.py. Run: python3 test_regression_tools.py"""
import json
import unittest

from coverage import RunScan, Tally, without_unreachable
from export_traces import find_note_card, find_scenario, seed_from_text, split_runs, state_items, strip_state
from run_regression import (Row, compare, kind_of, later_runs_got_further, load_real_stops, mark_past_real_stop, merge_real,
                            write_real_stops)

STARTER = [{"id": "Strike_R"}] * 5 + [{"id": "Defend_R"}] * 4 + [{"id": "Bash"}]


def game_line(game_state: dict) -> str:
    return json.dumps({"ready_for_command": True, "in_game": True, "game_state": game_state})


def transcript(*entries: tuple) -> list[str]:
    return [json.dumps({"t": i, "dir": d, "line": line}) for i, (d, line) in enumerate(entries)]


class UniverseTest(unittest.TestCase):
    def test_what_no_run_can_reach_does_not_count(self) -> None:
        universe = {"cards": {"Bash": {"class": "ironclad"}, "Impulse": {"class": "defect"}}, "powers": ["Winter", "Strength"], "relics": {"Anchor": None}}
        reachable = without_unreachable(universe)
        self.assertEqual(list(reachable["cards"]), ["Bash"])
        self.assertEqual(reachable["powers"], ["Strength"])
        self.assertEqual(reachable["relics"], {"Anchor": None})
        self.assertIn("Impulse", universe["cards"])  # the cached copy stays as it was

    def test_a_card_that_cant_be_played_counts_in_turns_held(self) -> None:
        universe = {"cards": {"Reflex": {"class": "silent", "kind": "card"}, "Bash": {"class": "ironclad", "kind": "card"}},
                    "relics": {}, "potions": [], "monsters": [], "events": [], "powers": []}
        cards = Tally(universe).targets()["cards"]
        self.assertEqual(list(cards["Reflex"]), ["in_hand"])
        self.assertIn("plays", cards["Bash"])

    def test_a_card_held_or_exhausted_on_draw_is_seen(self) -> None:
        scan = RunScan("t", ["START", "WATCHER", "0", "1"], 0.0)
        reflex = {"id": "Reflex", "uuid": "a", "type": "SKILL"}
        deus = {"id": "DeusExMachina", "uuid": "b", "type": "SKILL"}
        strike = {"id": "Strike_P", "uuid": "c", "type": "ATTACK"}
        for turn in (1, 1, 2):
            scan.scan_state({"floor": 3, "combat_state": {"turn": turn, "monsters": [], "player": {}, "hand": [reflex, strike], "exhaust_pile": [deus, strike]}})
        held = [e[2] for e in scan.run["events"] if e[0] == "u"]
        self.assertEqual(sorted(held), ["DeusExMachina", "Reflex", "Reflex"])  # per turn in hand, per fight once drawn


class SeedTest(unittest.TestCase):
    def test_seed_text_is_base_35_with_no_letter_o(self) -> None:
        self.assertEqual(seed_from_text("TK"), 1000)
        self.assertEqual(seed_from_text("1Q0"), 2100)
        self.assertEqual(seed_from_text("1QN"), 2123)


class StripTest(unittest.TestCase):
    def test_map_is_kept_only_on_the_map_screen(self) -> None:
        self.assertIn("map", json.loads(strip_state(game_line({"screen_type": "MAP", "map": [1]})))["game_state"])
        self.assertNotIn("map", json.loads(strip_state(game_line({"screen_type": "NONE", "map": [1]})))["game_state"])

    def test_event_text_goes_except_neow_options(self) -> None:
        screen = {"event_id": "Big Fish", "body_text": "words", "options": [{"text": "[Eat]", "label": "Eat"}]}
        stripped = json.loads(strip_state(game_line({"screen_type": "EVENT", "screen_state": screen})))["game_state"]["screen_state"]
        self.assertNotIn("body_text", stripped)
        self.assertEqual(stripped["options"], [{"label": "Eat"}])
        neow = {"event_id": "Neow Event", "body_text": "words", "options": [{"text": "[Gain 100 Gold]", "label": "x"}]}
        kept = json.loads(strip_state(game_line({"screen_type": "EVENT", "screen_state": neow})))["game_state"]["screen_state"]
        self.assertEqual(kept["options"][0]["text"], "[Gain 100 Gold]")

    def test_a_line_that_is_not_json_passes_through(self) -> None:
        self.assertEqual(strip_state("ready"), "ready")


class SplitTest(unittest.TestCase):
    def test_runs_split_at_start_and_the_last_needs_quiet(self) -> None:
        lines = transcript(("client", "STATE"), ("client", "START IRONCLAD 0 TK"), ("game", "{}"), ("note", "rng a=1,2,3"),
                           ("client", "START IRONCLAD 20 1QN"), ("game", "{}"))
        runs = list(split_runs(lines, "t.jsonl", quiet=False))
        self.assertEqual([r.seed_text for r in runs], ["TK", "1QN"])
        self.assertEqual([r.complete for r in runs], [True, False])
        self.assertEqual(runs[0].events[-1], ("N", "rng a=1,2,3"))
        self.assertEqual(runs[1].asc, 20)
        self.assertEqual(runs[1].trace, "t.jsonl#1")
        self.assertTrue(list(split_runs(lines, "t.jsonl", quiet=True))[1].complete)

    def test_a_partial_last_line_is_ignored(self) -> None:
        lines = transcript(("client", "START IRONCLAD 0 TK")) + ['{"t": 1, "dir": "ga']
        self.assertEqual(len(list(split_runs(lines, "t.jsonl", quiet=True))), 1)


class ScenarioTest(unittest.TestCase):
    def state(self, deck: list, relics: list, asc: int = 0) -> dict:
        return {"deck": deck, "relics": [{"id": r} for r in relics], "potions": [{"id": "Potion Slot"}], "ascension_level": asc}

    def test_ascenders_bane_is_part_of_the_kit_from_ascension_10(self) -> None:
        deck = [{"id": "AscendersBane"}] + STARTER
        self.assertEqual(state_items(self.state(deck, ["Burning Blood"], asc=10)), {})
        self.assertEqual(state_items(self.state(deck, ["Burning Blood"], asc=9)), {"card:AscendersBane": 1})

    def test_each_class_has_its_own_kit(self) -> None:
        deck = [{"id": c} for c in ("Strike_B",) * 4 + ("Defend_B",) * 4 + ("Zap", "Dualcast", "Consume")]
        gs = {**self.state(deck, ["Cracked Core", "DataDisk"]), "class": "DEFECT"}
        self.assertEqual(state_items(gs), {"relic:DataDisk": 1, "card:Consume": 1})

    def test_a_boss_relic_in_place_of_the_starter_is_an_item(self) -> None:
        deck = [{"id": c} for c in ("Strike_P",) * 4 + ("Defend_P",) * 4 + ("Eruption", "Vigilance")]
        gs = {**self.state(deck, ["HolyWater"]), "class": "WATCHER"}
        self.assertEqual(state_items(gs), {"relic:HolyWater": 1})

    def test_a_starter_a_relic_upgraded_is_still_the_starter(self) -> None:
        deck = [{"id": "Strike_B"}, {"id": "Strike_B"}, {"id": "Strike_B", "upgrades": 1}, {"id": "Strike_B", "upgrades": 1}]
        deck += [{"id": c} for c in ("Defend_B",) * 4 + ("Zap", "Dualcast", "Consume")]
        gs = {**self.state(deck, ["Cracked Core", "Whetstone"]), "class": "DEFECT"}
        self.assertEqual(state_items(gs), {"relic:Whetstone": 1, "card:Consume": 1})

    def test_an_upgraded_copy_beyond_the_kit_is_an_item(self) -> None:
        deck = [{"id": c} for c in ("Strike_B",) * 3] + [{"id": "Strike_B", "upgrades": 1}, {"id": "Strike_B", "upgrades": 1}]
        deck += [{"id": c} for c in ("Defend_B",) * 4 + ("Zap", "Dualcast")]
        gs = {**self.state(deck, ["Cracked Core"]), "class": "DEFECT"}
        self.assertEqual(state_items(gs), {"card:Strike_B+1": 1})

    def test_no_state_call_after_start_means_no_scenario(self) -> None:
        self.assertEqual(find_scenario([("C", "START IRONCLAD 0 TK"), ("G", "{}"), ("C", "CHOOSE 0")], "TK", [], {}), ("", "none"))

    def test_a_plan_is_used_if_its_items_match_what_the_game_showed(self) -> None:
        gs = self.state(STARTER + [{"id": "Clash", "upgrades": 1}], ["Burning Blood", "Anchor"])
        events = [("C", "START IRONCLAD 0 TK"), ("C", "STATE"), ("G", game_line(gs))]
        plan = "relic:Anchor;card:Clash+1;front:boss:Astrolabe"
        self.assertEqual(find_scenario(events, "TK", [], {1000: [plan]}), (plan, "plan"))

    def test_otherwise_the_scenario_is_rebuilt_from_the_state(self) -> None:
        gs = self.state(STARTER + [{"id": "Clash", "upgrades": 1}], ["Burning Blood", "Anchor"])
        events = [("C", "START IRONCLAD 0 TK"), ("C", "STATE"), ("G", game_line(gs))]
        self.assertEqual(find_scenario(events, "TK", [], {1000: ["relic:Orrery"]}), ("relic:Anchor;card:Clash+1", "rebuilt"))


class NoteCardTest(unittest.TestCase):
    def test_the_card_that_joins_the_deck_after_the_event(self) -> None:
        before = {"screen_state": {"event_id": "NoteForYourself"}, "deck": [{"id": "Bash"}]}
        after = {"screen_state": {}, "deck": [{"id": "Bash"}, {"id": "Doubt", "upgrades": 2}]}
        events = [("G", game_line(before)), ("C", "CHOOSE 0"), ("G", game_line(after))]
        self.assertEqual(find_note_card(events), ("Doubt", 2))

    def test_no_event_no_card(self) -> None:
        self.assertIsNone(find_note_card([("G", game_line({"deck": [{"id": "Bash"}]}))]))


class CompareTest(unittest.TestCase):
    def test_kinds(self) -> None:
        self.assertEqual(kind_of({"status": "completed"}), "pass")
        self.assertEqual(kind_of({"status": "trace_mismatch"}), "limited")
        self.assertEqual(kind_of({"status": "diverged"}), "fail")
        self.assertEqual(kind_of({"status": "diverged", "scenario_source": "rebuilt"}), "limited")
        self.assertEqual(kind_of({"status": "replay_crash"}), "fail")

    def test_a_new_failure_is_worse(self) -> None:
        self.assertIsNotNone(compare({"status": "diverged", "commands_replayed": 5}, Row("t", "completed", 5, "")))
        self.assertIsNotNone(compare({"status": "diverged"}, None))

    def test_checking_fewer_commands_is_worse_even_without_a_failure(self) -> None:
        self.assertIsNotNone(compare({"status": "trace_mismatch", "commands_replayed": 4}, Row("t", "trace_mismatch", 5, "")))

    def test_a_known_failure_at_the_same_place_is_not_worse(self) -> None:
        self.assertIsNone(compare({"status": "diverged", "commands_replayed": 5}, Row("t", "diverged", 5, "why")))

    def test_getting_further_is_not_worse(self) -> None:
        self.assertIsNone(compare({"status": "completed", "commands_replayed": 9}, Row("t", "diverged", 5, "why")))

    def test_later_runs_got_further(self) -> None:
        rows = [{"file": "a", "trace": "a", "seed": 1, "asc": 0, "status": "diverged", "floor": 5},
                {"file": "b", "trace": "b", "seed": 1, "asc": 0, "status": "completed", "floor": 9},
                {"file": "c", "trace": "c", "seed": 2, "asc": 0, "status": "diverged", "floor": 5}]
        self.assertEqual(later_runs_got_further(rows), {"a"})


class PastRealStopTest(unittest.TestCase):
    def test_tail_after_a_stop_on_an_unsupported_card_is_not_a_failure(self):
        row = {"seed_text": "1YN", "status": "diverged", "commands_total": 70, "commands_replayed": 47}
        mark_past_real_stop([row], {"1YN": [(44, "unsupported_card")]})
        self.assertEqual(row["status"], "past_real_stop")
        self.assertEqual(kind_of(row), "limited")

    def test_a_difference_before_the_stop_still_fails(self):
        row = {"seed_text": "1YN", "status": "diverged", "commands_total": 70, "commands_replayed": 30}
        mark_past_real_stop([row], {"1YN": [(44, "unsupported_card")]})
        self.assertEqual(row["status"], "diverged")

    def test_a_later_full_run_of_the_seed_isnt_excused(self):
        row = {"seed_text": "1YN", "status": "diverged", "commands_total": 800, "commands_replayed": 500}
        mark_past_real_stop([row], {"1YN": [(44, "unsupported_card"), (800, "diverged")]})
        self.assertEqual(row["status"], "diverged")


class RealStopsFileTest(unittest.TestCase):
    def test_round_trip_keeps_only_seeds_with_an_unsupported_card_stop(self):
        import os
        import tempfile
        real = {"1YN": [(44, "unsupported_card"), (800, "diverged")], "2AB": [(90, "completed")]}
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "stops.tsv")
            write_real_stops(path, real)
            self.assertEqual(load_real_stops(path), {"1YN": [(44, "unsupported_card"), (800, "diverged")]})

    def test_a_missing_file_is_empty(self):
        self.assertEqual(load_real_stops("/nonexistent/stops.tsv"), {})

    def test_merge_drops_duplicates(self):
        merged = merge_real({"1YN": [(44, "unsupported_card")]}, {"1YN": [(44, "unsupported_card"), (800, "diverged")]})
        self.assertEqual(merged, {"1YN": [(44, "unsupported_card"), (800, "diverged")]})

    def test_the_file_alone_marks_a_run_past_the_stop(self):
        row = {"seed_text": "1YN", "status": "diverged", "commands_total": 70, "commands_replayed": 47}
        mark_past_real_stop([row], merge_real({}, {"1YN": [(44, "unsupported_card")]}))
        self.assertEqual(row["status"], "past_real_stop")


if __name__ == "__main__":
    unittest.main()
