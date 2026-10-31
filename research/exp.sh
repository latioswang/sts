#!/usr/bin/env bash
# usage: exp.sh NAME "description" [k=v ...]
# Runs val split with best params + overrides, paired comparison vs current best,
# applies the keep rule (dscore > 0 and P(improve) >= 0.9) -> auto-promote, logs to results.tsv.
set -euo pipefail
cd "$(dirname "$0")/.."
NAME=$1; DESC=$2; shift 2
SPLIT=${SPLIT:-val}
BEST=$(cat research/best_name.txt)
declare -A P
while IFS='=' read -r k v; do [ -n "$k" ] && P[$k]=$v; done < research/best_params.txt
for kv in "$@"; do P[${kv%%=*}]=${kv#*=}; done
ARGS=(); for k in "${!P[@]}"; do ARGS+=(--param "$k=${P[$k]}"); done
OUT=$(python3 -u research/eval/run.py "$NAME" "$SPLIT" "${ARGS[@]}" --base "$BEST")
echo "$OUT"
for k in "${!P[@]}"; do echo "$k=${P[$k]}"; done | sort > "research/runs/$NAME.params"
D=$(echo "$OUT" | sed -n 's/.*dscore=\([-+0-9.]*\).*/\1/p')
PI=$(echo "$OUT" | sed -n 's/.*P(improve)=\([0-9.]*\).*/\1/p')
STATUS=discard
if python3 -c "import sys; sys.exit(0 if float('$D')>0 and float('$PI')>=0.9 else 1)"; then
  STATUS=keep; research/promote.sh "$NAME"; echo "KEEP -> new best $NAME"
fi
python3 research/eval/log.py "$NAME" "$SPLIT" "$STATUS" "$DESC [$*]" --base "$BEST" > /dev/null
