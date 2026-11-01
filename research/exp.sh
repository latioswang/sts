#!/usr/bin/env bash
# usage: exp.sh NAME "description" [k=v ...]
# Runs val split with best params + overrides, paired comparison vs current best,
# applies the keep rule (dscore > 0 and P(improve) >= 0.9) -> auto-promote, logs to results.tsv.
set -euo pipefail
cd "$(dirname "$0")/.."
NAME=$1; DESC=$2; shift 2
SPLIT=${SPLIT:-val}
# Tracks: a0 (default, win = beat the Act 3 boss, best_{name,params}.txt); heart (A0, keys forced,
# win = beat the Heart); a10 / a20 (ascension ladders). Non-default tracks keep their best in
# best_<track>_{name,params}.txt. Seed a new ascension track with research/tools/new_track.sh.
TRACK=${TRACK:-a0}
case "$TRACK" in a0) ASC=0;; heart) ASC=0;; a10) ASC=10;; a20) ASC=20;; *) echo "unknown TRACK $TRACK" >&2; exit 1;; esac
if [ "$TRACK" = "a0" ]; then BESTF=research/best_name.txt; PARF=research/best_params.txt; else BESTF=research/best_${TRACK}_name.txt; PARF=research/best_${TRACK}_params.txt; DESC="[$TRACK] $DESC"; fi
BEST=$(cat "$BESTF")
declare -A P
while IFS='=' read -r k v; do [ -n "$k" ] && P[$k]=$v; done < "$PARF"
if [ "$TRACK" = "heart" ]; then P[keys]=1; P[chest_pol]=1; fi
for kv in "$@"; do P[${kv%%=*}]=${kv#*=}; done
ARGS=(); for k in "${!P[@]}"; do ARGS+=(--param "$k=${P[$k]}"); done
if [ ! -f "research/runs/$BEST.$SPLIT.a$ASC.tsv" ]; then echo "best $BEST has no $SPLIT run at ascension $ASC" >&2; exit 1; fi
OUT=$(python3 -u research/eval/run.py "$NAME" "$SPLIT" "${ARGS[@]}" --base "$BEST" --asc "$ASC")
echo "$OUT"
for k in "${!P[@]}"; do echo "$k=${P[$k]}"; done | sort > "research/runs/$NAME.params"
D=$(echo "$OUT" | sed -n 's/.*dscore=\([-+0-9.]*\).*/\1/p')
PI=$(echo "$OUT" | sed -n 's/.*P(improve)=\([0-9.]*\).*/\1/p')
STATUS=discard
if python3 -c "import sys; sys.exit(0 if float('$D')>0 and float('$PI')>=0.9 else 1)"; then
  STATUS=keep; TRACK=$TRACK research/promote.sh "$NAME"; echo "KEEP -> new best $NAME ($TRACK)"
fi
python3 research/eval/log.py "$NAME" "$SPLIT" "$STATUS" "$DESC [$*]" --base "$BEST" --asc "$ASC" > /dev/null
