#!/usr/bin/env bash
# Seed a new ascension track (a10 | a20) from the current A0 best: run its params as the track
# baseline on val, log it, and write best_<track>_{name,params}.txt.  usage: new_track.sh a10
set -euo pipefail
cd "$(dirname "$0")/../.."
TRACK=${1:?a10|a20}
case "$TRACK" in a10) ASC=10;; a20) ASC=20;; *) echo "unsupported track $TRACK" >&2; exit 1;; esac
NAME="base_$TRACK"
ARGS=$(sed 's/^/--param /' research/best_params.txt | tr '\n' ' ')
python3 -u research/eval/run.py "$NAME" val $ARGS --asc "$ASC"
cp research/best_params.txt "research/runs/$NAME.params"
cp research/best_params.txt "research/best_${TRACK}_params.txt"
echo "$NAME" > "research/best_${TRACK}_name.txt"
python3 research/eval/log.py "$NAME" val best "[$TRACK] track baseline = current A0 best params at ascension $ASC" --asc "$ASC"
