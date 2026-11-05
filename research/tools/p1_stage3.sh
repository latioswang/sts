#!/usr/bin/env bash
# P1 stage 3 (pre-registered 2026-11-05 22:10 UTC, see program.md): on the fresh slice fs1207500n1000 (fidelity sim), the A0 best
# params with prior v1 (main fork build) vs prior v4 h128 (side build v4) at 500 sims (keep decision for the compiled prior on
# the fork: dscore > 0 and P >= 0.9), plus v4 at 100 and 200 sims vs the same 500-sim v1 reference (report only: the sims curve).
set -uo pipefail
cd "$(dirname "$0")/../.."
SL=fs1207500n1000; MAIN=/home/user/sts_lightspeed_fid/build/fair_eval; V4=/home/user/sts_lightspeed_fid/build_v4/fair_eval
A=$(sed 's/^/--param /' research/best_params.txt | tr '\n' ' ')
FAIR_EVAL_BIN=$MAIN python3 research/eval/run.py p3_ref500 $SL $A | sed -n 1p
FAIR_EVAL_BIN=$V4 python3 research/eval/run.py p3_v4s500 $SL $A --base p3_ref500 | grep -E "^p3_|^vs "
FAIR_EVAL_BIN=$V4 python3 research/eval/run.py p3_v4s200 $SL $A --param sims=200 --base p3_ref500 | grep -E "^p3_|^vs "
FAIR_EVAL_BIN=$V4 python3 research/eval/run.py p3_v4s100 $SL $A --param sims=100 --base p3_ref500 | grep -E "^p3_|^vs "
echo "P1 STAGE 3 DONE"
