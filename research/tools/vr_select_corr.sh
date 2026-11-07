#!/usr/bin/env bash
# Phase F16 selection (pre-registered in 9fa659a): score each candidate on the holdout dir with score_vr3.py and pick the
# HIGHEST correlation -> vr_TARGET (copies model + meta). usage: vr_select_corr.sh TARGET HOLD_DIR ID [ID ...]
set -euo pipefail
cd "$(dirname "$0")/../.."
T=$1; H=$2; shift 2; best=""; bc=-9
for i in "$@"; do
  c=$(python3 research/analysis/score_vr3.py research/models/vr_$i.txt "$H" | tee /dev/stderr | sed -n 's/.*corr=\([-0-9.na]*\).*/\1/p')
  echo "VR CORR vr_$i: $c"
  python3 -c "import sys; sys.exit(0 if float('$c') > $bc else 1)" && { bc=$c; best=$i; }
done
cp research/models/vr_$best.txt research/models/vr_$T.txt; cp research/models/vr_$best.meta research/models/vr_$T.meta
echo "VR SELECT (corr): vr_$best -> vr_$T (holdout corr $bc)"
