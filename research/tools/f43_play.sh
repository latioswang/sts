#!/usr/bin/env bash
# Phase F43 (pre-registered in program.md F43): 2,000-seed (N=…) fresh-slice play test of the track best + EXTRA params vs the best.
# Keep iff dscore > 0 and P(improve) >= 0.9 AND mean search actions per game (crash-free paired seeds) <= 1.01x the base's
# (else discard / discard-compute). Near miss -> one extension (ext_fresh.sh) with the compute check by hand.
# usage: f41_play.sh TRACK NAME "key=val key=val ..."   (TRACK = a0 | a10 | a20 | heart; keys in EXTRA replace the best's)
set -euo pipefail
cd "$(dirname "$0")/../.."
TRACK=$1; NEW=$2; EXTRA=$3; N=${N:-2000}
case $TRACK in a0) OLD=$(cat research/best_name.txt); ASC=0; WM=act3 ;; heart) OLD=$(cat research/best_heart_name.txt); ASC=0; WM=heart ;;
  a10) OLD=$(cat research/best_a10_name.txt); ASC=10; WM=act3 ;; a20) OLD=$(cat research/best_a20_name.txt); ASC=20; WM=act3 ;; *) echo "bad track"; exit 1 ;; esac
KEYS=$(for kv in $EXTRA; do echo "^${kv%%=*}="; done | paste -sd'|')
{ grep -v -E "$KEYS" "research/runs/$OLD.params"; for kv in $EXTRA; do echo "$kv"; done; } > "research/runs/$NEW.params"
NF=research/eval/fresh_next.txt; S=$(cat $NF); echo $((S + N)) > $NF; SPLIT=fs${S}n${N}
pa() { sed 's/^/--param /' "research/runs/$1.params" | tr '\n' ' '; }
echo "F43 $TRACK: $NEW ($EXTRA) vs $OLD on $SPLIT"
python3 -u research/eval/run.py "$OLD" "$SPLIT" $(pa "$OLD") --asc $ASC --winmode $WM | sed -n 1p
OUT=$(python3 -u research/eval/run.py "$NEW" "$SPLIT" $(pa "$NEW") --base "$OLD" --asc $ASC --winmode $WM); echo "$OUT"
R=$(python3 - "research/runs/$OLD.$SPLIT.a$ASC.tsv" "research/runs/$NEW.$SPLIT.a$ASC.tsv" <<'PY'
import sys
def rows(f):
    out = {}
    for line in open(f):
        p = line.split()
        if len(p) >= 12 and p[0].isdigit() and p[9] != "2": out[p[0]] = float(p[11])
    return out
A, B = rows(sys.argv[1]), rows(sys.argv[2]); c = set(A) & set(B)
print(f"{sum(B[s] for s in c) / sum(A[s] for s in c):.4f}")
PY
)
echo "F43 compute: mean search actions ratio new/base = $R"
D=$(echo "$OUT" | sed -n 's/.*dscore=\([-+0-9.]*\).*/\1/p'); PI=$(echo "$OUT" | sed -n 's/.*P(improve)=\([0-9.]*\).*/\1/p')
STATUS=discard
if python3 -c "import sys; sys.exit(0 if float('$D')>0 and float('$PI')>=0.9 else 1)"; then
  if python3 -c "import sys; sys.exit(0 if float('$R')<=1.01 else 1)"; then
    STATUS=keep; TRACK=$TRACK research/promote.sh "$NEW"; echo "F43 KEEP -> new $TRACK best $NEW"
  else STATUS=discard-compute; echo "F43 DISCARD-COMPUTE (dscore=$D P=$PI ratio=$R)"; fi
else echo "F43 DISCARD (dscore=$D P=$PI ratio=$R)"; fi
python3 research/eval/log.py "$NEW" "$SPLIT" "$STATUS" "[$TRACK] Phase F43: $EXTRA (actions ratio $R)" --base "$OLD" --asc $ASC --winmode $WM > /dev/null
