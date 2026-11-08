#!/usr/bin/env bash
# Phase F12 arm (pre-registered in program.md, F12): only if the selected Heart V (vr_205) is a new model (not a copy of vr_171);
# leak test with the Heart best + vr_model=205 first (exit 1 on FAIL), then the fresh-slice arm.
set -euo pipefail
cd "$(dirname "$0")/../.."
if cmp -s research/models/vr_205.txt research/models/vr_171.txt || cmp -s research/models/vr_205.txt research/models/vr_202.txt; then echo "F11: selection kept vr_171 -> no arm (phase closes)"; exit 0; fi
R=$(/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --asc 0 \
    $(sed 's/^/--param /' research/best_heart_params.txt | tr '\n' ' ') --param keys=1 --param chest_pol=1 --param vr_model=205 | tail -1)
echo "leaktest_heart [vr_model=205]: $R"; case "$R" in *PASS*) ;; *) exit 1;; esac
TRACK=heart research/tools/exp_fresh.sh f12heart "Phase F12: Heart best + Heart-track V v2 (vr_model=205)" vr_model=205
