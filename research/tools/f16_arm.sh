#!/usr/bin/env bash
# Phase F16 arm (pre-registered in 9fa659a): only if vr_222 (the correlation pick) is not vr_210; leak test, then f16heart.
set -euo pipefail
cd "$(dirname "$0")/../.."
if cmp -s research/models/vr_222.txt research/models/vr_210.txt; then echo "F16: selection kept vr_210 -> no arm"; exit 0; fi
R=$(/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --asc 0 \
    $(sed 's/^/--param /' research/best_heart_params.txt | tr '\n' ' ') --param keys=1 --param chest_pol=1 --param vr_model=222 | tail -1)
echo "leaktest_heart [vr_model=222]: $R"; case "$R" in *PASS*) ;; *) exit 1;; esac
TRACK=heart research/tools/exp_fresh.sh f16heart "Phase F16: Heart best + expert-iteration keys-aware V (vr_model=222)" vr_model=222
