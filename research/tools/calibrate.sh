#!/usr/bin/env bash
# Equal-wall-clock calibration: time single-threaded games for a candidate config vs the current best.
# usage: calibrate.sh "k=v k=v ..." [count=8] [seed_start=100010]
# Refuses to run if other fair_eval* processes are active (timings must come from an idle machine).
set -euo pipefail
cd "$(dirname "$0")/.."
CAND=${1:?candidate params, e.g. \"leaf_vf=1 rollout_turns=0 sims=3000\"}
COUNT=${2:-8}
SEED=${3:-100010}
BIN=${FAIR_EVAL_BIN:-/home/user/sts_lightspeed/build/fair_eval}
if pgrep -f "fair_eval" >/dev/null; then echo "machine busy (fair_eval processes running) — refusing to time" >&2; exit 2; fi
BASE=$(sed 's/^/--param /' research/best_params.txt | tr '\n' ' ')
CANDARGS=$BASE; for kv in $CAND; do CANDARGS="$CANDARGS --param $kv"; done
t() { local s=$(date +%s.%N); "$BIN" --inproc --seed_start "$SEED" --count "$COUNT" $1 >/dev/null 2>&1; local e=$(date +%s.%N); python3 -c "print(round($e-$s,2))"; }
TB=$(t "$BASE"); TC=$(t "$CANDARGS")
python3 - "$TB" "$TC" <<'EOF'
import sys; tb, tc = float(sys.argv[1]), float(sys.argv[2])
print(f"best: {tb}s  candidate: {tc}s  candidate/best = {tc/tb:.3f}  (scale candidate sims by {tb/tc:.3f} for equal wall-clock)")
EOF
