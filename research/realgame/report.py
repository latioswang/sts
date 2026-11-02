#!/usr/bin/env python3
"""Turn mirror_results.jsonl into a divergence report: one section per seed, grouped by the first differing field.

    report.py [--log-dir DIR] [--out FILE]

Each stop is either a simulator bug or a bridge bug. `SUSPECTS` maps a first-diff field to where to look first; it is
a starting point, and a confirmed cause belongs in the report's "Confirmed causes" notes by hand.
"""
import argparse
import json
import os
from collections import Counter
from dataclasses import dataclass, field
from typing import Iterable

SUSPECTS: tuple[tuple[str, str], ...] = (
    ("screen", "screen flow: an alignment step in Mirror::alignOutOfCombat, or the game entered a room the simulator did not"),
    ("map", "map generation (mapRng)"),
    ("act_boss", "boss selection at run start"),
    ("event_id", "event selection (eventRng, event pool order)"),
    ("rewards", "combat rewards (gold, potionRng, relicRng)"),
    ("card_reward", "card reward generation (cardRng, rarity factor) or unlock state of the game's profile"),
    ("shop", "shop generation (merchantRng, prices)"),
    ("boss_relics", "boss relic pool order"),
    ("select_cards", "which cards the game offers on this selection screen"),
    ("monsters.count", "encounter selection (monsterRng) or summon handling"),
    ("monster", "monster HP roll, AI roll (aiRng), or move table in GameIdTables.inc"),
    ("draw_pile", "shuffle (shuffleRng) or card order"),
    ("hand", "draw order, or card cost handling"),
    ("discard_pile", "card movement order"),
    ("exhaust_pile", "card movement order"),
    ("player.current_hp", "damage calculation"),
    ("player.block", "block calculation"),
    ("player.energy", "energy or card cost"),
    ("player.powers", "a card, relic or monster effect, or the power table in GameIdTables.inc"),
    ("potions", "potion drops or use"),
    ("relics", "relic effect or pool"),
    ("deck", "a card reward, removal, upgrade or transform"),
    ("current_hp", "HP outside combat (rest, event, relic)"),
    ("max_hp", "max HP change"),
    ("gold", "gold reward or cost"),
    ("floor", "room transition"),
    ("run_end", "the run ended differently"),
)


@dataclass(frozen=True)
class Stop:
    seed: int
    seed_text: str
    status: str
    floor: int
    act: int
    step: int
    where: str
    last_action: str
    message: str
    diffs: tuple[dict, ...] = field(default_factory=tuple)

    @property
    def first_field(self) -> str:
        hard = [d for d in self.diffs if d.get("hard", True)]
        return hard[0]["field"] if hard else self.status


def suspect_for(field_name: str) -> str:
    for prefix, text in SUSPECTS:
        if field_name.startswith(prefix):
            return text
    return "unclassified"


def load_stops(path: str) -> list[Stop]:
    """The newest result per seed wins, so a fixed bridge bug drops out of the report on re-run."""
    latest: dict[int, Stop] = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            row = json.loads(line)
            latest[row["seed"]] = Stop(
                seed=row["seed"],
                seed_text=row["seed_text"],
                status=row["status"],
                floor=row.get("floor", 0),
                act=row.get("act", 0),
                step=row.get("step", 0),
                where=row.get("where", ""),
                last_action=row.get("last_action", ""),
                message=row.get("message", ""),
                diffs=tuple(row.get("diffs", [])),
            )
    return [latest[seed] for seed in sorted(latest)]


def render(stops: Iterable[Stop]) -> str:
    stops = list(stops)
    clean = [s for s in stops if s.status in ("completed", "reached_limit")]
    # Not simulator bugs: the game itself plays differently from run to run here (see Mirror::stopOnDifference).
    nondeterministic = [s for s in stops if s.status == "game_nondeterminism"]
    stopped = [s for s in stops if s not in clean and s not in nondeterministic]
    fields = Counter(s.first_field for s in stopped)

    lines = [
        "# Mirror divergence report",
        "",
        f"{len(stops)} seeds: {len(clean)} matched to their end point, {len(stopped)} stopped at a difference, "
        f"{len(nondeterministic)} stopped where the game itself is not deterministic.",
        "",
    ]
    if fields:
        lines += ["## First differences by field", "", "| Field | Seeds | Look first at |", "|---|---|---|"]
        lines += [f"| `{name}` | {count} | {suspect_for(name)} |" for name, count in fields.most_common()]
        lines.append("")
    if clean:
        lines += ["## Matched", "", "| Seed | Result | Floor | Steps |", "|---|---|---|---|"]
        lines += [f"| {s.seed} (`{s.seed_text}`) | {s.status} | {s.floor} | {s.step} |" for s in clean]
        lines.append("")
    if nondeterministic:
        lines += [
            "## Stopped where the game is not deterministic",
            "",
            "After a discovery, `cardRandomRng` depends on the frame rate until the next floor, so these runs say nothing about the simulator past that point.",
            "",
            "| Seed | Floor | Steps | First field |",
            "|---|---|---|---|",
        ]
        lines += [f"| {s.seed} (`{s.seed_text}`) | {s.floor} | {s.step} | `{s.first_field}` |" for s in nondeterministic]
        lines.append("")
    if stopped:
        lines += ["## Stops", ""]
    for s in stopped:
        lines += [
            f"### Seed {s.seed} (`{s.seed_text}`): {s.status} on floor {s.floor}, act {s.act}, step {s.step}",
            "",
            f"- Where: {s.where}",
            f"- Last action: `{s.last_action}`",
            f"- Repro: `sts_bridge mirror --seed_start {s.seed} --count 1`; log `logs/mirror-{s.seed_text}.jsonl`",
            f"- Suspected: {suspect_for(s.first_field)}",
        ]
        if s.message:
            lines.append(f"- Message: {s.message}")
        if s.diffs:
            lines += ["", "| Field | Simulator | Game |", "|---|---|---|"]
            lines += [f"| `{d['field']}`{'' if d.get('hard', True) else ' (soft)'} | `{d['sim']}` | `{d['game']}` |" for d in s.diffs]
        lines.append("")
    return "\n".join(lines)


def main() -> None:
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--log-dir", default=os.path.join(here, "logs"))
    ap.add_argument("--out", default=os.path.join(here, "reports", "divergences.md"))
    args = ap.parse_args()

    report = render(load_stops(os.path.join(args.log_dir, "mirror_results.jsonl")))
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        f.write(report)
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
