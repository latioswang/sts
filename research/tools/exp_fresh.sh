#!/usr/bin/env bash
# Fresh-slice keep protocol (adopted 2026-11-05 after the A20 selection-bias diagnostic): every comparison runs on a
# new, never-used seed slice, the current best is re-run on that slice, and the paired decision (dscore > 0, P >= 0.9)
# uses only that slice — the base was never selected on those seeds. Slices come from research/eval/fresh_next.txt
# (allocated before any game; never reused). Size: 500 seeds on a0/a10, 1000 on a20/heart (low win rates).
# usage: TRACK=a0|a10|a20|heart [FRESH_SLICE=start] exp_fresh.sh NAME "description" [k=v ...]
set -euo pipefail
cd "$(dirname "$0")/../.."
NAME=$1; DESC=$2; shift 2
TRACK=${TRACK:-a0}
case "$TRACK" in a0) ASC=0;; heart) ASC=0;; a10) ASC=10;; a20) ASC=20;; *) echo "unknown TRACK $TRACK" >&2; exit 1;; esac
WINMODE=act3; [ "$TRACK" = heart ] && WINMODE=heart
N=500; { [ "$TRACK" = a20 ] || [ "$TRACK" = heart ]; } && N=1000
if [ "$TRACK" = a0 ]; then BESTF=research/best_name.txt; PARF=research/best_params.txt; else BESTF=research/best_${TRACK}_name.txt; PARF=research/best_${TRACK}_params.txt; DESC="[$TRACK] $DESC"; fi
BEST=$(cat "$BESTF")
NF=research/eval/fresh_next.txt
if [ -n "${FRESH_SLICE:-}" ]; then S=$FRESH_SLICE   # resume an experiment on the slice it was already allocated (never a used one)
else S=$(cat "$NF"); echo $((S + N)) > "$NF"; fi   # allocate the slice first
SPLIT=fs${S}n${N}
declare -A P; while IFS='=' read -r k v; do [ -n "$k" ] && P[$k]=$v; done < "$PARF"
if [ "$TRACK" = heart ]; then P[keys]=1; P[chest_pol]=1; fi
BASEARGS=(); for k in "${!P[@]}"; do BASEARGS+=(--param "$k=${P[$k]}"); done
for kv in "$@"; do P[${kv%%=*}]=${kv#*=}; done
ARGS=(); for k in "${!P[@]}"; do ARGS+=(--param "$k=${P[$k]}"); done
echo "fresh slice $SPLIT (seeds $S..$((S + N - 1))) base $BEST"
[ -f "research/runs/$BEST.$SPLIT.a$ASC.tsv" ] || python3 -u research/eval/run.py "$BEST" "$SPLIT" "${BASEARGS[@]}" --asc "$ASC" --winmode "$WINMODE" | sed -n 1p   # sed reads to the end (head closed the pipe: BrokenPipe)
OUT=$(python3 -u research/eval/run.py "$NAME" "$SPLIT" "${ARGS[@]}" --base "$BEST" --asc "$ASC" --winmode "$WINMODE")
echo "$OUT"
for k in "${!P[@]}"; do echo "$k=${P[$k]}"; done | sort > "research/runs/$NAME.params"
D=$(echo "$OUT" | sed -n 's/.*dscore=\([-+0-9.]*\).*/\1/p'); PI=$(echo "$OUT" | sed -n 's/.*P(improve)=\([0-9.]*\).*/\1/p')
STATUS=discard
if python3 -c "import sys; sys.exit(0 if float('$D')>0 and float('$PI')>=0.9 else 1)"; then
  STATUS=keep; TRACK=$TRACK research/promote.sh "$NAME"; echo "KEEP (fresh slice) -> new best $NAME ($TRACK)"
fi
python3 research/eval/log.py "$NAME" "$SPLIT" "$STATUS" "$DESC [fresh slice; $*]" --base "$BEST" --asc "$ASC" --winmode "$WINMODE" > /dev/null
