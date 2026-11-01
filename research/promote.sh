#!/usr/bin/env bash
# usage: promote.sh NAME  -> makes NAME's params the new best
set -euo pipefail
cd "$(dirname "$0")/.."
TRACK=${TRACK:-a0}
if [ "$TRACK" = "a0" ]; then cp "research/runs/$1.params" research/best_params.txt; echo "$1" > research/best_name.txt
else cp "research/runs/$1.params" "research/best_${TRACK}_params.txt"; echo "$1" > "research/best_${TRACK}_name.txt"; fi
