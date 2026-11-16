#!/usr/bin/env bash
# SOTA stage 0, E2 (PLAN.md rev 2 §3; MEASUREMENT ONLY): the A20 best replays SPLIT; at every boss arrival K fair re-fights
# and K2 clairvoyant re-fights (lightspeed searcher, 40,000 sims, bosses x3) with every RNG stream re-seeded. The main line
# is bit-identical to the plain best (checked 2026-11-16 on 6 seeds). Rows: research/s0/e2/refight_<agentSeed>.tsv
# cols: agentSeed floor enc hp maxhp deck potions relics nFair winsFair hpFracAfterWinFair nClair winsClair hpFracClair
# usage: s0_e2.sh SPLIT K K2
set -uo pipefail
cd "$(dirname "$0")/../.."
SPLIT=$1; K=$2; K2=$3
export FAIR_EVAL_BIN=${FAIR_EVAL_BIN:-/home/user/sts_lightspeed/build_s0/fair_eval} FAIR_ALARM=${FAIR_ALARM:-14400}
export FAIR_REFIGHT_DIR=research/s0/e2; mkdir -p $FAIR_REFIGHT_DIR
pa() { sed 's/^/--param /' "research/runs/$1.params" | tr '\n' ' '; }
echo "== $(date -u +%H:%M) E2 on $SPLIT K=$K K2=$K2"
python3 -u research/eval/run.py s0_a20_e2 "$SPLIT" $(pa f43a20_sf2) --param diag_refight=$K --param diag_refight_cheat=$K2 --asc 20 --winmode act3 | sed -n 1,3p
echo "== $(date -u +%H:%M) done; arrivals: $(cat $FAIR_REFIGHT_DIR/*.tsv | wc -l)"
