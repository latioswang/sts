#!/usr/bin/env bash
# Replication run of a kept experiment on the untouched dev slice (100000+), paired against the
# previous best's dev run (BASE_dev), logged as an info row. usage: TRACK=heart rep_dev.sh NAME BASE
set -euo pipefail
cd "$(dirname "$0")/../.."
NAME=$1; BASE=$2; TRACK=${TRACK:-a0}
case "$TRACK" in a0|heart) ASC=0;; a10) ASC=10;; a20) ASC=20;; *) echo "unknown TRACK $TRACK" >&2; exit 1;; esac
WINMODE=act3; [ "$TRACK" = heart ] && WINMODE=heart
ARGS=$(sed 's/^/--param /' "research/runs/$NAME.params" | tr '\n' ' ')
[ -f "research/runs/${BASE}_dev.dev.a$ASC.tsv" ] || { echo "no dev run for base $BASE" >&2; exit 1; }
python3 -u research/eval/run.py "${NAME}_dev" dev $ARGS --base "${BASE}_dev" --asc "$ASC" --winmode "$WINMODE" | tail -2
cp "research/runs/$NAME.params" "research/runs/${NAME}_dev.params"
python3 research/eval/log.py "${NAME}_dev" dev info "VERIFY: $NAME on dev vs ${BASE}_dev (replication of a $TRACK keep)" --base "${BASE}_dev" --asc "$ASC" --winmode "$WINMODE" > /dev/null
