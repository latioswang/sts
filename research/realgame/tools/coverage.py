#!/usr/bin/env python3
"""How much of the game the real-game runs have exercised, as a score out of 100 with its history.

    python3 -I research/realgame/tools/coverage.py [--replay_results FILE] [--estimate] [--count_repeats]

Reads logs/transcript-*.jsonl, where a run starts at a client START line and counts up to GAME_OVER, and caches each
transcript's results in logs/coverage_cache. What the game contains comes from its jar, through coverage_report.py.
Writes reports/coverage.json and reports/coverage_history.tsv.

Without --replay_results the figures are marked unverified. With it, a run counts up to its commands_replayed (client
commands other than WAIT and STATE, START included), and a run with no row counts for nothing. A rerun of a seed plays the
same commands up to its first different decision, so that shared start isn't counted twice unless --count_repeats is given.
"""
import argparse
import collections
import glob
import gzip
import json
import multiprocessing
import os
import re
import sys
import time

REALGAME = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, REALGAME)  # python3 -I leaves the script's folder off the path
from coverage_report import FILLER_RELICS, game_universe  # noqa: E402

CACHE_VERSION = 3

# Targets. Below its target an item scores count/target, and a category is the mean of its items.
# A card has to be played often enough, upgraded, and in different situations, since most card bugs need an interaction
# (Strength, Weak, Vulnerable, several monsters) to show. These counts are judgement calls, not derived. Class runs are
# derived, by the rule of three: with no failure in n independent runs, the failure rate per run is below 3/n with 95%
# confidence, so 60 clean runs bound it at 5% and 300 at 1%. Reruns of a seed aren't independent, so only distinct seed and
# ascension pairs count towards this.
CARD_PLAYS, CARD_UPGRADED, CARD_SIGNATURES = 10, 5, 8
CARD_WEIGHTS = (0.5, 0.25, 0.25)
UNPLAYABLE_IN_HAND = 5  # curses and statuses, counted in turns held
RELIC_RUNS, POTION_USES, POWER_RUNS = 5, 5, 3
MONSTER_FIGHTS = 3  # in each ascension band
EVENT_VISITS = 3
EVENT_WEIGHTS = (0.5, 0.5)  # visits, share of options chosen
ASCENSION_RUNS, ASCENSION_FLOOR = 3, 17  # runs reaching that floor at each level
CLASS_RUNS, CLASS_RUNS_STRETCH = 60, 300
POINTS = {"cards": 35, "relics": 15, "potions": 5, "monsters": 15, "events": 10, "powers": 5, "ascensions": 5, "class_runs": 10}
HIGH_ASCENSION = 17  # most of the monsters' harder rules have started by here
STEP_SEC = 2 * 3600
SECONDS_PER_RUN = 240
RECENT_RUNS = 100
COMPLETED_ROW_TOLERANCE = 3  # commands a mirror row can differ from a run by, from the client's nudges
OK_REPLAY_STATUSES = {"completed", "trace_end", "passed", "pass", "ok"}
CLASSES = ("ironclad", "silent", "defect", "watcher")
JOBS = 4


def norm_label(text: str) -> str:
    return re.sub(r"\d+", "#", text or "").strip()


def has_power(entity: dict, power_id: str) -> bool:
    return any(p.get("id") == power_id for p in entity.get("powers", ()))


class RunScan:
    """Turns one run's lines into events, each [kind, command index, ...]. A state's events carry the commands sent before it."""

    def __init__(self, trace: str, parts: list, t: float):
        self.run = {"trace": trace, "class": parts[1].lower() if len(parts) > 1 else "?",
                    "asc": int(parts[2]) if len(parts) > 2 and parts[2].isdigit() else 0,
                    "seed": parts[3] if len(parts) > 3 else "", "t0": t, "t1": t, "n": 1, "n_all": 1, "over": None,
                    "victory": False, "bad_plays": 0, "cmds": [" ".join(parts)], "events": []}
        self.seen = set()
        self.raw, self.gs, self.pending = None, None, None
        self.fairy, self.last_head = 0, "START"

    def once(self, key: tuple, event: list) -> None:
        if key not in self.seen:
            self.seen.add(key)
            self.run["events"].append(event)

    def scan_state(self, gs: dict) -> None:
        k, floor = self.run["n"], gs.get("floor", 0)
        for relic in gs.get("relics", ()):
            self.once(("r", relic.get("id")), ["r", k, relic.get("id")])
        if floor >= ASCENSION_FLOOR:
            self.once(("f",), ["f", k])
        fairy = sum(p.get("id") == "FairyPotion" for p in gs.get("potions", ()))
        if fairy < self.fairy and self.last_head != "POTION":  # it saves the player by itself, with no command
            self.run["events"].append(["p", k, "FairyPotion"])
        self.fairy = fairy
        combat = gs.get("combat_state")
        if combat:
            monsters = [m for m in combat.get("monsters", ()) if not m.get("is_gone")]
            for m in monsters:
                self.once(("m", floor, m["id"]), ["m", k, m["id"]])
                self.once(("v", m["id"], m.get("move_id")), ["v", k, m["id"], m.get("move_id")])
            for who in monsters + [combat.get("player", {})]:
                for p in who.get("powers", ()):
                    power = re.sub(r"^TheBomb\d+$", "TheBomb", p["id"])  # each Bomb's id has a running number
                    self.once(("w", power), ["w", k, power])
            for c in combat.get("hand", ()):
                if c.get("type") in ("CURSE", "STATUS"):
                    self.once(("u", floor, combat.get("turn"), c.get("uuid")), ["u", k, c.get("id")])
        if gs.get("screen_type") == "EVENT":
            screen = gs.get("screen_state", {})
            eid = screen.get("event_id", "")
            self.once(("e", floor, eid), ["e", k, eid])
            for o in screen.get("options", ()):
                label = norm_label(o.get("label") or o.get("text"))
                self.once(("s", eid, label), ["s", k, eid, label])

    def flush(self) -> None:
        if self.pending:
            self.run["events"].append(self.pending)
            self.pending = None
        if self.raw is not None:
            msg = json.loads(self.raw)
            self.raw = None
            if msg.get("game_state"):
                self.gs = msg["game_state"]
                self.scan_state(self.gs)

    def play_event(self, parts: list):
        combat = self.gs.get("combat_state") if self.gs else None
        hand = combat.get("hand", []) if combat else []
        i = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else -1
        i = 9 if i == 0 else i - 1  # the mod takes 0 as the tenth card
        if not 0 <= i < len(hand):
            self.run["bad_plays"] += 1
            return None
        player, monsters = combat.get("player", {}), combat.get("monsters", [])
        target = monsters[int(parts[2])] if len(parts) > 2 and parts[2].isdigit() and int(parts[2]) < len(monsters) else {}
        alive = sum(1 for m in monsters if not m.get("is_gone") and m.get("current_hp", 0) > 0)
        strength = next((p.get("amount", 0) for p in player.get("powers", ()) if p.get("id") == "Strength"), 0)
        room = self.gs.get("room_type", "")
        bits = (strength > 0, has_power(player, "Weakened"), has_power(player, "Frail"), has_power(target, "Vulnerable"),
                has_power(target, "Weakened"), target.get("block", 0) > 0, alive > 1, room == "MonsterRoomElite",
                room == "MonsterRoomBoss", self.run["asc"] >= HIGH_ASCENSION)
        return ["c", self.run["n"], hand[i].get("id"), 1 if hand[i].get("upgrades", 0) > 0 else 0, sum(1 << b for b, on in enumerate(bits) if on)]

    def command(self, line: str, t: float) -> None:
        if self.run["over"] is not None:
            return
        self.flush()
        self.run["t1"] = t
        parts = line.split()
        head = parts[0].upper() if parts else ""
        if head == "STATE":
            return
        self.run["n_all"] += 1
        if head == "WAIT":
            return
        self.run["n"] += 1
        self.run["cmds"].append(line)
        self.last_head = head
        if head == "PLAY":
            self.pending = self.play_event(parts)
        elif head == "POTION" and len(parts) > 2 and parts[1].lower() == "use" and parts[2].isdigit() and self.gs:
            potions = self.gs.get("potions", [])
            if int(parts[2]) < len(potions):
                self.pending = ["p", self.run["n"], potions[int(parts[2])].get("id")]
        elif head == "CHOOSE" and len(parts) > 1 and parts[1].isdigit() and self.gs and self.gs.get("screen_type") == "EVENT":
            screen = self.gs.get("screen_state", {})
            for o in screen.get("options", ()):
                if o.get("choice_index") == int(parts[1]):
                    self.pending = ["h", self.run["n"], screen.get("event_id", ""), norm_label(o.get("label") or o.get("text"))]

    def game(self, line: str, t: float) -> None:
        if self.run["over"] is not None:
            return
        self.run["t1"] = t
        if len(line) < 2000 and '"error"' in line:
            self.pending = None  # the mod refused the command, so it didn't happen
            return
        if self.pending:
            self.run["events"].append(self.pending)
            self.pending = None
        self.raw = line
        if '"screen_type":"GAME_OVER"' in line:
            self.flush()
            self.run["over"] = self.run["n"]
            self.run["victory"] = bool(self.gs and self.gs.get("screen_state", {}).get("victory"))


def scan_transcript(path: str) -> list:
    name = os.path.basename(path)
    scans = []
    with open(path, errors="ignore") as f:
        for raw in f:
            try:
                row = json.loads(raw)
            except ValueError:
                continue  # the newest file can end in a partial line
            direction, line, t = row.get("dir"), row.get("line", ""), row.get("t", 0)
            if direction == "client" and line.startswith("START"):
                scans.append(RunScan("%s#%d" % (name, len(scans)), line.split(), t))
            elif direction == "client" and scans:
                scans[-1].command(line, t)
            elif direction == "game" and scans:
                scans[-1].game(line, t)
    for scan in scans:
        scan.flush()  # also keeps what the last lines of each run left unread
    return [scan.run for scan in scans]


def cached_scan(job: tuple) -> None:
    path, cache_path = job
    runs = scan_transcript(path)
    tmp = cache_path + ".tmp%d" % os.getpid()
    with gzip.open(tmp, "wt", compresslevel=3) as out:
        json.dump(runs, out)
    os.replace(tmp, cache_path)


def load_runs(logs: str, jobs: int) -> list:
    cache_dir = os.path.join(logs, "coverage_cache")
    os.makedirs(cache_dir, exist_ok=True)
    todo, caches = [], []
    for path in sorted(glob.glob(os.path.join(logs, "transcript-*.jsonl"))):
        st, base = os.stat(path), os.path.join(cache_dir, os.path.basename(path))
        cache_path = "%s.v%d.%d.%d.json.gz" % (base, CACHE_VERSION, st.st_size, st.st_mtime_ns)
        caches.append(cache_path)
        if not os.path.exists(cache_path):
            for old in glob.glob(base + ".*"):
                os.remove(old)
            todo.append((path, cache_path))
    if todo:
        print("parsing %d of %d transcripts" % (len(todo), len(caches)), file=sys.stderr)
        with multiprocessing.Pool(jobs) as pool:
            for _ in pool.imap_unordered(cached_scan, todo):
                pass
    runs = []
    for cache_path in caches:
        with gzip.open(cache_path, "rt") as f:
            runs.extend(json.load(f))
    return sorted(runs, key=lambda r: r["t0"])


def load_universe(jar: str, javap: str, logs: str) -> dict:
    st = os.stat(jar)
    cache = os.path.join(logs, "coverage_cache", "universe.v%d.%d.%d.json" % (CACHE_VERSION, st.st_size, st.st_mtime_ns))
    if not os.path.exists(cache):
        with open(cache, "w") as f:
            json.dump(game_universe(jar, javap), f)
    with open(cache) as f:
        return json.load(f)


class Tally:
    def __init__(self, universe: dict):
        self.universe = universe
        self.plays, self.upgraded, self.in_hand, self.relics = (collections.Counter() for _ in range(4))
        self.potions, self.fights, self.visits, self.powers = (collections.Counter() for _ in range(4))
        self.to_floor, self.class_runs, self.clean_all = (collections.Counter() for _ in range(3))
        self.sigs, self.moves, self.shown, self.chosen, self.clean = (collections.defaultdict(set) for _ in range(5))
        self.runs = 0

    def add_run(self, run: dict, skip: int, limit, clean: bool) -> None:
        self.runs += 1
        self.class_runs[run["class"]] += 1
        if clean:
            self.clean[run["class"]].add((run["seed"], run["asc"]))
            self.clean_all[run["class"]] += 1
        counters = {"u": self.in_hand, "r": self.relics, "p": self.potions, "e": self.visits, "w": self.powers}
        sets = {"v": self.moves, "s": self.shown, "h": self.chosen}
        band = "high" if run["asc"] >= HIGH_ASCENSION else "low"
        for e in run["events"]:
            kind, k = e[0], e[1]
            if k <= skip or (limit is not None and k > limit):
                continue
            if kind in counters:
                counters[kind][e[2]] += 1
            elif kind in sets:
                sets[kind][e[2]].add(e[3])
            elif kind == "c":
                self.plays[e[2]] += 1
                self.upgraded[e[2]] += e[3]
                self.sigs[e[2]].add(e[4])
            elif kind == "m":
                self.fights[(e[2], band)] += 1
            elif kind == "f":
                self.to_floor[run["asc"]] += 1

    def unmatched(self) -> dict:
        u = self.universe
        return {"cards": sorted((set(self.plays) | set(self.in_hand)) - set(u["cards"])),
                "relics": sorted(set(self.relics) - set(u["relics"]) - FILLER_RELICS),
                "potions": sorted(set(self.potions) - set(u["potions"])), "events": sorted(set(self.visits) - set(u["events"])),
                "monsters": sorted({m for m, _ in self.fights} - set(u["monsters"]))}

    def targets(self) -> dict:
        """{category: {item: {field: (count, target, weight)}}}"""
        u, cards = self.universe, {}
        for cid, info in u["cards"].items():
            if info["kind"] == "unplayable":
                cards[cid] = {"in_hand": (self.in_hand[cid], UNPLAYABLE_IN_HAND, 1.0)}
            else:
                cards[cid] = {"plays": (self.plays[cid], CARD_PLAYS, CARD_WEIGHTS[0]),
                              "upgraded": (self.upgraded[cid], CARD_UPGRADED, CARD_WEIGHTS[1]),
                              "signatures": (len(self.sigs.get(cid, ())), CARD_SIGNATURES, CARD_WEIGHTS[2])}
        return {"cards": cards, "relics": {r: {"runs": (self.relics[r], RELIC_RUNS, 1.0)} for r in u["relics"]},
                "potions": {p: {"uses": (self.potions[p], POTION_USES, 1.0)} for p in u["potions"]},
                "monsters": {"%s (%s)" % (m, b): {"fights": (self.fights[(m, b)], MONSTER_FIGHTS, 1.0)} for m in u["monsters"] for b in ("low", "high")},
                "events": {e: {"visits": (self.visits[e], EVENT_VISITS, EVENT_WEIGHTS[0]),
                               "options_chosen": (len(self.chosen.get(e, ())), max(1, len(self.shown.get(e, ()))), EVENT_WEIGHTS[1])} for e in u["events"]},
                "powers": {p: {"runs": (self.powers[p], POWER_RUNS, 1.0)} for p in u["powers"]},
                "ascensions": {"A%d" % a: {"runs_to_floor": (self.to_floor[a], ASCENSION_RUNS, 1.0)} for a in range(21)},
                "class_runs": {c: {"clean_distinct_seeds": (len(self.clean.get(c, ())), CLASS_RUNS, 1.0)} for c in CLASSES}}


def item_scores(targets: dict) -> dict:
    return {cat: {name: sum(w * min(1.0, c / n) for c, n, w in fields.values()) for name, fields in items.items()} for cat, items in targets.items()}


def category_scores(scores: dict) -> tuple:
    means = {cat: sum(v.values()) / len(v) if v else 0.0 for cat, v in scores.items()}
    return means, sum(POINTS[c] * means[c] for c in POINTS)


def remaining(targets: dict) -> dict:
    """What's still needed in each category, in the units the targets are counted in."""
    return {cat: sum(max(0, n - c) for fields in items.values() for c, n, _ in fields.values()) for cat, items in targets.items()}


def load_replay(path: str) -> dict:
    with open(path) as f:
        rows = [json.loads(line) for line in f if line.strip()]
    return {re.sub(r"(?<!\.jsonl)#", ".jsonl#", r["trace"], count=1): r for r in rows}


def load_clean_rows(logs: str) -> tuple:
    """Mirror's and live mode's own results say which runs reached the end with no stop, and so no filler moves."""
    completed, live = collections.defaultdict(list), set()
    roots = [logs] + glob.glob(os.path.expanduser("~/Projects/sts_bridge*/research/realgame/logs"))
    for name in ("mirror_results.jsonl", "live_results.jsonl"):  # not replay's results, which say nothing about the original run
        for path in (p for root in roots for p in glob.glob(os.path.join(root, "**", name), recursive=True)):
            with open(path) as f:
                for r in map(json.loads, f):
                    if r.get("status") == "completed":
                        completed[r["seed_text"]].append(r.get("commands", 0))
                    elif r.get("status") == "finished":
                        live.add(r["seed_text"])
    return completed, live


def is_clean(run: dict, replay, completed: dict, live: set) -> bool:
    if run["over"] is None:
        return False
    if replay is not None:
        r = replay.get(run["trace"])
        return bool(r) and r.get("status") in OK_REPLAY_STATUSES and run["over"] <= r.get("commands_replayed", 0)
    return run["seed"] in live or any(abs(c - run["n_all"]) <= COMPLETED_ROW_TOLERANCE for c in completed.get(run["seed"], ()))


def fresh_from(runs: list, count_repeats: bool) -> dict:
    """For each run, how many of its first commands an earlier run of the same seed and ascension already played."""
    skip, groups = {}, collections.defaultdict(list)
    for run in runs:
        group = groups[(run["class"], run["asc"], run["seed"])]
        shared = (len(os.path.commonprefix([prev, run["cmds"]])) for prev in group)
        skip[run["trace"]] = 0 if count_repeats else max(shared, default=0)
        group.append(run["cmds"])
    return skip


def iso(t: float) -> str:
    return time.strftime("%Y-%m-%dT%H:%M", time.localtime(t))


def build_history(runs: list, plan: list, universe: dict, path: str) -> list:
    t, end, i = runs[0]["t0"], time.time(), 0
    tally, rows = Tally(universe), []
    with open(path, "w") as out:
        out.write("\t".join(["time", "runs", "total"] + list(POINTS)) + "\n")
        while True:
            while i < len(runs) and runs[i]["t1"] <= t:
                tally.add_run(*plan[i])
                i += 1
            means, total = category_scores(item_scores(tally.targets()))
            out.write("\t".join([iso(t), str(tally.runs), "%.2f" % total] + ["%.2f" % (POINTS[c] * means[c]) for c in POINTS]) + "\n")
            rows.append((t, total))
            if t >= end:
                return rows
            t = min(t + STEP_SEC, end)


def class_view(t: Tally, scores: dict) -> dict:
    out = {}
    for cls in CLASSES:
        cards = [c for c, i in t.universe["cards"].items() if i["class"] == cls]
        relics = [r for r, owner in t.universe["relics"].items() if owner == cls]
        out[cls] = {"cards_score": round(sum(scores["cards"][c] for c in cards) / len(cards), 3), "cards": len(cards),
                    "cards_played": sum(1 for c in cards if t.plays[c]),
                    "relics_score": round(sum(scores["relics"][r] for r in relics) / len(relics), 3), "relics": len(relics),
                    "relics_seen": sum(1 for r in relics if t.relics[r]), "runs": t.class_runs[cls], "clean_runs": t.clean_all[cls],
                    "clean_distinct_seeds": len(t.clean.get(cls, ()))}
    return out


def write_report(path: str, t: Tally, targets: dict, scores: dict, means: dict, total: float, verified: bool, repeats_counted: bool, guess: dict) -> None:
    items = {cat: {name: dict({f: v[0] for f, v in fields.items()}, score=round(scores[cat][name], 3)) for name, fields in rows.items()} for cat, rows in targets.items()}
    for cid, info in t.universe["cards"].items():
        items["cards"][cid].update(info)
    for relic, owner in t.universe["relics"].items():
        items["relics"][relic]["class"] = owner
    report = {"generated": iso(time.time()), "verified": verified, "repeats_counted": repeats_counted, "total": round(total, 2), "runs": t.runs,
              "categories": {c: {"points": POINTS[c], "earned": round(POINTS[c] * means[c], 2), "score": round(means[c], 3), "items": len(scores[c]),
                                 "at_zero": sum(v == 0 for v in scores[c].values())} for c in POINTS},
              "classes": class_view(t, scores), "items": items, "unmatched": t.unmatched(), "estimate": guess,
              "monster_moves": {m: sorted(str(x) for x in t.moves.get(m, ())) for m in t.universe["monsters"]},
              "event_options": {e: {"shown": sorted(t.shown.get(e, ())), "chosen": sorted(t.chosen.get(e, ()))} for e in t.universe["events"]}}
    with open(path, "w") as f:
        json.dump(report, f, indent=1)


def print_summary(t: Tally, scores: dict, means: dict, total: float, verified: bool) -> None:
    if not verified:
        print("UNVERIFIED: no --replay_results, so nothing here confirms the simulator matched the game on what's counted.")
    print("Total %.1f / 100 from %d runs" % (total, t.runs))
    for c in sorted(POINTS, key=lambda c: means[c]):
        zero = sum(v == 0 for v in scores[c].values())
        print("  %-11s %5.1f / %-3d (%3.0f%%)  %d items, %d at zero" % (c, POINTS[c] * means[c], POINTS[c], 100 * means[c], len(scores[c]), zero))
    print("Classes:")
    for cls, v in class_view(t, scores).items():
        print("  %-9s cards %3.0f%% (%d of %d played), class relics %3.0f%% (%d of %d seen), runs %d, clean on distinct seeds %d (target %d, stretch %d)" % (
            cls, 100 * v["cards_score"], v["cards_played"], v["cards"], 100 * v["relics_score"], v["relics_seen"], v["relics"],
            v["runs"], v["clean_distinct_seeds"], CLASS_RUNS, CLASS_RUNS_STRETCH))
    for c in ("relics", "potions", "monsters", "events", "powers", "ascensions"):
        names = sorted(n for n, v in scores[c].items() if v == 0)
        if names:
            print("At zero, %s (%d): %s%s" % (c, len(names), ", ".join(names[:30]), " ..." if len(names) > 30 else ""))
    by_class = collections.Counter(t.universe["cards"][n]["class"] for n, v in scores["cards"].items() if v == 0)
    print("At zero, cards (%d): %s" % (sum(by_class.values()), ", ".join("%s %d" % kv for kv in by_class.most_common())))
    for cat, names in t.unmatched().items():
        if names:
            print("NOT IN THE GAME'S LISTS, %s: %s" % (cat, ", ".join(names[:30])))


def estimate(before: dict, now: dict, recent: int, history: list) -> dict:
    """A rough linear guess at what's left, by category and from the score's slope over the last day."""
    categories = {}
    for c in POINTS:
        rate = 1.0 if c == "class_runs" else max(0.0, (before[c] - now[c]) / max(1, recent))  # one clean run adds one, at best
        categories[c] = {"needs": now[c], "per_run": round(rate, 2), "runs": round(now[c] / rate) if rate > 0 else None}
    slowest = max(categories, key=lambda c: categories[c]["runs"] or 0)
    runs_left = categories[slowest]["runs"] or 0
    day = [h for h in history if h[0] >= history[-1][0] - 24 * 3600]
    slope = (day[-1][1] - day[0][1]) / ((day[-1][0] - day[0][0]) / 3600) if len(day) >= 2 and day[-1][0] > day[0][0] else None
    return {"categories": categories, "slowest": slowest, "runs_left": runs_left, "hours_left": round(runs_left * SECONDS_PER_RUN / 3600),
            "points_per_hour": round(slope, 2) if slope is not None else None,
            "hours_to_100": round((100 - day[-1][1]) / slope) if slope else None}


def print_estimate(info: dict) -> None:
    print("Estimate, a rough linear guess. The early items come fastest, so the real tail is longer, and much of it is what the")
    print("scenario mod has to force. Other classes' full runs also need the simulator ports and bridge support first.")
    for c, row in info["categories"].items():
        print("  %-11s still needs %6d, gaining %.2f a run: %s" % (c, row["needs"], row["per_run"], "%d runs" % row["runs"] if row["runs"] is not None else "no recent progress"))
    print("  slowest category with progress: %d more runs, about %d hours at %d seconds a run" % (info["runs_left"], info["hours_left"], SECONDS_PER_RUN))
    if info["points_per_hour"] is not None:
        left = "%d more hours to 100" % info["hours_to_100"] if info["points_per_hour"] > 0 else "flat"
        print("  score over the last day: %.2f points an hour, %s" % (info["points_per_hour"], left))


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--logs", default=os.path.join(REALGAME, "logs"))
    ap.add_argument("--reports", default=os.path.join(REALGAME, "reports"))
    ap.add_argument("--jar", default=os.environ.get("STS_GAME_JAR", os.path.expanduser("~/.sts_realgame/desktop-1.0.jar")))
    ap.add_argument("--javap", default=os.environ.get("JAVAP", os.path.expanduser("~/Projects/sts_lightspeed/.modbuild/tools/jdk8/Contents/Home/bin/javap")))
    ap.add_argument("--replay_results")
    ap.add_argument("--count_repeats", action="store_true")
    ap.add_argument("--estimate", action="store_true")
    args = ap.parse_args()
    os.nice(10)
    t_start = time.time()
    all_runs = load_runs(args.logs, JOBS)
    t_parsed = time.time()
    universe = load_universe(args.jar, args.javap, args.logs)
    universe["powers"] = sorted(set(universe["powers"]) | {e[2] for r in all_runs for e in r["events"] if e[0] == "w"})
    replay = load_replay(args.replay_results) if args.replay_results else None
    completed, live = load_clean_rows(args.logs)
    skip = fresh_from(all_runs, args.count_repeats)
    runs = sorted((r for r in all_runs if replay is None or r["trace"] in replay), key=lambda r: r["t1"])
    if not runs:
        sys.exit("no runs to count")
    plan = [(r, skip[r["trace"]], replay[r["trace"]].get("commands_replayed", 0) if replay else None, is_clean(r, replay, completed, live)) for r in runs]
    history = build_history(runs, plan, universe, os.path.join(args.reports, "coverage_history.tsv"))
    tally, recent, before = Tally(universe), min(RECENT_RUNS, len(plan)), None
    for i, p in enumerate(plan):
        if i == len(plan) - recent:
            before = remaining(tally.targets())
        tally.add_run(*p)
    targets = tally.targets()
    scores = item_scores(targets)
    means, total = category_scores(scores)
    guess = estimate(before, remaining(targets), recent, history)
    write_report(os.path.join(args.reports, "coverage.json"), tally, targets, scores, means, total, replay is not None, args.count_repeats, guess)
    print_summary(tally, scores, means, total, replay is not None)
    print("%d runs in the transcripts, %d distinct seed and ascension pairs, %d bad PLAY lookups%s" % (
        len(runs), len({(r["class"], r["asc"], r["seed"]) for r in runs}), sum(r["bad_plays"] for r in runs), ", repeats counted" if args.count_repeats else ""))
    if args.estimate:
        print_estimate(guess)
    print("took %.1fs, %.1fs of it parsing and reading the cache" % (time.time() - t_start, t_parsed - t_start))


if __name__ == "__main__":
    main()
