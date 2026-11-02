#!/usr/bin/env bash
# Produce the dev2 and dev3 runs of a track's current best (needed by exp_pooled.sh).
# usage: TRACK=heart|a20 base_slices.sh
set -euo pipefail
cd "$(dirname "$0")/../.."
TRACK=${TRACK:?}; case "$TRACK" in a0) ASC=0;; heart) ASC=0;; a10) ASC=10;; a20) ASC=20;; esac
WINMODE=act3; [ "$TRACK" = heart ] && WINMODE=heart
if [ "$TRACK" = a0 ]; then BEST=$(cat research/best_name.txt); else BEST=$(cat research/best_${TRACK}_name.txt); fi
ARGS=$(sed 's/^/--param /' "research/runs/$BEST.params" | tr '\n' ' ')
for SPLIT in dev2 dev3; do
  [ -f "research/runs/$BEST.$SPLIT.a$ASC.tsv" ] && { echo "$BEST $SPLIT exists"; continue; }
  if [ -f "research/runs/${BEST}_$SPLIT.$SPLIT.a$ASC.tsv" ]; then  # replication run already made under the queue's _split spelling
    cp "research/runs/${BEST}_$SPLIT.$SPLIT.a$ASC.tsv" "research/runs/$BEST.$SPLIT.a$ASC.tsv"; echo "$BEST $SPLIT copied from ${BEST}_$SPLIT"; continue
  fi
  python3 -u research/eval/run.py "$BEST" "$SPLIT" $ARGS --asc "$ASC" --winmode "$WINMODE" | tail -1
  python3 research/eval/log.py "$BEST" "$SPLIT" info "base slice for pooled decisions ($TRACK)" --asc "$ASC" --winmode "$WINMODE" > /dev/null
done
