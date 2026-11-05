#!/usr/bin/env python3
"""Compressed shard store for run data (decided 2026-11-04 13:16 UTC; see program.md "Current state").

A run-data dir is either the old layout (one raw float32 file per run: run_<id>.bin) or a shard layout:
  index.tsv            name <tab> shard <tab> offset <tab> nbytes   (one line per original run file, sorted by name)
  shard_00000.z ...    zlib (level 6) of the concatenated raw bytes of up to PER consecutive files (sorted by name)
plus any small side files (counts.tsv, ids_cache.tsv), copied unchanged.

Readers use three calls that mirror what they did with the old layout, so results are bit-identical by construction
(same bytes, same order): list_runs(d) == sorted(glob(d/run_*.bin)) as virtual paths, size(path) == os.path.getsize,
read(path) == np.fromfile(path, float32). Measured on 300 files: full rows 72.7x smaller, k32 subsamples 46.8x; zlib
decompresses at ~500 MB/s.

usage: runstore.py pack SRC_DIR DST_DIR [--per 1000] [--glob 'pol_*.bin']   (verifies every file round-trips before it returns)
       runstore.py verify SRC_DIR DST_DIR [--glob ..]   (byte-compare a shard dir against the per-file dir)
       runstore.py unpack SHARD_DIR OUT_DIR              (write the packed files back out as plain files)
"""
import argparse, glob, hashlib, os, shutil, sys, zlib
import numpy as np

_idx = {}      # dir -> {name: (shard, offset, nbytes)}
_cache = {}    # (dir, shard) -> bytes   (one shard kept: readers go through files in name order)


def _index(d):
    d = os.path.abspath(d)
    if d not in _idx:
        p = os.path.join(d, "index.tsv")
        _idx[d] = None
        if os.path.exists(p):
            m = {}
            for line in open(p):
                n, s, o, b = line.rstrip("\n").split("\t"); m[n] = (int(s), int(o), int(b))
            _idx[d] = m
    return _idx[d]


def is_sharded(d):
    return _index(d) is not None


def list_files(d):
    """All packed file paths of a shard dir (any name), sorted; old layout: every *.bin file."""
    m = _index(d)
    if m is None:
        return sorted(glob.glob(os.path.join(d, "*.bin")))
    return sorted(os.path.join(d, n) for n in m)


def unpack(src, dst):
    """Write every packed file of a shard dir back out as plain files (for readers that glob files, e.g. the policy
    trainers); byte-identical by construction."""
    os.makedirs(dst, exist_ok=True); n = 0
    for v in list_files(src):
        open(os.path.join(dst, os.path.basename(v)), "wb").write(read(v).tobytes()); n += 1
    for side in ("counts.tsv", "ids_cache.tsv"):
        if os.path.exists(os.path.join(src, side)):
            shutil.copyfile(os.path.join(src, side), os.path.join(dst, side))
    print(f"UNPACK {src} -> {dst}: {n} files")


def list_runs(d):
    """Sorted run file paths of a dir, real (old layout) or virtual (shards); same strings either way."""
    m = _index(d)
    if m is None:
        return sorted(glob.glob(os.path.join(d, "run_*.bin")))
    return sorted(os.path.join(d, n) for n in m if n.startswith("run_") and n.endswith(".bin"))


def _loc(path):
    d, n = os.path.split(path); m = _index(d)
    return (os.path.abspath(d), m[n]) if m is not None and n in m else (None, None)


def size(path):
    d, e = _loc(path)
    return os.path.getsize(path) if d is None else e[2]


def read(path, count=-1):
    """np.fromfile(path, float32, count) for either layout."""
    d, e = _loc(path)
    if d is None:
        return np.fromfile(path, dtype=np.float32, count=count)
    s, o, b = e; key = (d, s)
    if key not in _cache:
        _cache.clear()
        _cache[key] = zlib.decompress(open(os.path.join(d, f"shard_{s:05d}.z"), "rb").read())
    v = np.frombuffer(_cache[key], dtype=np.float32, count=b // 4, offset=o)
    return (v if count < 0 else v[:count]).copy()


def release():
    """Free the decompressed-shard cache (call once loading is done)."""
    _cache.clear()


def pack(src, dst, per=1000, pattern="run_*.bin"):
    files = sorted(glob.glob(os.path.join(src, pattern)))
    if not files:
        sys.exit(f"pack: no run_*.bin under {src}")
    os.makedirs(dst, exist_ok=True)
    lines = []; raw = 0; comp = 0
    for s in range(0, len(files), per):
        chunk = files[s:s + per]; parts = []; off = 0; k = s // per
        for f in chunk:
            b = open(f, "rb").read(); parts.append(b)
            lines.append(f"{os.path.basename(f)}\t{k}\t{off}\t{len(b)}\n"); off += len(b)
        blob = b"".join(parts); z = zlib.compress(blob, 6); raw += len(blob); comp += len(z)
        tmp = os.path.join(dst, f"shard_{k:05d}.z.tmp"); open(tmp, "wb").write(z); os.replace(tmp, tmp[:-4])
    for side in ("counts.tsv", "ids_cache.tsv"):
        if os.path.exists(os.path.join(src, side)):
            shutil.copyfile(os.path.join(src, side), os.path.join(dst, side))
    tmp = os.path.join(dst, "index.tsv.tmp"); open(tmp, "w").write("".join(lines)); os.replace(tmp, tmp[:-4])
    _idx.pop(os.path.abspath(dst), None)
    bad = verify(src, dst, pattern)
    print(f"PACK {src} -> {dst}: {len(files)} files, {(len(files) + per - 1) // per} shards, {raw / 1e6:.1f} MB -> "
          f"{comp / 1e6:.1f} MB ({raw / max(comp, 1):.1f}x), verify {'OK' if not bad else f'FAILED ({bad} files differ)'}")
    return bad


def verify(src, dst, pattern="run_*.bin"):
    files = sorted(glob.glob(os.path.join(src, pattern))); virt = list_files(dst)
    if [os.path.basename(f) for f in files] != [os.path.basename(f) for f in virt]:
        return max(1, abs(len(files) - len(virt)))
    bad = 0
    for f, v in zip(files, virt):
        if hashlib.sha1(open(f, "rb").read()).digest() != hashlib.sha1(read(v).tobytes()).digest():
            bad += 1
    return bad


if __name__ == "__main__":
    ap = argparse.ArgumentParser(); ap.add_argument("cmd", choices=["pack", "verify", "unpack"]); ap.add_argument("src"); ap.add_argument("dst")
    ap.add_argument("--per", type=int, default=1000); ap.add_argument("--glob", default="run_*.bin", help="files to pack (e.g. 'pol_*.bin')")
    a = ap.parse_args()
    if a.cmd == "pack":
        sys.exit(1 if pack(a.src, a.dst, a.per, a.glob) else 0)
    if a.cmd == "unpack":
        unpack(a.src, a.dst); sys.exit(0)
    bad = verify(a.src, a.dst, a.glob); print(f"VERIFY {a.src} vs {a.dst}: {'OK' if not bad else f'{bad} files differ'}"); sys.exit(1 if bad else 0)
