#!/usr/bin/env bash
# Phase F17 arm (pre-registered in program.md F17): only if vr_232 (the correlation pick) is not vr_222; leak test, then f17heart.
set -euo pipefail
cd "$(dirname "$0")/../.."
if cmp -s research/models/vr_232.txt research/models/vr_222.txt; then echo "F17: selection kept vr_222 -> no arm"; exit 0; fi
R=$(/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --asc 0 \
    $(sed 's/^/--param /' research/best_heart_params.txt | tr '\n' ' ') --param keys=1 --param chest_pol=1 --param vr_model=232 | tail -1)
echo "leaktest_heart [vr_model=232]: $R"; case "$R" in *PASS*) ;; *) exit 1;; esac
TRACK=heart research/tools/exp_fresh.sh f17heart "Phase F17: Heart best + expert-iteration keys-aware V (vr_model=232)" vr_model=232
