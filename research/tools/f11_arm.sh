#!/usr/bin/env bash
# Phase F11 arm (pre-registered in 2d35a27): only if the selected Heart V (vr_202) is a new model (not a copy of vr_171);
# leak test with the Heart best + vr_model=202 first (exit 1 on FAIL), then the fresh-slice arm.
set -euo pipefail
cd "$(dirname "$0")/../.."
if cmp -s research/models/vr_202.txt research/models/vr_171.txt; then echo "F11: selection kept vr_171 -> no arm (phase closes)"; exit 0; fi
R=$(/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --asc 0 \
    $(sed 's/^/--param /' research/best_heart_params.txt | tr '\n' ' ') --param keys=1 --param chest_pol=1 --param vr_model=202 | tail -1)
echo "leaktest_heart [vr_model=202]: $R"; case "$R" in *PASS*) ;; *) exit 1;; esac
TRACK=heart research/tools/exp_fresh.sh f11heart "Phase F11: Heart best + Heart-track V (vr_model=202)" vr_model=202
