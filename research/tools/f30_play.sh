#!/usr/bin/env bash
# Phase F30 (pre-registered in program.md F30): play test of the decision-trained V (vr_251) at A10 on one fresh, never-used
# 4,000-seed slice, paired against the A10 best (f23a10). Rule: adopt f30a10 as the A20 best iff dscore > 0 and P(improve) >= 0.9
# on this slice; the slice is fresh and twice the F25 confirmation size, so it counts as keep and confirmation together.
set -euo pipefail
cd "$(dirname "$0")/../.."
OLD=f23a10; NEW=f30a10; N=4000
NF=research/eval/fresh_next.txt; S=$(cat $NF); echo $((S + N)) > $NF; SPLIT=fs${S}n${N}
pa() { sed 's/^/--param /' "research/runs/$1.params" | tr '\n' ' '; }
echo "F30 a10: $NEW vs $OLD on $SPLIT"
python3 -u research/eval/run.py "$OLD" "$SPLIT" $(pa "$OLD") --asc 10 --winmode act3 | sed -n 1p
OUT=$(python3 -u research/eval/run.py "$NEW" "$SPLIT" $(pa "$NEW") --base "$OLD" --asc 10 --winmode act3); echo "$OUT"
D=$(echo "$OUT" | sed -n 's/.*dscore=\([-+0-9.]*\).*/\1/p'); PI=$(echo "$OUT" | sed -n 's/.*P(improve)=\([0-9.]*\).*/\1/p')
STATUS=discard
if python3 -c "import sys; sys.exit(0 if float('$D')>0 and float('$PI')>=0.9 else 1)"; then
  STATUS=keep; TRACK=a10 research/promote.sh "$NEW"; echo "F30 KEEP -> new A10 best $NEW"
else echo "F30 DISCARD (dscore=$D P=$PI)"; fi
python3 research/eval/log.py "$NEW" "$SPLIT" "$STATUS" "[a10] Phase F30: A10 best + decision-trained V vr_251 (4,000-seed play test) [vr_model=251]" --base "$OLD" --asc 10 --winmode act3 > /dev/null
