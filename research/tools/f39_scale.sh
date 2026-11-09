#!/usr/bin/env bash
# Phase F39 (pre-registered in program.md F39): compute-scaling DIAGNOSTIC — never adopted (equal-compute rule; the sims budget is
# the owner's decision). One fresh 1,000-seed slice per track; the track's current best at its own sims (500) and at sims x2 / x4,
# paired on the same seeds. usage: f39_scale.sh TRACK   (TRACK = a10 | a20 | heart)
set -euo pipefail
cd "$(dirname "$0")/../.."
TRACK=$1; N=1000
case $TRACK in heart) OLD=$(cat research/best_heart_name.txt); ASC=0; WM=heart ;;
  a10) OLD=$(cat research/best_a10_name.txt); ASC=10; WM=act3 ;; a20) OLD=$(cat research/best_a20_name.txt); ASC=20; WM=act3 ;; *) echo "bad track"; exit 1 ;; esac
grep -q '^sims=' "research/runs/$OLD.params" && { echo "base sets sims explicitly; F39 assumes the default 500"; exit 1; }
NF=research/eval/fresh_next.txt; S=$(cat $NF); echo $((S + N)) > $NF; SPLIT=fs${S}n${N}
pa() { sed 's/^/--param /' "research/runs/$1.params" | tr '\n' ' '; }
echo "F39 $TRACK: $OLD at sims 500 / 1000 / 2000 on $SPLIT"
python3 -u research/eval/run.py "$OLD" "$SPLIT" $(pa "$OLD") --asc $ASC --winmode $WM | sed -n 1p
for M in 2 4; do
  NEW=f39${TRACK}_s${M}x; { cat "research/runs/$OLD.params"; echo "sims=$((500 * M))"; } > "research/runs/$NEW.params"
  python3 -u research/eval/run.py "$NEW" "$SPLIT" $(pa "$NEW") --base "$OLD" --asc $ASC --winmode $WM
  python3 research/eval/log.py "$NEW" "$SPLIT" scaling "[$TRACK] Phase F39: compute-scaling diagnostic, track best at sims $((500 * M)) (${M}x search; never adopted)" --base "$OLD" --asc $ASC --winmode $WM > /dev/null
done
