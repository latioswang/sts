"""Tests for recordings.py: day grouping, packing only finished days, and a round trip through zstd."""
import gzip
import io
import os
import shutil
import subprocess
import tarfile

import pytest

import recordings

needs_zstd = pytest.mark.skipif(shutil.which("zstd") is None, reason="zstd isn't installed")


def write_run(folder, name, text):
    with gzip.open(os.path.join(folder, name), "wb") as f:
        f.write(text.encode())


def test_day_of_reads_the_transcript_date():
    assert recordings.day_of("transcript-20261105-123456_002.trace.gz") == "20261105"
    assert recordings.day_of("transcript-20261105-123456.jsonl.done") is None
    assert recordings.day_of("notes.txt") is None


def test_only_days_before_today_are_finished():
    assert recordings.finished(["20261107", "20261105", "20261106"], "20261107") == ["20261105", "20261106"]


@needs_zstd
def test_pack_then_unpack_gives_back_every_run(tmp_path):
    traces, archives, restored = tmp_path / "traces", tmp_path / "recordings", tmp_path / "restored"
    traces.mkdir()
    write_run(traces, "transcript-20261105-100000_000.trace.gz", "{\"seed\": 1}\nC END\n")
    write_run(traces, "transcript-20261105-100000_001.trace.gz", "{\"seed\": 2}\nC END\n")
    write_run(traces, "transcript-20261107-090000_000.trace.gz", "{\"seed\": 3}\n")
    written = recordings.pack(str(traces), str(archives), today="20261107")
    assert [os.path.basename(p) for p in written] == ["20261105.tar.zst"]  # today's run waits
    assert recordings.pack(str(traces), str(archives), today="20261107") == []  # written once
    assert recordings.check(str(traces), str(archives), today="20261107") == []
    assert recordings.unpack(str(restored), str(archives)) == 2
    with gzip.open(restored / "transcript-20261105-100000_001.trace.gz", "rt") as f:
        assert f.read() == "{\"seed\": 2}\nC END\n"
    assert recordings.unpack(str(restored), str(archives)) == 0  # nothing new


@needs_zstd
def test_check_reports_a_finished_day_without_an_archive(tmp_path):
    traces, archives = tmp_path / "traces", tmp_path / "recordings"
    traces.mkdir()
    archives.mkdir()
    write_run(traces, "transcript-20261104-100000_000.trace.gz", "x\n")
    assert recordings.check(str(traces), str(archives), today="20261107") == ["20261104"]


@needs_zstd
def test_unpack_refuses_a_member_outside_the_run_names(tmp_path):
    archives = tmp_path / "recordings"
    archives.mkdir()
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w") as tar:
        info = tarfile.TarInfo("../escape.trace")
        info.size = 1
        tar.addfile(info, io.BytesIO(b"x"))
    path = archives / "20261101.tar.zst"
    path.write_bytes(subprocess.run(["zstd", "-q", "-c", "-"], input=raw.getvalue(), capture_output=True, check=True).stdout)
    with pytest.raises(ValueError):
        recordings.unpack(str(tmp_path / "traces"), str(archives))
    assert not (tmp_path / "escape.trace").exists()
