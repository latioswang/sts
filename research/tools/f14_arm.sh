#!/usr/bin/env bash
# Phase F14 arms (pre-registered in 20b97b9): only if the selected Heart V (vr_212) is a new model (not vr_171 / vr_202);
# leak test with the Heart best + vr_model=212 first (exit 1 on FAIL), then f14heart and f14heart_b on the fresh-slice rule.
set -euo pipefail
cd "$(dirname "$0")/../.."
if cmp -s research/models/vr_212.txt research/models/vr_171.txt || cmp -s research/models/vr_212.txt research/models/vr_202.txt; then
  echo "F14: selection kept an old model -> no arm"; exit 0; fi
R=$(/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --asc 0 \
    $(sed 's/^/--param /' research/best_heart_params.txt | tr '\n' ' ') --param keys=1 --param chest_pol=1 --param vr_model=212 | tail -1)
echo "leaktest_heart [vr_model=212]: $R"; case "$R" in *PASS*) ;; *) exit 1;; esac
TRACK=heart research/tools/exp_fresh.sh f14heart "Phase F14: Heart best + keys-aware Heart V (vr_model=212)" vr_model=212
TRACK=heart research/tools/exp_fresh.sh f14heart_b "Phase F14: Heart best + keys-aware Heart V as a bonus (vr_model=212 vr_pure=0 vr_pure_relic=0)" vr_model=212 vr_pure=0 vr_pure_relic=0
