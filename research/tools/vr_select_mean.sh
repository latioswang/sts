#!/usr/bin/env bash
# Phase F1 selection (pre-registered 2026-11-06): score each candidate vr id on every holdout dir and pick the lowest MEAN
# rmse -> vr_TARGET. Gate: it must beat its own predict-mean holdout rmse (meta field 7), else the rest of the queue is stashed.
# usage: vr_select_mean.sh TARGET "HOLD_DIR [HOLD_DIR ...]" ID [ID ...]
set -euo pipefail
cd "$(dirname "$0")/../.."
T=$1; HOLDS=$2; shift 2
best=""; bm=9
for i in "$@"; do
  s=0; n=0
  for h in $HOLDS; do
    r=$(python3 research/analysis/score_vr.py research/models/vr_$i.txt "$h" | tee /dev/stderr | sed -n 's/.*rmse=\([0-9.]*\).*/\1/p')
    s=$(python3 -c "print($s + $r)"); n=$((n + 1))
  done
  m=$(python3 -c "print(round($s / $n, 4))"); echo "VR MEAN vr_$i: $m over $n holdouts"
  python3 -c "import sys; sys.exit(0 if $m < $bm else 1)" && { bm=$m; best=$i; }
done
cp research/models/vr_$best.txt research/models/vr_$T.txt; cp research/models/vr_$best.meta research/models/vr_$T.meta
RMSE=$(cut -d' ' -f1 research/models/vr_$T.meta); BASE=$(cut -d' ' -f7 research/models/vr_$T.meta)
echo "VR SELECT: vr_$best -> vr_$T (mean holdout rmse $bm; meta $(cat research/models/vr_$T.meta))"
if python3 -c "import sys; sys.exit(0 if float('$RMSE') < float('$BASE') else 1)"; then echo "VR GATE OK: $RMSE < $BASE"
else echo "VR GATE FAILED: vr_$T does not beat predicting the mean -> rest of queue stashed"; mv research/queue.txt research/queue.gated.txt; touch research/queue.txt; fi
