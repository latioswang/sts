#!/usr/bin/env bash
# usage: exp.sh NAME "description" [k=v ...]   -> runs val split with best params + overrides,
# compares paired vs current best, logs to results.tsv. Promotion is done by promote.sh.
set -euo pipefail
cd "$(dirname "$0")/.."
NAME=$1; DESC=$2; shift 2
SPLIT=${SPLIT:-val}
BEST=$(cat research/best_name.txt)
declare -A P
while IFS='=' read -r k v; do [ -n "$k" ] && P[$k]=$v; done < research/best_params.txt
for kv in "$@"; do P[${kv%%=*}]=${kv#*=}; done
ARGS=(); for k in "${!P[@]}"; do ARGS+=(--param "$k=${P[$k]}"); done
if [ ! -f "research/runs/$BEST.$SPLIT.a0.tsv" ]; then echo "best $BEST has no $SPLIT run" >&2; exit 1; fi
python3 -u research/eval/run.py "$NAME" "$SPLIT" "${ARGS[@]}" --base "$BEST"
for k in "${!P[@]}"; do echo "$k=${P[$k]}"; done | sort > "research/runs/$NAME.params"
python3 research/eval/log.py "$NAME" "$SPLIT" pending "$DESC [$*]" --base "$BEST" > /dev/null
