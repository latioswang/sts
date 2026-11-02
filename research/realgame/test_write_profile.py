import json
import sys
from pathlib import Path

import write_profile


def run(directory: Path, monkeypatch) -> None:
    monkeypatch.setattr(sys, "argv", ["write_profile.py", str(directory)])
    assert write_profile.main() == 0


def read(directory: Path, name: str) -> dict[str, str]:
    return json.loads((directory / name).read_text(encoding="utf-8"))


def test_unlocks_everything_the_game_locks(tmp_path: Path, monkeypatch) -> None:
    run(tmp_path, monkeypatch)
    unlocks = read(tmp_path, "STSUnlocks")
    assert len(unlocks) == 36 + 24 + 3  # the game logs 9 cards x 4 colours and 24 relics; 3 characters start locked
    assert set(unlocks.values()) == {write_profile.UNLOCKED}


def test_neow_is_on_and_uploads_are_off(tmp_path: Path, monkeypatch) -> None:
    run(tmp_path, monkeypatch)
    assert read(tmp_path, "STSTips")["NEOW_SKIP"] == "true"
    assert read(tmp_path, "STSPlayer")["THE_SILENT_WIN"] == "true"
    assert read(tmp_path, "STSSeenBosses")["GHOST"] == "1"
    settings = read(tmp_path, "STSGameplaySettings")
    assert settings["Upload Data"] == "false"
    assert settings["Fast Mode"] == "true"


def test_keeps_other_keys_and_is_safe_to_rerun(tmp_path: Path, monkeypatch) -> None:
    (tmp_path / "STSGameplaySettings").write_text(json.dumps({"LANGUAGE": "ENG", "Fast Mode": "false"}), encoding="utf-8")
    run(tmp_path, monkeypatch)
    first = read(tmp_path, "STSGameplaySettings")
    assert first["LANGUAGE"] == "ENG"
    assert first["Fast Mode"] == "true"
    run(tmp_path, monkeypatch)
    assert read(tmp_path, "STSGameplaySettings") == first
