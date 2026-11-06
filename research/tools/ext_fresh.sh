#!/usr/bin/env bash
# Near-miss extension under the fresh-slice rule (2026-11-06): a fresh-slice result with dscore > 0 and 0.7 <= P < 0.9 gets ONE
# second never-used slice (same size), base and challenger re-run there; keep iff pooled (both slices) P >= 0.9 AND the second
# slice alone has dscore > 0 (mirrors ext_pooled.sh). usage: TRACK=a0|a10|a20|heart ext_fresh.sh NAME FIRST_SPLIT
set -euo pipefail
cd "$(dirname "$0")/../.."
NAME=$1; S1=$2; TRACK=${TRACK:?}
case "$TRACK" in a0|heart) ASC=0;; a10) ASC=10;; a20) ASC=20;; *) echo "unknown TRACK $TRACK" >&2; exit 1;; esac
WINMODE=act3; [ "$TRACK" = heart ] && WINMODE=heart
if [ "$TRACK" = a0 ]; then BESTF=research/best_name.txt; PARF=research/best_params.txt; else BESTF=research/best_${TRACK}_name.txt; PARF=research/best_${TRACK}_params.txt; fi
BASE=${BASE:-$(cat "$BESTF")}; N=${S1##*n}   # BASE env: judge against a fixed earlier base (two near misses on one base)
[ "$BASE" != "$(cat "$BESTF")" ] && PARF=research/runs/$BASE.params
NF=research/eval/fresh_next.txt
if [ -n "${FRESH_SLICE:-}" ]; then S=$FRESH_SLICE; else S=$(cat "$NF"); echo $((S + N)) > "$NF"; fi
S2=fs${S}n${N}
BA=$(sed 's/^/--param /' "$PARF" | tr '\n' ' '); CA=$(sed 's/^/--param /' "research/runs/$NAME.params" | tr '\n' ' ')
[ "$TRACK" = heart ] && BA="$BA --param keys=1 --param chest_pol=1"
echo "extension slice $S2 for $NAME vs $BASE (first slice $S1)"
[ -f "research/runs/$BASE.$S2.a$ASC.tsv" ] || python3 -u research/eval/run.py "$BASE" "$S2" $BA --asc "$ASC" --winmode "$WINMODE" | sed -n 1p
OUT2=$(python3 -u research/eval/run.py "$NAME" "$S2" $CA --base "$BASE" --asc "$ASC" --winmode "$WINMODE"); echo "$OUT2"
D2=$(echo "$OUT2" | sed -n 's/.*dscore=\([-+0-9.]*\).*/\1/p')
OUTP=$(python3 research/tools/pooled.py "$NAME" "$BASE" "$ASC" "$S1,$S2" "$WINMODE"); echo "POOLED $S1+$S2: $OUTP"
P=$(echo "$OUTP" | sed -n 's/.*P(improve)=\([0-9.]*\).*/\1/p' | head -1)
STATUS=discard
if python3 -c "import sys; sys.exit(0 if float('$P')>=0.9 and float('$D2')>0 else 1)"; then
  if [ "${NOPROMOTE:-0}" = 1 ]; then STATUS=keep-candidate; echo "KEEP-CANDIDATE (fresh-slice extension, not promoted): pooled P=$P second-slice dscore=$D2 pooled $(echo "$OUTP" | sed -n 's/.*\(dscore=[-+0-9.]*\).*/\1/p' | head -1)"
  else STATUS=keep; TRACK=$TRACK research/promote.sh "$NAME"; echo "KEEP (fresh-slice extension) -> new best $NAME ($TRACK)"; fi
else echo "DISCARD (fresh-slice extension: pooled P=$P, second-slice dscore=$D2)"; fi
python3 research/eval/log.py "$NAME" "$S2" "$STATUS" "[$TRACK] fresh-slice extension of a near miss ($S1 + $S2 pooled P=$P, second-slice dscore=$D2)" --base "$BASE" --asc "$ASC" --winmode "$WINMODE" > /dev/null
