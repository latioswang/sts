#!/usr/bin/env python3
"""Tests for coverage_report.py. Run: python3 test_coverage_report.py"""
import json
import unittest

from coverage_report import empty_seen, read_transcript, report


def game(state):
    return json.dumps({"dir": "game", "line": json.dumps({"game_state": state})})


def sent(command):
    return json.dumps({"dir": "sent", "line": command})


COMBAT = {
    "relics": [{"id": "Burning Blood"}],
    "potions": [{"id": "Fire Potion"}, {"id": "Potion Slot"}],
    "deck": [{"id": "Strike_R"}],
    "screen_type": "NONE",
    "combat_state": {
        "hand": [{"id": "Bash"}, {"id": "Defend_R"}],
        "monsters": [{"id": "JawWorm", "powers": [{"id": "Strength"}]}],
        "player": {"powers": [{"id": "Vulnerable"}]},
    },
}


class TranscriptTest(unittest.TestCase):
    def test_play_names_the_card_in_the_previous_state(self):
        seen = empty_seen()
        read_transcript([game(COMBAT), sent("PLAY 2 0")], seen)
        self.assertEqual(seen["cards_played"], {"Defend_R": 1})

    def test_potion_use_is_counted_and_empty_slots_are_not_potions(self):
        seen = empty_seen()
        read_transcript([game(COMBAT), sent("POTION Use 0 0")], seen)
        self.assertEqual(seen["potions_used"], {"Fire Potion": 1})
        self.assertEqual(set(seen["potions_seen"]), {"Fire Potion"})

    def test_state_contents_are_collected(self):
        seen = empty_seen()
        read_transcript([game(COMBAT)], seen)
        self.assertEqual(set(seen["relics"]), {"Burning Blood"})
        self.assertEqual(set(seen["monsters"]), {"JawWorm"})
        self.assertEqual(set(seen["powers"]), {"Strength", "Vulnerable"})
        self.assertEqual(set(seen["cards_seen"]), {"Strike_R", "Bash", "Defend_R"})

    def test_event_id_comes_from_the_event_screen(self):
        seen = empty_seen()
        read_transcript([game({"screen_type": "EVENT", "screen_state": {"event_id": "Big Fish"}})], seen)
        self.assertEqual(seen["events"], {"Big Fish": 1})

    def test_bad_lines_and_commands_before_any_state_are_ignored(self):
        seen = empty_seen()
        read_transcript(["not json", sent("PLAY 1"), json.dumps({"dir": "game", "line": "nope"})], seen)
        self.assertEqual(seen, empty_seen())

    def test_report_lists_what_never_came_up(self):
        seen = empty_seen()
        read_transcript([game(COMBAT)], seen)
        text = report(seen, {"relics": {"Burning Blood", "Anchor"}})
        self.assertIn("relics: 1 seen", text)
        self.assertIn("never: Anchor", text)


if __name__ == "__main__":
    unittest.main()
