#!/usr/bin/env bash
# Runs the prior-guided-rollout experiment at the simulation count that equalises wall-clock with the
# best, using the ratio measured by calibrate.sh (research/runs/calib_rollprior.txt).
set -euo pipefail
cd "$(dirname "$0")/../.."
RATIO=$(sed -n 's/.*candidate\/best = \([0-9.]*\).*/\1/p' research/runs/calib_rollprior.txt)
[ -n "$RATIO" ] || { echo "no calibration ratio in research/runs/calib_rollprior.txt"; exit 1; }
BASE_SIMS=$(sed -n 's/^sims=//p' research/best_params.txt); BASE_SIMS=${BASE_SIMS:-500}
SIMS=$(python3 -c "import sys; print(max(50, int(round($BASE_SIMS / $RATIO / 10.0) * 10)))")
echo "roll_prior cost ratio $RATIO -> sims $SIMS (best uses $BASE_SIMS)"
research/exp.sh rollprior_cal "prior-guided rollouts (bilinear fast policy, eps 0.25) at equal wall-clock: sims $SIMS (cost ratio $RATIO)" roll_prior=1 sims=$SIMS
