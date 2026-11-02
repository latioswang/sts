#!/usr/bin/env python3
"""Keep K random logged states per run (Phase R2): full run data at 10^5 runs (~43 GB) does not fit in RAM; train_vr2
already samples 16 rows per run per epoch, so K=32 fixed rows per run lose little. Rows are chosen with
RandomState(run_id % 2**32) without replacement (all rows if a run has fewer). usage: subsample_runs.py IN_DIR OUT_DIR [--k 32]"""
import argparse, glob, os
import numpy as np
DIM, EXTRA = 666, 4
ap = argparse.ArgumentParser(); ap.add_argument("inp"); ap.add_argument("out"); ap.add_argument("--k", type=int, default=32)
a = ap.parse_args(); os.makedirs(a.out, exist_ok=True); n = rows_in = rows_out = 0
for f in sorted(glob.glob(os.path.join(a.inp, "run_*.bin"))):
    x = np.fromfile(f, dtype=np.float32); x = x[: (x.size // (DIM + EXTRA)) * (DIM + EXTRA)].reshape(-1, DIM + EXTRA)
    if len(x) == 0: continue
    rid = int(x[0, DIM + 3]); idx = np.arange(len(x))
    if len(x) > a.k: idx = np.sort(np.random.RandomState(rid % (2 ** 32)).choice(len(x), a.k, replace=False))
    x[idx].tofile(os.path.join(a.out, os.path.basename(f))); n += 1; rows_in += len(x); rows_out += len(idx)
print(f"SUBSAMPLE {a.inp} -> {a.out}: {n} runs, rows {rows_in} -> {rows_out} (k={a.k})")
