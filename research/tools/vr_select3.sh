#!/usr/bin/env bash
# Phase R2 selection + gate: lowest holdout RMSE among the given vr ids becomes vr_TARGET; gate: it must beat its own
# predict-mean holdout RMSE (meta field 7), else the rest of the queue is stashed to research/queue.gated.txt.
# usage: vr_select3.sh TARGET ID [ID ...]
set -euo pipefail
cd "$(dirname "$0")/../.."
T=$1; shift
BEST=$(for i in "$@"; do [ -f research/models/vr_$i.meta ] && echo "$(cut -d' ' -f1 research/models/vr_$i.meta) $i"; done | sort -g | head -1 | cut -d' ' -f2)
cp research/models/vr_$BEST.txt research/models/vr_$T.txt; cp research/models/vr_$BEST.meta research/models/vr_$T.meta
RMSE=$(cut -d' ' -f1 research/models/vr_$T.meta); BASE=$(cut -d' ' -f7 research/models/vr_$T.meta)
echo "VR SELECT: vr_$BEST -> vr_$T ($(cat research/models/vr_$T.meta))"
if python3 -c "import sys; sys.exit(0 if float('$RMSE') < float('$BASE') else 1)"; then echo "VR GATE OK: $RMSE < $BASE"
else echo "VR GATE FAILED: vr_$T does not beat predicting the mean -> rest of queue stashed"; mv research/queue.txt research/queue.gated.txt; touch research/queue.txt; fi
