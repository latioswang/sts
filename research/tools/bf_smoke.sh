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
DIM, EXTRA = 666, 8
a = np.concatenate([np.fromfile(f, dtype=np.float32) for f in glob.glob(sys.argv[1] + "/bf_*.bin")])
n = a.size // (DIM + EXTRA); a = a[: n * (DIM + EXTRA)].reshape(n, DIM + EXTRA)
Y = a[:, DIM:DIM + 4]; base = a[:, DIM + 7] > 0.5; kind = a[:, DIM + 6]
print(f"rows={n} base={int(base.sum())} kinds={[int((kind == k).sum()) for k in (0, 1, 2)]} boss_win={Y[:,0].mean():.3f} hall_win={Y[Y[:,2]>=0,2].mean():.3f} finite={np.isfinite(a).all()}")
assert n >= 40 and base.sum() >= 10 and np.isfinite(a).all(), "bf smoke failed"
assert 0.0 <= Y[:, 0].mean() <= 1.0 and (Y[:, 1] <= 1.0).all()
print("bf smoke OK")
PY
