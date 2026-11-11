#!/usr/bin/env bash
# F52: play test of a pure learned leaf (leaf_sv=ID, rollout_turns=0) vs the A0 best at equal wall-clock on N fresh seeds.
# Measurement only (status scaling); never promotes. usage: f52_eval.sh ID NAME [N=300]
set -euo pipefail
cd "$(dirname "$0")/../.."
ID=$1; NAME=$2; N=${3:-300}
BIN=/home/user/sts_lightspeed/build/fair_eval; BEST=$(cat research/best_name.txt)
ARGS=$(sed 's/^/--param /' research/best_params.txt | tr '\n' ' ')
"$BIN" --leaktest --seed_start 100000 --count 2 $ARGS --param leaf_sv=$ID --param rollout_turns=0 | tail -1
C=$(research/tools/calibrate.sh "leaf_sv=$ID rollout_turns=0" 8 100010 | tail -1); echo "calib $NAME: $C"
F=$(echo "$C" | sed -n 's/.*scale candidate sims by \([0-9.]*\).*/\1/p')
S1=$(python3 -c "print(max(50, int(1000*float('$F'))//25*25))"); echo "sims: $S1"
NF=research/eval/fresh_next.txt; S=$(cat $NF); echo $((S + N)) > $NF; SPLIT=fs${S}n${N}
pa() { sed 's/^/--param /' "research/runs/$1.params" | tr '\n' ' '; }
{ grep -v -E '^(leaf_sv|rollout_turns|sims)=' "research/runs/$BEST.params"; echo leaf_sv=$ID; echo rollout_turns=0; echo sims=$S1; } > research/runs/$NAME.params
python3 -u research/eval/run.py "$BEST" "$SPLIT" $(pa "$BEST") --asc 0 | sed -n 1p
python3 -u research/eval/run.py $NAME "$SPLIT" $(pa $NAME) --base "$BEST" --asc 0 | tail -2
python3 research/eval/log.py $NAME "$SPLIT" scaling "[a0] F52: pure learned leaf sv_$ID (rollout_turns=0) at equal wall-clock (sims $S1) vs $BEST" --base "$BEST" --asc 0 > /dev/null
