#!/usr/bin/env bash
# Phase F18 (b) arm (pre-registered in program.md F18): Heart best + vr_key=1 (V decides the ruby-key recall); leak test, then
# the fresh-slice rule vs the Heart best.
set -euo pipefail
cd "$(dirname "$0")/../.."
R=$(/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --asc 0 \
    $(sed 's/^/--param /' research/best_heart_params.txt | tr '\n' ' ') --param keys=1 --param chest_pol=1 --param vr_key=1 | tail -1)
echo "leaktest_heart [vr_key=1]: $R"; case "$R" in *PASS*) ;; *) exit 1;; esac
TRACK=heart research/tools/exp_fresh.sh f18heart_vkey "Phase F18: Heart best + V-chosen ruby-key recall (vr_key=1)" vr_key=1
