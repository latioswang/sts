#!/usr/bin/env bash
# usage: exp.sh NAME "description" [k=v ...]
# Runs val split with best params + overrides, paired comparison vs current best,
# applies the keep rule (dscore > 0 and P(improve) >= 0.9) -> auto-promote, logs to results.tsv.
set -euo pipefail
cd "$(dirname "$0")/.."
NAME=$1; DESC=$2; shift 2
SPLIT=${SPLIT:-val}
# TRACK=a0 (default): win = beat the Act 3 boss, best in best_{name,params}.txt
# TRACK=heart       : keys=1 forced, win = beat the Heart, best in best_heart_{name,params}.txt
TRACK=${TRACK:-a0}
if [ "$TRACK" = "heart" ]; then BESTF=research/best_heart_name.txt; PARF=research/best_heart_params.txt; DESC="[heart] $DESC"; else BESTF=research/best_name.txt; PARF=research/best_params.txt; fi
BEST=$(cat "$BESTF")
declare -A P
while IFS='=' read -r k v; do [ -n "$k" ] && P[$k]=$v; done < "$PARF"
if [ "$TRACK" = "heart" ]; then P[keys]=1; P[chest_pol]=1; fi
for kv in "$@"; do P[${kv%%=*}]=${kv#*=}; done
ARGS=(); for k in "${!P[@]}"; do ARGS+=(--param "$k=${P[$k]}"); done
OUT=$(python3 -u research/eval/run.py "$NAME" "$SPLIT" "${ARGS[@]}" --base "$BEST")
echo "$OUT"
for k in "${!P[@]}"; do echo "$k=${P[$k]}"; done | sort > "research/runs/$NAME.params"
D=$(echo "$OUT" | sed -n 's/.*dscore=\([-+0-9.]*\).*/\1/p')
PI=$(echo "$OUT" | sed -n 's/.*P(improve)=\([0-9.]*\).*/\1/p')
STATUS=discard
if python3 -c "import sys; sys.exit(0 if float('$D')>0 and float('$PI')>=0.9 else 1)"; then
  STATUS=keep; TRACK=$TRACK research/promote.sh "$NAME"; echo "KEEP -> new best $NAME ($TRACK)"
fi
python3 research/eval/log.py "$NAME" "$SPLIT" "$STATUS" "$DESC [$*]" --base "$BEST" > /dev/null
