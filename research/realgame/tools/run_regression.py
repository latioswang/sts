#!/usr/bin/env python3
"""Checks the simulator against every recorded run, with no game running. Exits 1 if anything got worse.

    run_regression.py [--bridge PATH] [--traces DIR] [--jobs N] [--baseline FILE] [--results FILE]
                      [--update_baseline] [--skip_export] [--only TEXT] [--real_stops FILE]

Each recording is played back through mirror mode (`sts_bridge replay`), and its result is compared with the baseline:

- pass:    it reached the end of the recording with no difference.
- limited: it stopped early for a reason that says nothing about the simulator: a card the build can't play yet, a
           Discovery in a recording without the game's random streams, a move the bridge no longer sends, or a
           difference in a run whose scenario could only be rebuilt from what the game showed.
- fail:    the simulator differs from what the game did.

A recording made while the simulator had a bug ends at that bug, so it should pass once the bug is fixed.

Against the baseline, a run is worse if it now fails and didn't, or if it checks fewer commands. A known failure stays
in the baseline with the reason in its last column. --update_baseline fills that in for an older recording whose seed
later got further, since most of those came from older versions of the bridge (shorter waits, for one).
"""
import argparse
import glob
import concurrent.futures
import json
import os
import subprocess
import sys
import time
from typing import Iterable, NamedTuple, Optional

PASS_STATUSES = {"completed", "trace_end"}
LIMITED_STATUSES = {"unsupported_card", "game_nondeterminism", "no_rng", "trace_mismatch", "reached_limit", "past_real_stop"}
TRACES_PER_PROCESS = 8
MAX_REASONS_SHOWN = 12


class Row(NamedTuple):
    trace: str
    status: str
    commands: int
    note: str


def real_results(directories: Iterable[str]) -> dict[str, list[tuple[int, str]]]:
    """Each seed's runs on the game, as (commands sent by the time mirror mode stopped, status)."""
    found: dict[str, list[tuple[int, str]]] = {}
    for directory in directories:
        try:
            with open(os.path.join(directory, "mirror_results.jsonl")) as f:
                rows = [json.loads(line) for line in f if line.strip()]
        except OSError:
            continue
        for row in rows:
            if "seed_text" in row and "commands" in row:
                found.setdefault(row["seed_text"], []).append((int(row["commands"]), row["status"]))
    return found


def load_real_stops(path: str) -> dict[str, list[tuple[int, str]]]:
    """The real runs kept in git for seeds that stopped on a card the simulator couldn't play (see write_real_stops)."""
    found: dict[str, list[tuple[int, str]]] = {}
    try:
        with open(path) as f:
            for line in f:
                if not line.strip() or line.startswith("#"):
                    continue
                seed, commands, status = line.rstrip("\n").split("\t")
                found.setdefault(seed, []).append((int(commands), status))
    except OSError:
        pass
    return found


def write_real_stops(path: str, real: dict[str, list[tuple[int, str]]]) -> None:
    """Keeps every real run of each seed with a stop on an unsupported card. mark_past_real_stop needs them, and the
    mirror results in logs/ stay out of git, so without this file a fresh clone (or CI) would judge those seeds differently."""
    keep = {seed: sorted(set(runs)) for seed, runs in real.items() if any(status == "unsupported_card" for _, status in runs)}
    with open(path, "w") as f:
        f.write("# seed_text\tcommands\tstatus. Real mirror runs of seeds that stopped on a card the simulator couldn't play;"
                " written by run_regression.py --update_baseline\n")
        for seed in sorted(keep):
            for commands, status in keep[seed]:
                f.write(f"{seed}\t{commands}\t{status}\n")


def merge_real(*sources: dict[str, list[tuple[int, str]]]) -> dict[str, list[tuple[int, str]]]:
    merged: dict[str, set[tuple[int, str]]] = {}
    for source in sources:
        for seed, runs in source.items():
            merged.setdefault(seed, set()).update(runs)
    return {seed: sorted(runs) for seed, runs in merged.items()}


def mark_past_real_stop(results: list[dict], real: dict[str, list[tuple[int, str]]]) -> None:
    """A run on the game that stopped on a card the simulator couldn't play then still has to get to the end of the run, so
    finishRun picks the first option on each screen, with none of mirror mode's waits. Those commands were never compared
    and there's nothing to learn from them, so a recording that gets that far has passed what the game played."""
    for row in results:
        if row["status"] in PASS_STATUSES:
            continue
        total = row.get("commands_total", 0)
        runs = [r for r in real.get(row.get("seed_text", ""), []) if r[0] <= total]
        if not runs:
            continue
        commands, status = max(runs)
        if status == "unsupported_card" and row.get("commands_replayed", 0) >= commands:
            row["status"] = "past_real_stop"
            row["message"] = f"past where mirror mode stopped on the game, after {commands} commands: {row.get('message', '')}"


def kind_of(row: dict) -> str:
    status = row["status"]
    if status in PASS_STATUSES:
        return "pass"
    if status in LIMITED_STATUSES or row.get("scenario_source") == "rebuilt":
        return "limited"
    return "fail"


def later_runs_got_further(results: list[dict]) -> set[str]:
    """Failed recordings whose seed and ascension later ran past that floor. A trace's name sorts by time."""
    by_seed: dict[tuple, list[dict]] = {}
    for row in sorted(results, key=lambda r: r["file"]):
        by_seed.setdefault((row.get("seed"), row.get("asc")), []).append(row)
    found = set()
    for runs in by_seed.values():
        for i, row in enumerate(runs):
            floor = row.get("floor") or 0
            if kind_of(row) == "fail" and any((x.get("floor") or 0) > floor and kind_of(x) != "fail" for x in runs[i + 1 :]):
                found.add(row["trace"])
    return found


def chunks(items: list[str], size: int) -> Iterable[list[str]]:
    for i in range(0, len(items), size):
        yield items[i : i + size]


def replay_chunk(bridge: str, paths: list[str]) -> list[dict]:
    command = ["nice", "-n", "10", bridge, "replay"]
    for path in paths:
        command += ["--trace", path]
    done = subprocess.run(command, capture_output=True, text=True, errors="replace", check=False)
    rows = [json.loads(line) for line in done.stdout.splitlines() if line.startswith("{")]
    seen = {row.get("file") for row in rows}
    for path in paths:
        if path not in seen:
            # The process died on this one, which is a failure in itself.
            rows.append({"file": path, "trace": os.path.basename(path), "status": "replay_crash", "message": done.stderr[-300:]})
            break
    missing = [path for path in paths if path not in seen][1:]
    return rows + (replay_chunk(bridge, missing) if missing else [])


def load_baseline(path: str) -> dict[str, Row]:
    if not os.path.exists(path):
        return {}
    baseline: dict[str, Row] = {}
    with open(path) as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            trace, status, commands, note = (line.rstrip("\n").split("\t") + [""])[:4]
            baseline[trace] = Row(trace, status, int(commands), note)
    return baseline


def write_baseline(path: str, results: list[dict], old: dict[str, Row]) -> None:
    superseded = later_runs_got_further(results)
    with open(path, "w") as f:
        f.write("# trace\tstatus\tcommands checked\twhy, for a known failure\n")
        for row in sorted(results, key=lambda r: r["trace"]):
            kept = old.get(row["trace"])
            note = kept.note if kept and kept.note and kept.status == row["status"] else ""
            if not note and kind_of(row) == "fail":
                note = "older recording; this seed later got further" if row["trace"] in superseded else "not looked into"
            f.write(f"{row['trace']}\t{row['status']}\t{row.get('commands_replayed', 0)}\t{note}\n")


def compare(row: dict, known: Optional[Row]) -> Optional[str]:
    """Why this run is worse than the baseline, if it is."""
    commands = row.get("commands_replayed", 0)
    if known is None:
        return "new failure" if kind_of(row) == "fail" else None
    if kind_of(row) == "fail" and known.status != row["status"] and known.status in PASS_STATUSES | LIMITED_STATUSES:
        return f"was {known.status}, now {row['status']}"
    if commands < known.commands:
        return f"checks {commands} commands, was {known.commands}"
    return None


def describe(row: dict) -> str:
    diffs = "; ".join(f"{d['field']}: sim {str(d['sim'])[:90]} / game {str(d['game'])[:90]}" for d in row.get("diffs", [])[:3])
    return (f"{row['trace']} seed {row.get('seed_text', '?')} A{row.get('asc', '?')} {row['status']} floor {row.get('floor', '?')} "
            f"at {row.get('where', '')} after [{row.get('last_action', '')}] {row.get('message', '')[:160]} {diffs}")


def main() -> int:
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bridge", default=os.environ.get("STS_BRIDGE", os.path.expanduser("~/Projects/sts_lightspeed/build/sts_bridge")))
    ap.add_argument("--traces", default=os.path.join(here, "traces"))
    ap.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) // 2))
    ap.add_argument("--baseline", default=os.path.join(here, "regression_baseline.tsv"))
    ap.add_argument("--results", default=os.path.join(here, "logs", "replay_results.jsonl"))
    ap.add_argument("--update_baseline", action="store_true")
    ap.add_argument("--real_stops", default=os.path.join(here, "regression_real_stops.tsv"),
                    help="real runs kept in git for the seeds that stopped on an unsupported card; --update_baseline rewrites it")
    ap.add_argument("--skip_export", action="store_true", help="don't look for new transcripts first")
    ap.add_argument("--only", default="", help="only recordings whose file name contains this")
    ap.add_argument("--mirror_logs", action="append", default=None,
                    help="a logs folder holding mirror_results.jsonl; default this checkout's and each sts_bridge_* one's")
    args = ap.parse_args()

    # A fresh clone has the recordings (recordings.py unpack) but not the raw transcripts in logs/, which stay out of git.
    transcripts = glob.glob(os.path.join(here, "logs", "transcript-*.jsonl"))
    if not args.skip_export and transcripts:
        subprocess.run([sys.executable, "-I", os.path.join(here, "tools", "export_traces.py"), "--out", args.traces], check=True, stdout=subprocess.DEVNULL)
    elif not args.skip_export:
        print("no transcripts in logs/, so replaying the recordings already in traces/", file=sys.stderr)

    if not os.path.isdir(args.traces) or not any(n.endswith(".trace.gz") for n in os.listdir(args.traces)):
        sys.exit(f"no recordings in {args.traces}: fill it with  python3 -I {os.path.join(here, 'tools', 'recordings.py')} unpack")
    if not os.path.exists(args.bridge):
        sys.exit(f"no sts_bridge at {args.bridge}: build it (see USAGE.md) and pass --bridge or set STS_BRIDGE")
    os.makedirs(os.path.dirname(os.path.abspath(args.results)), exist_ok=True)

    paths = sorted(os.path.join(args.traces, n) for n in os.listdir(args.traces) if n.endswith(".trace.gz") and args.only in n)
    started = time.time()
    results: list[dict] = []
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        for rows in pool.map(lambda c: replay_chunk(args.bridge, c), chunks(paths, TRACES_PER_PROCESS)):
            results += rows
    seconds = time.time() - started
    for row in results:
        row.setdefault("trace", os.path.basename(row["file"]))
    logs = args.mirror_logs
    if logs is None:
        logs = [os.path.join(os.path.dirname(os.path.abspath(args.traces)), "logs"), os.path.join(here, "logs")]
        logs += glob.glob(os.path.expanduser("~/Projects/sts_bridge_*/research/realgame/logs"))
    real = merge_real(real_results(sorted(set(logs))), load_real_stops(args.real_stops))
    mark_past_real_stop(results, real)

    with open(args.results, "w") as f:
        for row in results:
            f.write(json.dumps(row) + "\n")

    baseline = load_baseline(args.baseline)
    by_kind = {"pass": 0, "limited": 0, "fail": 0}
    statuses: dict[str, int] = {}
    for row in results:
        by_kind[kind_of(row)] += 1
        statuses[row["status"]] = statuses.get(row["status"], 0) + 1
    worse = [(row, why) for row in results if (why := compare(row, baseline.get(row["trace"])))]
    checked = sum(row.get("commands_replayed", 0) for row in results)
    total = sum(row.get("commands_total", 0) for row in results)

    print(f"{len(results)} recordings in {seconds:.1f}s on {args.jobs} jobs: {by_kind['pass']} pass, {by_kind['limited']} limited, {by_kind['fail']} fail")
    print(f"commands checked: {checked} of {total}")
    print("by status: " + ", ".join(f"{k} {v}" for k, v in sorted(statuses.items(), key=lambda kv: -kv[1])))
    known_failures = [row for row in results if kind_of(row) == "fail" and row["trace"] in baseline]
    if known_failures:
        print(f"known failures still failing: {len(known_failures)}")
    for row, why in worse[:MAX_REASONS_SHOWN]:
        print(f"WORSE ({why}): {describe(row)}")
    if len(worse) > MAX_REASONS_SHOWN:
        print(f"... and {len(worse) - MAX_REASONS_SHOWN} more, see {args.results}")

    if args.update_baseline:
        write_baseline(args.baseline, results, baseline)
        write_real_stops(args.real_stops, real)
        print(f"baseline written to {args.baseline}, real stops to {args.real_stops}")
        return 0
    return 1 if worse else 0


if __name__ == "__main__":
    sys.exit(main())
