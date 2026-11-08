#!/usr/bin/env bash
# Phase F24 (measurement only, pre-registered in program.md): confirm a keep — NEW vs the best it replaced (OLD) on a fresh,
# never-used slice of N seeds (both arms re-run on it, paired). No promotion; logged with status "measure".
# usage: f13_measure.sh TRACK OLD NEW N
set -euo pipefail
cd "$(dirname "$0")/../.."
TRACK=$1; OLD=$2; NEW=$3; N=$4
case "$TRACK" in a0|heart) ASC=0;; a10) ASC=10;; a20) ASC=20;; esac
WM=act3; [ "$TRACK" = heart ] && WM=heart
NF=research/eval/fresh_next.txt; S=$(cat $NF); echo $((S + N)) > $NF; SPLIT=fs${S}n${N}
pa() { sed 's/^/--param /' "research/runs/$1.params" | tr '\n' ' '; }
echo "F24 $TRACK: $NEW vs $OLD on $SPLIT"
python3 -u research/eval/run.py "$OLD" "$SPLIT" $(pa "$OLD") --asc $ASC --winmode $WM | sed -n 1p
python3 -u research/eval/run.py "$NEW" "$SPLIT" $(pa "$NEW") --base "$OLD" --asc $ASC --winmode $WM
python3 research/eval/log.py "$NEW" "$SPLIT" measure "Phase F24: confirm $NEW vs $OLD [$TRACK]" --base "$OLD" --asc $ASC --winmode $WM > /dev/null
