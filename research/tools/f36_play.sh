#!/usr/bin/env bash
# Phase F36 (b) (pre-registered in program.md F36): 4,000-seed play test of the learned path-choice model on one fresh, never-used
# slice, paired against the track's best. usage: f36_play.sh TRACK ID   (TRACK = a20; ID = the armed pm_<ID>; base = f35a20 with its map_model line replaced)
# Rule: adopt iff dscore > 0 and P(improve) >= 0.9 (counts as keep; a gain is reported only after the F25 fresh 2,000-seed
# confirmation vs the track's last confirmed best). A near miss (0.7 <= P < 0.9) gets one same-size extension (ext_fresh.sh).
set -euo pipefail
cd "$(dirname "$0")/../.."
TRACK=$1; ID=$2; N=4000
case $TRACK in a20) OLD=f35a20; ASC=20 ;; *) echo "bad track"; exit 1 ;; esac
NEW=f36${TRACK}_pm${ID}
{ grep -v "^map_model=" "research/runs/$OLD.params"; echo "map_model=$ID"; } > "research/runs/$NEW.params"
NF=research/eval/fresh_next.txt; S=$(cat $NF); echo $((S + N)) > $NF; SPLIT=fs${S}n${N}
pa() { sed 's/^/--param /' "research/runs/$1.params" | tr '\n' ' '; }
echo "F36 $TRACK: $NEW (map_model=$ID) vs $OLD on $SPLIT"
python3 -u research/eval/run.py "$OLD" "$SPLIT" $(pa "$OLD") --asc $ASC --winmode act3 | sed -n 1p
OUT=$(python3 -u research/eval/run.py "$NEW" "$SPLIT" $(pa "$NEW") --base "$OLD" --asc $ASC --winmode act3); echo "$OUT"
D=$(echo "$OUT" | sed -n 's/.*dscore=\([-+0-9.]*\).*/\1/p'); PI=$(echo "$OUT" | sed -n 's/.*P(improve)=\([0-9.]*\).*/\1/p')
STATUS=discard
if python3 -c "import sys; sys.exit(0 if float('$D')>0 and float('$PI')>=0.9 else 1)"; then
  STATUS=keep; TRACK=$TRACK research/promote.sh "$NEW"; echo "F36 KEEP -> new $TRACK best $NEW"
else echo "F36 DISCARD (dscore=$D P=$PI)"; fi
python3 research/eval/log.py "$NEW" "$SPLIT" "$STATUS" "[$TRACK] Phase F36: f35a20 with retrained path-choice model pm_$ID (4,000-seed play test) [map_model=$ID]" --base "$OLD" --asc $ASC --winmode act3 > /dev/null
