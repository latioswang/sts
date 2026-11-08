#!/usr/bin/env bash
# Phase F21 arm (pre-registered in program.md F21; post-hoc hypothesis from F20, disclosed): A20 best + vr_model=242 with its
# three V weights scaled by 1.5 (72 -> 108; the ratio of vr_171's to vr_242's prediction SD on the F20 A20 holdout, 0.275 / 0.183);
# leak test at A20, fresh-slice rule vs the A20 best.
set -euo pipefail
cd "$(dirname "$0")/../.."
P="vr_model=242 vr_bonus=108 vr_relic_w=108 vr_upg_w=108"
R=$(/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --asc 20 \
    $(sed 's/^/--param /' research/best_a20_params.txt | tr '\n' ' ') $(for x in $P; do echo "--param $x"; done) | tail -1)
echo "leaktest_a20 [$P]: $R"; case "$R" in *PASS*) ;; *) exit 1;; esac
TRACK=a20 research/tools/exp_fresh.sh f21a20 "Phase F21: A20 best + vr_model=242 at 1.5x V weights (scale-matched to vr_171)" $P
