#!/usr/bin/env bash
# Phase F20 arm (pre-registered in program.md F20): only if vr_242 (the correlation pick) is not vr_171; leak test at A20,
# then f20a20 under the fresh-slice rule.
set -euo pipefail
cd "$(dirname "$0")/../.."
if cmp -s research/models/vr_242.txt research/models/vr_171.txt; then echo "F20: selection kept vr_171 -> no arm"; exit 0; fi
R=$(/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --asc 20 \
    $(sed 's/^/--param /' research/best_a20_params.txt | tr '\n' ' ') --param vr_model=242 | tail -1)
echo "leaktest_a20 [vr_model=242]: $R"; case "$R" in *PASS*) ;; *) exit 1;; esac
TRACK=a20 research/tools/exp_fresh.sh f20a20 "Phase F20: A20 best + A20 expert-iteration V (vr_model=242)" vr_model=242
