#!/usr/bin/env bash
# Pre-registered extension of a pooled-rule near-miss (0.7 <= P < 0.9 on val+dev2+dev3): run the dev slice
# once, then decide on the pooled 600 (val, dev2, dev3, dev): keep needs pooled P >= 0.9 AND dev-alone
# dscore > 0. usage: TRACK=a20|heart ext_pooled.sh NAME BASE
set -euo pipefail
cd "$(dirname "$0")/../.."
NAME=$1; BASE=$2; TRACK=${TRACK:?}
case "$TRACK" in a0|heart) ASC=0;; a10) ASC=10;; a20) ASC=20;; *) echo "unknown TRACK $TRACK" >&2; exit 1;; esac
WINMODE=act3; [ "$TRACK" = heart ] && WINMODE=heart
TRACK=$TRACK research/tools/rep_dev.sh "$NAME" "$BASE"
OUT=$(python3 research/tools/pooled.py "$NAME" "$BASE" "$ASC" val,dev2,dev3,dev "$WINMODE" | tee "research/runs/pooled600_$NAME.txt")
echo "$OUT"
DEVD=$(python3 - "$NAME" "$BASE" "$ASC" "$WINMODE" <<'PY'
import sys, os
sys.path.insert(0, "research/eval")
from run import load, ok, paired, ROOT
n, b, asc, wm = sys.argv[1:5]
A = ok(load(os.path.join(ROOT, "runs", f"{n}_dev.dev.a{asc}.tsv"), wm)); B = ok(load(os.path.join(ROOT, "runs", f"{b}_dev.dev.a{asc}.tsv"), wm))
print(paired(A, B)[1])
PY
)
P=$(echo "$OUT" | head -1 | sed -n 's/.*P(improve)=\([0-9.]*\).*/\1/p')
if python3 -c "import sys; sys.exit(0 if float('$P')>=0.9 and float('$DEVD')>0 else 1)"; then
  TRACK=$TRACK research/promote.sh "$NAME"; echo "KEEP (pooled 600, dev-alone dscore=$DEVD) -> new best $NAME ($TRACK)"
  python3 research/eval/log.py "$NAME" val keep "[$TRACK pooled600 extension] KEEP: $(echo "$OUT" | head -1 | sed 's/.*dscore=/pooled600 dscore=/') dev-alone dscore=$DEVD" --base "$BASE" --asc "$ASC" --winmode "$WINMODE" > /dev/null
else
  echo "no (pooled 600 P=$P, dev-alone dscore=$DEVD)"
  python3 research/eval/log.py "$NAME" val discard "[$TRACK pooled600 extension] no: $(echo "$OUT" | head -1 | sed 's/.*dscore=/pooled600 dscore=/') dev-alone dscore=$DEVD" --base "$BASE" --asc "$ASC" --winmode "$WINMODE" > /dev/null
fi
