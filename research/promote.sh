#!/usr/bin/env bash
# usage: promote.sh NAME  -> makes NAME's params the new best
set -euo pipefail
cd "$(dirname "$0")/.."
TRACK=${TRACK:-a0}
if [ "$TRACK" = "heart" ]; then cp "research/runs/$1.params" research/best_heart_params.txt; echo "$1" > research/best_heart_name.txt
else cp "research/runs/$1.params" research/best_params.txt; echo "$1" > research/best_name.txt; fi
