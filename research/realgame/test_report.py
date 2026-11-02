"""Tests for report.py. Run: pytest research/realgame/test_report.py"""
import json

from report import load_stops, render, suspect_for


def write_results(path, rows) -> str:
    target = path / "mirror_results.jsonl"
    target.write_text("\n".join(json.dumps(r) for r in rows) + "\n", encoding="utf-8")
    return str(target)


def row(seed: int, status: str, diffs=()) -> dict:
    return {"seed": seed, "seed_text": f"S{seed}", "status": status, "floor": 3, "act": 1, "step": 40, "where": "BATTLE",
            "last_action": "END", "message": "", "diffs": list(diffs)}


def test_newest_result_per_seed_wins(tmp_path):
    diff = {"field": "gold", "sim": "99", "game": "100", "hard": True}
    path = write_results(tmp_path, [row(1, "diverged", [diff]), row(1, "completed")])
    stops = load_stops(path)
    assert [s.status for s in stops] == ["completed"]


def test_first_field_skips_soft_differences(tmp_path):
    soft = {"field": "monster[0].move_id", "sim": "A", "game": "unmapped byte 9", "hard": False}
    hard = {"field": "draw_pile", "sim": "[a]", "game": "[b]", "hard": True}
    stops = load_stops(write_results(tmp_path, [row(2, "diverged", [soft, hard])]))
    assert stops[0].first_field == "draw_pile"


def test_stop_with_no_hard_difference_is_named_by_status(tmp_path):
    stops = load_stops(write_results(tmp_path, [row(3, "bridge_gap")]))
    assert stops[0].first_field == "bridge_gap"


def test_suspect_uses_longest_meaningful_prefix():
    assert "move table" in suspect_for("monster[1].move")
    assert "encounter selection" in suspect_for("monsters.count")
    assert suspect_for("something new") == "unclassified"


def test_render_counts_and_lists_repro(tmp_path):
    diff = {"field": "player.block", "sim": "5", "game": "8", "hard": True}
    report = render(load_stops(write_results(tmp_path, [row(4, "completed"), row(5, "diverged", [diff])])))
    assert "2 seeds: 1 matched to their end point, 1 stopped at a difference." in report
    assert "`player.block`" in report
    assert "sts_bridge mirror --seed_start 5 --count 1" in report
