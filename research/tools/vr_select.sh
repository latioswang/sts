#!/usr/bin/env bash
# Phase R: copy the Phase R grid model (ids 1-9) with the lowest holdout RMSE to vr_10 (the model used by the
# integration experiments; selection rule pre-registered). usage: vr_select.sh
set -euo pipefail
cd "$(dirname "$0")/../.."
BEST=$(for i in 1 2 3 4 5 6 7 8 9; do [ -f research/models/vr_$i.meta ] && echo "$(cut -d' ' -f1 research/models/vr_$i.meta) $i"; done | sort -g | head -1 | cut -d' ' -f2)
cp research/models/vr_$BEST.txt research/models/vr_10.txt; cp research/models/vr_$BEST.meta research/models/vr_10.meta
echo "VR SELECT: vr_$BEST -> vr_10 ($(cat research/models/vr_10.meta))"
