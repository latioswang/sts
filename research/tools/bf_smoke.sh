#!/usr/bin/env bash
# Smoke test for the interventional fight logger: 2 train-seed games with bf_log=1, then row-count and
# label sanity checks. Exit 1 on failure (the queue line that follows is chained with &&).
set -euo pipefail
cd "$(dirname "$0")/../.."
D=research/vf/data/bf_smoke; rm -rf "$D"; mkdir -p "$D"
ARGS=$(sed 's/^/--param /' research/best_params.txt | tr '\n' ' ')
S=$(date +%s)
VF_LOG_DIR=$D /home/user/sts_lightspeed/build/fair_eval --agent fair --seed_start 300000 --count 2 --asc 0 --threads 2 $ARGS --param bf_log=1 > "$D/games.tsv"
echo "smoke games: $(( $(date +%s) - S ))s"; cat "$D/games.tsv"
python3 - "$D" <<'PY'
import sys, glob, numpy as np
DIM = 666
v1 = glob.glob(sys.argv[1] + "/bg_*.bin"); files = v1 or glob.glob(sys.argv[1] + "/bf_*.bin")
O, EXTRA = (8, 12) if v1 else (4, 8)
a = np.concatenate([np.fromfile(f, dtype=np.float32) for f in files])
n = a.size // (DIM + EXTRA); a = a[: n * (DIM + EXTRA)].reshape(n, DIM + EXTRA)
Y = a[:, DIM:DIM + O]; base = a[:, DIM + O + 3] > 0.5; kind = a[:, DIM + O + 2]
names = ["boss_win", "boss_hp", "hall_win", "hall_hp", "boss_dmg", "next_win", "next_hp", "next_dmg"][:O]
print(f"rows={n} base={int(base.sum())} kinds={[int((kind == k).sum()) for k in (0, 1, 2)]} finite={np.isfinite(a).all()} " +
      " ".join(f"{nm}={Y[Y[:, j] >= 0, j].mean():.3f}" for j, nm in enumerate(names)))
assert n >= 40 and base.sum() >= 10 and np.isfinite(a).all(), "bf smoke failed"
assert (Y <= 1.0).all() and (Y >= -1.0).all()
print("bf smoke OK")
PY
