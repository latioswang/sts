#!/usr/bin/env bash
# Phase S evaluation of model ID at equal wall-clock: calibrate the per-game cost of (leaf_sv=ID, no rollouts)
# against the A0 best on the idle machine, convert to simulations, run val 300 paired vs the A0 best, log an
# informational "scaling" row. Never promotes. usage: sv_eval.sh ID "label"
set -euo pipefail
cd "$(dirname "$0")/../.."
ID=${1:?}; LABEL=${2:-}
BEST=$(cat research/best_name.txt)
[ -s "research/models/sv_$ID.txt" ] || { echo "no model sv_$ID"; exit 1; }
CAL=$(research/tools/calibrate.sh "leaf_sv=$ID rollout_turns=0" 8 100010 | tail -1)
echo "$CAL"
F=$(echo "$CAL" | sed -n 's/.*scale candidate sims by \([0-9.]*\).*/\1/p')
SIMS=$(python3 -c "print(max(50, int(round(500 * float('$F')))))")
ARGS=$(sed 's/^/--param /' research/best_params.txt | tr '\n' ' ')
python3 -u research/eval/run.py "sv$ID" val $ARGS --param leaf_sv=$ID --param rollout_turns=0 --param sims=$SIMS --base "$BEST" --asc 0 | tail -2
{ sed 's/^/ /' research/best_params.txt | sed 's/^ //'; echo "leaf_sv=$ID"; echo "rollout_turns=0"; echo "sims=$SIMS"; } | sort > "research/runs/sv$ID.params"
python3 research/eval/log.py "sv$ID" val scaling "PHASE S $LABEL: leaf value model sv_$ID, no rollouts, equal wall-clock (cost ratio $(echo "$CAL" | sed -n 's/.*candidate\/best = \([0-9.]*\).*/\1/p'), sims $SIMS) vs $BEST" --base "$BEST" --asc 0 > /dev/null
