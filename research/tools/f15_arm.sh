#!/usr/bin/env bash
# Phase F15 (pre-registered in 093b9de): leak test with the Heart best + vr_model=210, then two fresh-slice arms.
set -euo pipefail
cd "$(dirname "$0")/../.."
R=$(/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --asc 0 \
    $(sed 's/^/--param /' research/best_heart_params.txt | tr '\n' ' ') --param keys=1 --param chest_pol=1 --param vr_model=210 | tail -1)
echo "leaktest_heart [vr_model=210]: $R"; case "$R" in *PASS*) ;; *) exit 1;; esac
