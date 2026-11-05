#!/usr/bin/env python3
"""Turns the relay's transcripts into one small recording per run, for `sts_bridge replay`.

    export_traces.py [--logs DIR] [--mirror_logs DIR ...] [--plans DIR ...] [--out DIR]

A recording starts with a JSON header, and then has one line per event: "C <command>", "G <game message>" or
"N <note>". It keeps what the bridge reads and drops what it doesn't: the map outside the map screen, and the game's
event text, apart from Neow's options, which the bridge reads the blessings from.

The relay doesn't record a run's scenario or the card Note For Yourself holds, so they are worked out here. A scenario
is read from the seed's mirror log or a plan, if the items match what the game showed. Otherwise it is rebuilt from
that state, which misses anything the state can't show (a relic put first in a pool, or a forced encounter).
"""
import argparse
import collections
import gzip
import json
import os
import re
import time
from typing import Iterable, Iterator, Optional

ALPHABET = "0123456789ABCDEFGHIJKLMNPQRSTUVWXYZ"
STARTER_DECKS = {
    "IRONCLAD": collections.Counter({"Strike_R": 5, "Defend_R": 4, "Bash": 1}),
    "THE_SILENT": collections.Counter({"Strike_G": 5, "Defend_G": 5, "Survivor": 1, "Neutralize": 1}),
    "DEFECT": collections.Counter({"Strike_B": 4, "Defend_B": 4, "Zap": 1, "Dualcast": 1}),
    "WATCHER": collections.Counter({"Strike_P": 4, "Defend_P": 4, "Eruption": 1, "Vigilance": 1}),
}
STARTER_RELICS = {"IRONCLAD": "Burning Blood", "THE_SILENT": "Ring of the Snake", "DEFECT": "Cracked Core", "WATCHER": "PureWater"}
ASCENDERS_BANE_ASCENSION = 10
START_LINE = re.compile(r"^START (\w+) (\d+) (\w+)$")
# A transcript still being written may end in the middle of a run.
QUIET_SECONDS = 300
NOTE_EVENT = "NoteForYourself"
NEOW_EVENT = "Neow Event"
NOTE_LOOKAHEAD = 40


def seed_from_text(text: str) -> int:
    value = 0
    for c in text.upper():
        value = value * len(ALPHABET) + ALPHABET.index(c)
    return value


def strip_state(line: str) -> str:
    """Drops what the bridge never reads from one game message."""
    if not line.startswith("{"):
        return line
    msg = json.loads(line)
    gs = msg.get("game_state")
    if gs is not None:
        if gs.get("screen_type") != "MAP":
            gs.pop("map", None)
        screen = gs.get("screen_state")
        if isinstance(screen, dict) and "event_id" in screen:
            screen.pop("body_text", None)
            if screen["event_id"] != NEOW_EVENT:
                for option in screen.get("options", []):
                    option.pop("text", None)
    return json.dumps(msg, separators=(",", ":"), ensure_ascii=False)


def card_key(card: dict) -> str:
    upgrades = card.get("upgrades", 0)
    return card["id"] + (f"+{upgrades}" if upgrades else "")


def state_items(gs: dict) -> collections.Counter:
    """What the player has beyond the class's starting kit, as scenario items."""
    items: collections.Counter = collections.Counter()
    character = gs.get("class", "IRONCLAD")
    for relic in gs.get("relics", []):
        if relic["id"] != STARTER_RELICS[character]:
            items["relic:" + relic["id"]] += 1
    starter = STARTER_DECKS[character].copy()
    if gs.get("ascension_level", 0) >= ASCENDERS_BANE_ASCENSION:
        starter["AscendersBane"] += 1
    deck = collections.Counter(card_key(c) for c in gs.get("deck", [])) - starter
    for key, count in deck.items():
        items["card:" + key] += count
    for potion in gs.get("potions", []):
        if potion["id"] != "Potion Slot":
            items["potion:" + potion["id"]] += 1
    return items


def scenario_items(scenario: str) -> collections.Counter:
    return collections.Counter(item for item in scenario.split(";") if item.split(":")[0] in ("relic", "card", "potion"))


def rebuild_scenario(gs: dict) -> str:
    items = list(state_items(gs).elements())
    keys = gs.get("keys", {})
    if keys and all(keys.values()):
        items.append("keys")
    return ";".join(items)


def known_scenarios(seed_text: str, mirror_dirs: Iterable[str], plans: dict[int, list[str]]) -> Iterator[tuple[str, str]]:
    for directory in mirror_dirs:
        path = os.path.join(directory, f"mirror-{seed_text}.jsonl")
        if not os.path.exists(path):
            continue
        with open(path, errors="replace") as f:
            for line in f:
                if '"t":"note"' in line and "scenario: " in line:
                    yield "mirror_log", json.loads(line)["note"].split("scenario: ", 1)[1]
                    break
    for scenario in plans.get(seed_from_text(seed_text), []):
        yield "plan", scenario


def load_plans(directories: Iterable[str]) -> dict[int, list[str]]:
    plans: dict[int, list[str]] = collections.defaultdict(list)
    for directory in directories:
        if not os.path.isdir(directory):
            continue
        for name in sorted(os.listdir(directory)):
            if not name.endswith(".tsv"):
                continue
            with open(os.path.join(directory, name)) as f:
                for line in f:
                    if line.startswith("#") or "\t" not in line:
                        continue
                    seed, scenario = line.rstrip("\n").split("\t", 1)
                    if seed.isdigit():
                        plans[int(seed)].append(scenario)
    return plans


def find_scenario(events: list[tuple[str, str]], seed_text: str, mirror_dirs: Iterable[str], plans: dict[int, list[str]]) -> tuple[str, str]:
    """A scenario run asks for the state straight after START, which shows the items."""
    clients = [i for i, (kind, _) in enumerate(events) if kind == "C"]
    if len(clients) < 2 or events[clients[1]][1] != "STATE":
        return "", "none"
    reply = next((text for kind, text in events[clients[1] + 1:] if kind == "G"), None)
    if reply is None:
        return "", "none"
    gs = json.loads(reply).get("game_state", {})
    shown = state_items(gs)
    for source, scenario in known_scenarios(seed_text, mirror_dirs, plans):
        if scenario_items(scenario) == shown:
            return scenario, source
    return rebuild_scenario(gs), "rebuilt"


def find_note_card(events: list[tuple[str, str]]) -> Optional[tuple[str, int]]:
    """The card the run took from Note For Yourself: whatever joins the deck once the event has come up."""
    states = (json.loads(text).get("game_state") or {} for kind, text in events if kind == "G" and text.startswith("{"))
    before: Optional[collections.Counter] = None
    seen_after = 0
    for gs in states:
        screen = gs.get("screen_state")
        if before is None:
            if isinstance(screen, dict) and screen.get("event_id") == NOTE_EVENT:
                before = collections.Counter(card_key(c) for c in gs.get("deck", []))
            continue
        seen_after += 1
        added = collections.Counter(card_key(c) for c in gs.get("deck", [])) - before
        if added:
            key = next(iter(added))
            name, _, upgrades = key.partition("+")
            return name, int(upgrades or 0)
        if seen_after > NOTE_LOOKAHEAD:
            return None
    return None


class Run:
    """One run of a transcript, from its START line to the next."""

    def __init__(self, source: str, index: int, character: str, asc: int, seed_text: str, start_time: float) -> None:
        self.source = source
        self.index = index
        self.character = character
        self.asc = asc
        self.seed_text = seed_text
        self.start_time = start_time
        self.end_time = start_time
        self.complete = False
        self.events: list[tuple[str, str]] = []

    @property
    def trace(self) -> str:
        return f"{self.source}#{self.index}"


def split_runs(lines: Iterable[str], source: str, quiet: bool) -> Iterator[Run]:
    """Yields runs in order. `quiet` means nothing more is coming, so the last run is complete too."""
    run: Optional[Run] = None
    index = -1
    for raw in lines:
        try:
            entry = json.loads(raw)
        except ValueError:
            continue  # a partial last line
        direction, line = entry["dir"], entry["line"]
        match = START_LINE.match(line) if direction == "client" else None
        if match:
            if run:
                run.complete = True
                yield run
            index += 1
            run = Run(source, index, match.group(1), int(match.group(2)), match.group(3), entry["t"])
        if run is None:
            continue
        run.end_time = entry["t"]
        if direction == "client":
            run.events.append(("C", line))
        elif direction == "game":
            run.events.append(("G", strip_state(line)))
        elif direction == "note":
            run.events.append(("N", line))
    if run:
        run.complete = quiet
        yield run


def write_run(run: Run, path: str, mirror_dirs: Iterable[str], plans: dict[int, list[str]]) -> None:
    scenario, source = find_scenario(run.events, run.seed_text, mirror_dirs, plans)
    note = find_note_card(run.events)
    header = {
        "trace": run.trace,
        "class": run.character,
        "seed": seed_from_text(run.seed_text),
        "seed_text": run.seed_text,
        "asc": run.asc,
        "scenario": scenario,
        "scenario_source": source,
        "note_card": note[0] if note else "",
        "note_upgrades": note[1] if note else 0,
        "start_time": run.start_time,
        "end_time": run.end_time,
    }
    tmp = path + ".tmp"
    with gzip.open(tmp, "wt", encoding="utf-8", compresslevel=6) as f:
        f.write(json.dumps(header) + "\n")
        for kind, text in run.events:
            f.write(f"{kind} {text}\n")
    os.replace(tmp, path)


def export_transcript(path: str, out: str, mirror_dirs: Iterable[str], plans: dict[int, list[str]]) -> int:
    """Writes the runs not exported yet and returns how many. A finished transcript is marked so it isn't read again."""
    source = os.path.basename(path)
    done = os.path.join(out, source + ".done")
    if os.path.exists(done):
        return 0
    quiet = time.time() - os.path.getmtime(path) > QUIET_SECONDS
    written = 0
    with open(path, errors="replace") as f:
        for run in split_runs(f, source, quiet):
            if not run.complete:
                continue
            target = os.path.join(out, f"{source.removesuffix('.jsonl')}_{run.index:03d}.trace.gz")
            if not os.path.exists(target):
                write_run(run, target, mirror_dirs, plans)
                written += 1
    if quiet:
        with open(done, "w"):
            pass
    return written


def main() -> None:
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--logs", default=os.path.join(here, "logs"))
    ap.add_argument("--mirror_logs", action="append", default=[], help="more folders with mirror-<seed>.jsonl")
    ap.add_argument("--plans", action="append", default=[], help="more folders with scenario plans")
    ap.add_argument("--out", default=os.path.join(here, "traces"))
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    mirror_dirs = [args.logs, *args.mirror_logs]
    plans = load_plans([os.path.join(here, "plans"), *args.plans])
    transcripts = sorted(name for name in os.listdir(args.logs) if name.startswith("transcript-") and name.endswith(".jsonl"))
    written = sum(export_transcript(os.path.join(args.logs, name), args.out, mirror_dirs, plans) for name in transcripts)
    print(f"{written} new recordings from {len(transcripts)} transcripts in {args.out}")


if __name__ == "__main__":
    main()
