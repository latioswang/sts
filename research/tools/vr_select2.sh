#!/usr/bin/env bash
# Phase R selection + gate (pre-registered amendment, see program.md "Phase R amendment"): the lowest holdout RMSE
# among the v1 grid (ids 1-9) and the v2 grid (ids 11-22) becomes vr_10. Gate: vr_10 must beat predicting the mean on
# the same holdout (field 7 of the v2 metas); otherwise the remaining queue (leak test + integration) is stashed to
# research/queue.gated.txt and nothing is played with a model worse than a constant. usage: vr_select2.sh
set -euo pipefail
cd "$(dirname "$0")/../.."
BEST=$(for i in 1 2 3 4 5 6 7 8 9 11 12 13 14 15 16 17 18 19 20 21 22; do [ -f research/models/vr_$i.meta ] && echo "$(cut -d' ' -f1 research/models/vr_$i.meta) $i"; done | sort -g | head -1 | cut -d' ' -f2)
cp research/models/vr_$BEST.txt research/models/vr_10.txt; cp research/models/vr_$BEST.meta research/models/vr_10.meta
RMSE=$(cut -d' ' -f1 research/models/vr_10.meta)
BASE=$(cat research/models/vr_1[1-9].meta research/models/vr_2[0-2].meta 2>/dev/null | awk 'NF>=7{print $7; exit}')
echo "VR SELECT: vr_$BEST -> vr_10 ($(cat research/models/vr_10.meta)); predict-mean holdout RMSE $BASE"
if [ -z "$BASE" ] || ! python3 -c "import sys; sys.exit(0 if float('$RMSE') < float('$BASE') else 1)"; then
  echo "VR GATE FAILED: vr_10 does not beat predicting the mean -> integration stashed"
  mv research/queue.txt research/queue.gated.txt; touch research/queue.txt
else
  echo "VR GATE OK: $RMSE < $BASE"
fi
