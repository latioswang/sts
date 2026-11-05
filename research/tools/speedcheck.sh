#!/usr/bin/env bash
# Time two evaluator binaries on the same games (single-threaded, idle machine) and require identical
# game outputs. usage: speedcheck.sh OLD_BIN NEW_BIN [count=8] [seed_start=200000] [k=v extra params...]
set -euo pipefail
cd "$(dirname "$0")/../.."
OLD=${1:?old binary}; NEW=${2:?new binary}; COUNT=${3:-8}; SEED=${4:-200000}; shift 4 || shift $#
if pgrep -x fair_eval >/dev/null; then echo "machine busy — refusing to time" >&2; exit 2; fi
ARGS=$(sed 's/^/--param /' research/best_params.txt | tr '\n' ' '); for kv in "$@"; do ARGS="$ARGS --param $kv"; done
run() { local s=$(date +%s.%N); "$1" --agent fair --inproc --seed_start "$SEED" --count "$COUNT" --asc 0 $ARGS 2>/dev/null | cut -f1-5 > "$2"; local e=$(date +%s.%N); python3 -c "print(round($e-$s,2))"; }
T1=$(run "$OLD" /tmp/speed_old.txt); T2=$(run "$NEW" /tmp/speed_new.txt); T3=$(run "$OLD" /tmp/speed_old2.txt); T4=$(run "$NEW" /tmp/speed_new2.txt)
cmp -s /tmp/speed_old.txt /tmp/speed_new.txt && echo "outputs identical on $COUNT games" || { echo "OUTPUT MISMATCH"; diff /tmp/speed_old.txt /tmp/speed_new.txt | head -5; exit 1; }
python3 - "$T1" "$T2" "$T3" "$T4" <<'PY'
import sys; a,b,c,d = map(float, sys.argv[1:])
old=(a+c)/2; new=(b+d)/2
print(f"old: {a}s,{c}s (mean {old:.2f})  new: {b}s,{d}s (mean {new:.2f})  speedup old/new = {old/new:.3f}")
PY
