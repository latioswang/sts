#!/usr/bin/env bash
# Pooled-rule experiment for evaluation-limited tracks (A20, Heart): runs val (300), dev2 (100) and dev3
# (100) with best params + overrides, decides on the pooled 500 seeds (dscore > 0 and P >= 0.9), promotes
# on keep. The base must already have val/dev2/dev3 runs (see tools/base_slices.sh).
# usage: TRACK=heart|a20 exp_pooled.sh NAME "description" [k=v ...]
set -euo pipefail
cd "$(dirname "$0")/../.."
NAME=$1; DESC=$2; shift 2
TRACK=${TRACK:?TRACK required (heart|a20|a10|a0)}
case "$TRACK" in a0) ASC=0;; heart) ASC=0;; a10) ASC=10;; a20) ASC=20;; *) echo "unknown TRACK $TRACK" >&2; exit 1;; esac
WINMODE=act3; [ "$TRACK" = heart ] && WINMODE=heart
if [ "$TRACK" = a0 ]; then BESTF=research/best_name.txt; PARF=research/best_params.txt; else BESTF=research/best_${TRACK}_name.txt; PARF=research/best_${TRACK}_params.txt; fi
BEST=$(cat "$BESTF")
declare -A P; while IFS='=' read -r k v; do [ -n "$k" ] && P[$k]=$v; done < "$PARF"
if [ "$TRACK" = heart ]; then P[keys]=1; P[chest_pol]=1; fi
for kv in "$@"; do P[${kv%%=*}]=${kv#*=}; done
ARGS=(); for k in "${!P[@]}"; do ARGS+=(--param "$k=${P[$k]}"); done
for SPLIT in val dev2 dev3; do
  [ -f "research/runs/$BEST.$SPLIT.a$ASC.tsv" ] || { echo "base $BEST has no $SPLIT run (run tools/base_slices.sh)" >&2; exit 1; }
  python3 -u research/eval/run.py "$NAME" "$SPLIT" "${ARGS[@]}" --base "$BEST" --asc "$ASC" --winmode "$WINMODE" | tail -2
done
for k in "${!P[@]}"; do echo "$k=${P[$k]}"; done | sort > "research/runs/$NAME.params"
OUT=$(python3 research/tools/pooled.py "$NAME" "$BEST" "$ASC" val,dev2,dev3 | tee "research/runs/pooled_$NAME.txt")
echo "$OUT"
STATUS=discard
if echo "$OUT" | tail -1 | grep -q "^KEEP$"; then STATUS=keep; TRACK=$TRACK research/promote.sh "$NAME"; echo "KEEP (pooled 500) -> new best $NAME ($TRACK)"; fi
python3 research/eval/log.py "$NAME" val "$STATUS" "[$TRACK pooled500] $DESC [$*] $(echo "$OUT" | head -1 | sed 's/.*dscore=/pooled dscore=/')" --base "$BEST" --asc "$ASC" --winmode "$WINMODE" > /dev/null
for SPLIT in dev2 dev3; do
  python3 research/eval/log.py "$NAME" "$SPLIT" info "[$TRACK pooled500 slice] $DESC" --base "$BEST" --asc "$ASC" --winmode "$WINMODE" > /dev/null
done
