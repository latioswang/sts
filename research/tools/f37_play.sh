#!/usr/bin/env bash
# Phase F37 (pre-registered in program.md F37): 4,000-seed play test of a path-choice model on a track's current best, paired
# against that best on one fresh slice. usage: f37_play.sh TRACK ID   (TRACK = a0 | heart | a10 | a20; ID = pm_<ID>)
# Rule: adopt iff dscore > 0 and P(improve) >= 0.9; a gain is reported only after the F25 fresh 2,000-seed confirmation.
set -euo pipefail
cd "$(dirname "$0")/../.."
TRACK=$1; ID=$2; N=4000
case $TRACK in a0) OLD=$(cat research/best_name.txt); ASC=0; WM=act3 ;; heart) OLD=$(cat research/best_heart_name.txt); ASC=0; WM=heart ;;
  a10) OLD=$(cat research/best_a10_name.txt); ASC=10; WM=act3 ;; a20) OLD=$(cat research/best_a20_name.txt); ASC=20; WM=act3 ;; *) echo "bad track"; exit 1 ;; esac
NEW=f37${TRACK}_pm${ID}
{ grep -v '^map_model=' "research/runs/$OLD.params"; echo "map_model=$ID"; } > "research/runs/$NEW.params"
NF=research/eval/fresh_next.txt; S=$(cat $NF); echo $((S + N)) > $NF; SPLIT=fs${S}n${N}
pa() { sed 's/^/--param /' "research/runs/$1.params" | tr '\n' ' '; }
echo "F37 $TRACK: $NEW (map_model=$ID) vs $OLD on $SPLIT"
python3 -u research/eval/run.py "$OLD" "$SPLIT" $(pa "$OLD") --asc $ASC --winmode $WM | sed -n 1p
OUT=$(python3 -u research/eval/run.py "$NEW" "$SPLIT" $(pa "$NEW") --base "$OLD" --asc $ASC --winmode $WM); echo "$OUT"
D=$(echo "$OUT" | sed -n 's/.*dscore=\([-+0-9.]*\).*/\1/p'); PI=$(echo "$OUT" | sed -n 's/.*P(improve)=\([0-9.]*\).*/\1/p')
STATUS=discard
if python3 -c "import sys; sys.exit(0 if float('$D')>0 and float('$PI')>=0.9 else 1)"; then
  STATUS=keep; TRACK=$TRACK research/promote.sh "$NEW"; echo "F37 KEEP -> new $TRACK best $NEW"
else echo "F37 DISCARD (dscore=$D P=$PI)"; fi
python3 research/eval/log.py "$NEW" "$SPLIT" "$STATUS" "[$TRACK] Phase F37: track best + path-choice model pm_$ID (4,000-seed play test) [map_model=$ID]" --base "$OLD" --asc $ASC --winmode $WM > /dev/null
