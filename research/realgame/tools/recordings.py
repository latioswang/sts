#!/usr/bin/env python3
"""Keeps the replay recordings in git as one compressed archive per day.

    recordings.py pack      writes recordings/<YYYYMMDD>.tar.zst for each finished day that has none yet
    recordings.py unpack    puts every archived run back in traces/, where the replay tools read them
    recordings.py check     lists finished days with runs in traces/ that no archive holds

traces/ has one gzipped file per run and is gitignored. An archive holds a day's runs uncompressed in one tar, compressed
with zstd --ultra -22 --long=31. Runs repeat each other's states, so compressing a day's runs together makes them about
3.4 times smaller than gzipping each one: on 2026-11-05, 526 runs were 1.94 GB raw, 43 MB gzipped and 12.7 MB archived.

A day is finished once the local date has moved past it, since transcript names carry the local time their game started.
Its archive is written once and then left alone, so git never stores a second copy; --force rewrites it. Every archive
is decompressed and compared with its source files before it replaces anything. zstd must be on the PATH, and reading an
archive takes up to 2 GB of memory for the long window.
"""
import argparse
import datetime
import gzip
import hashlib
import io
import os
import re
import shutil
import subprocess
import sys
import tarfile
from typing import Iterable, Iterator

REALGAME = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TRACES = os.path.join(REALGAME, "traces")
ARCHIVES = os.path.join(REALGAME, "recordings")
RUN_NAME = re.compile(r"^transcript-(\d{8})-\d{6}_\d{3}\.trace\.gz$")
MEMBER_NAME = re.compile(r"^transcript-\d{8}-\d{6}_\d{3}\.trace$")
# Each thread at this level keeps its own large window, and a game batch may be starting games meanwhile, so four threads.
ZSTD_PACK = ["--ultra", "-22", "--long=31", "-T4"]
ZSTD_READ = ["--long=31"]


def day_of(name: str) -> str | None:
    match = RUN_NAME.match(name)
    return match.group(1) if match else None


def runs_by_day(traces: str) -> dict[str, list[str]]:
    days: dict[str, list[str]] = {}
    for name in sorted(os.listdir(traces)):
        day = day_of(name)
        if day:
            days.setdefault(day, []).append(name)
    return days


def finished(days: Iterable[str], today: str) -> list[str]:
    return sorted(d for d in days if d < today)


def archive_path(archives: str, day: str) -> str:
    return os.path.join(archives, f"{day}.tar.zst")


def zstd() -> str:
    path = shutil.which("zstd")
    if not path:
        raise SystemExit("zstd isn't on the PATH; install it (brew install zstd, apt install zstd)")
    return path


def member_name(run: str) -> str:
    return run.removesuffix(".gz")


def read_run(traces: str, run: str) -> bytes:
    with gzip.open(os.path.join(traces, run), "rb") as f:
        return f.read()


def write_archive(traces: str, runs: list[str], target: str) -> None:
    """Streams the runs, uncompressed and in name order, through zstd into target."""
    with open(target, "wb") as out:
        proc = subprocess.Popen([zstd(), "-q", *ZSTD_PACK, "-c", "-"], stdin=subprocess.PIPE, stdout=out)
        assert proc.stdin is not None
        try:
            with tarfile.open(fileobj=proc.stdin, mode="w|", format=tarfile.PAX_FORMAT) as tar:
                for run in runs:
                    data = read_run(traces, run)
                    info = tarfile.TarInfo(member_name(run))
                    info.size, info.mode, info.mtime = len(data), 0o644, 0
                    tar.addfile(info, io.BytesIO(data))
        finally:
            proc.stdin.close()
            if proc.wait() != 0:
                raise RuntimeError(f"zstd failed writing {target}")


def archived_runs(path: str) -> Iterator[tuple[str, bytes]]:
    """Yields (member name, content) for each run in an archive, refusing any name that isn't a plain run name."""
    proc = subprocess.Popen([zstd(), "-q", "-d", *ZSTD_READ, "-c", path], stdout=subprocess.PIPE)
    assert proc.stdout is not None
    try:
        with tarfile.open(fileobj=proc.stdout, mode="r|") as tar:
            for info in tar:
                if not info.isfile() or not MEMBER_NAME.match(info.name):
                    raise ValueError(f"{path}: unexpected member {info.name!r}")
                f = tar.extractfile(info)
                assert f is not None
                yield info.name, f.read()
    finally:
        proc.stdout.close()
        if proc.wait() != 0:
            raise RuntimeError(f"zstd failed reading {path}")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def verify(traces: str, runs: list[str], path: str) -> None:
    want = {member_name(r): digest(read_run(traces, r)) for r in runs}
    got = {name: digest(data) for name, data in archived_runs(path)}
    if got != want:
        missing, extra = sorted(set(want) - set(got)), sorted(set(got) - set(want))
        changed = sorted(n for n in set(want) & set(got) if want[n] != got[n])
        raise RuntimeError(f"{path} doesn't match traces/: missing {missing[:3]}, extra {extra[:3]}, changed {changed[:3]}")


def pack(traces: str, archives: str, today: str, force: bool = False) -> list[str]:
    os.nice(19)  # below a game batch: a slow start makes run_batch.sh give up on the game
    os.makedirs(archives, exist_ok=True)
    written = []
    days = runs_by_day(traces)
    for day in finished(days, today):
        target = archive_path(archives, day)
        if os.path.exists(target) and not force:
            continue
        partial = target + ".partial"
        write_archive(traces, days[day], partial)
        verify(traces, days[day], partial)
        os.replace(partial, target)
        written.append(target)
        print(f"{day}: {len(days[day])} runs, {os.path.getsize(target) / 1e6:.1f} MB")
    return written


def unpack(traces: str, archives: str) -> int:
    os.makedirs(traces, exist_ok=True)
    added = 0
    for name in sorted(os.listdir(archives)):
        if not name.endswith(".tar.zst"):
            continue
        for member, data in archived_runs(os.path.join(archives, name)):
            target = os.path.join(traces, member + ".gz")
            if os.path.exists(target):
                continue
            with gzip.open(target + ".partial", "wb") as f:
                f.write(data)
            os.replace(target + ".partial", target)
            added += 1
    print(f"{added} runs added to {traces}")
    return added


def check(traces: str, archives: str, today: str) -> list[str]:
    """Finished days whose runs in traces/ aren't all in that day's archive."""
    short = []
    days = runs_by_day(traces)
    for day in finished(days, today):
        path = archive_path(archives, day)
        held = {name for name, _ in archived_runs(path)} if os.path.exists(path) else set()
        missing = [r for r in days[day] if member_name(r) not in held]
        if missing:
            short.append(day)
            print(f"{day}: {len(missing)} of {len(days[day])} runs not archived")
    return short


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["pack", "unpack", "check"])
    ap.add_argument("--traces", default=TRACES)
    ap.add_argument("--archives", default=ARCHIVES)
    ap.add_argument("--force", action="store_true", help="pack: rewrite archives that already exist")
    args = ap.parse_args()
    today = datetime.date.today().strftime("%Y%m%d")
    if args.command == "pack":
        pack(args.traces, args.archives, today, args.force)
        return 0
    if args.command == "unpack":
        unpack(args.traces, args.archives)
        return 0
    return 1 if check(args.traces, args.archives, today) else 0


if __name__ == "__main__":
    sys.exit(main())
