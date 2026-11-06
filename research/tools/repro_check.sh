#!/usr/bin/env bash
# Weights/binary consistency check: re-run the first COUNT games of a recorded experiment with the
# current fair_eval binary and compare every column except timing against the stored TSV.
# usage: repro_check.sh NAME [count=16] [split=val]   (NAME must have research/runs/NAME.params and a val TSV)
# Exit 0 = bit-identical, 1 = differs (the binary no longer reproduces the recorded best: stop and
# find out why before trusting new paired comparisons).
set -euo pipefail
cd "$(dirname "$0")/../.."
NAME=${1:?experiment name}; COUNT=${2:-16}
BIN=${FAIR_EVAL_BIN:-/home/user/sts_lightspeed/build/fair_eval}
SPLIT=${3:-val}   # optional: a recorded split other than val (e.g. fs1210000n1000 for the fork gates, 2026-11-06)
TSV=$(ls research/runs/"$NAME".$SPLIT.a*.tsv | head -1)
S0=$(awk -F'\t' '$1 ~ /^[0-9]+$/ && (m == "" || $1 < m) {m = $1} END {print m}' "$TSV")   # lowest recorded seed
ASC=$(echo "$TSV" | sed 's/.*\.a\([0-9]*\)\.tsv/\1/')
ARGS=$(sed 's/^/--param /' "research/runs/$NAME.params" | tr '\n' ' ')
OUT=$(mktemp)
"$BIN" --agent fair --seed_start "$S0" --count "$COUNT" --asc "$ASC" --threads 4 $ARGS > "$OUT"
python3 - "$TSV" "$OUT" "$COUNT" <<'PY'
import sys
ref, new, n = sys.argv[1], sys.argv[2], int(sys.argv[3])
# columns: seed win floor act hp max_hp deck battles decisions aborted secs [actions]; no header row
def rows(p):
    out = {}
    for line in open(p):
        f = line.rstrip('\n').split('\t')
        if len(f) >= 11 and f[0].isdigit(): out[int(f[0])] = f
    return out
R, N = rows(ref), rows(new)
names = ['win', 'floor', 'act', 'hp', 'max_hp', 'deck', 'battles', 'decisions', 'aborted']
bad = 0
for s in sorted(N):
    if s not in R: continue
    d = [names[i-1] for i in range(1, 10) if R[s][i] != N[s][i]]
    if len(R[s]) > 11 and len(N[s]) > 11 and R[s][11] != N[s][11]: d.append('actions')
    if d: bad += 1; print(f"seed {s}: differs in {d}")
print(f"repro_check {ref}: {len(N)} games compared, {bad} differ -> {'OK bit-identical' if bad == 0 else 'MISMATCH'}")
sys.exit(1 if bad else 0)
PY
