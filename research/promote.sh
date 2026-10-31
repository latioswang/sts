#!/usr/bin/env bash
# usage: promote.sh NAME  -> makes NAME's params the new best
set -euo pipefail
cd "$(dirname "$0")/.."
cp "research/runs/$1.params" research/best_params.txt
echo "$1" > research/best_name.txt
