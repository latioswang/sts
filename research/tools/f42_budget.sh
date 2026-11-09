#!/usr/bin/env bash
# Phase F42 (a) OWNER-APPROVED budget test (owner, 2026-11-09 ≈ 20:3x UTC): track best + EXTRA (e.g. sims=2X) vs the best on a fresh slice.
# Logged as status owner-test and NEVER promoted here: the owner decides on the search budget after seeing the result.
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
echo "F42 $TRACK: $NEW ($EXTRA) vs $OLD on $SPLIT"
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
echo "F42 compute: mean search actions ratio new/base = $R"
D=$(echo "$OUT" | sed -n 's/.*dscore=\([-+0-9.]*\).*/\1/p'); PI=$(echo "$OUT" | sed -n 's/.*P(improve)=\([0-9.]*\).*/\1/p')
STATUS=owner-test; echo "F42 OWNER TEST (not promoted; owner decides): dscore=$D P=$PI actions ratio=$R"
python3 research/eval/log.py "$NEW" "$SPLIT" "$STATUS" "[$TRACK] Phase F42 owner budget test: $EXTRA (actions ratio $R)" --base "$OLD" --asc $ASC --winmode $WM > /dev/null
