#!/usr/bin/env bash
# F42 (a) re-run of ONLY the 2x arm on the SAME split after the runner was stopped (base TSV already exists; fresh_next not advanced).
# Same comparison, compute check and owner-test logging as f42_budget.sh. usage: f42_arm.sh TRACK NEW SPLIT
set -euo pipefail
cd "$(dirname "$0")/../.."
TRACK=$1; NEW=$2; SPLIT=$3
case $TRACK in a0) OLD=$(cat research/best_name.txt); ASC=0; WM=act3 ;; heart) OLD=$(cat research/best_heart_name.txt); ASC=0; WM=heart ;;
  a10) OLD=$(cat research/best_a10_name.txt); ASC=10; WM=act3 ;; a20) OLD=$(cat research/best_a20_name.txt); ASC=20; WM=act3 ;; *) echo "bad track"; exit 1 ;; esac
test -f "research/runs/$OLD.$SPLIT.a$ASC.tsv" || { echo "missing base TSV"; exit 1; }
pa() { sed 's/^/--param /' "research/runs/$1.params" | tr '\n' ' '; }
echo "F42 $TRACK (arm re-run): $NEW vs $OLD on $SPLIT"
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
echo "F42 OWNER TEST (not promoted; owner decides): dscore=$D P=$PI actions ratio=$R"
python3 research/eval/log.py "$NEW" "$SPLIT" owner-test "[$TRACK] Phase F42 owner budget test: $(grep '^sims=' research/runs/$NEW.params) (actions ratio $R)" --base "$OLD" --asc $ASC --winmode $WM > /dev/null
