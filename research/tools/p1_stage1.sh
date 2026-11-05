#!/usr/bin/env bash
# Phase P1 stage 1 (pre-registered): prior usage at sims=50 on the fidelity simulator, 200 train games (seeds 400000-400199;
# tuning only, never a keep), plus the 500-sim best as reference on the same seeds. Output: research/runs/p1s1_*.train2f.tsv
set -uo pipefail
cd "$(dirname "$0")/../.."
BIN=/home/user/sts_lightspeed_fid/build/fair_eval
ARGS=$(sed 's/^/--param /' research/best_params.txt | tr '\n' ' ')
run() { local name=$1; shift; local P=""; for kv in "$@"; do P="$P --param $kv"; done
  local t0=$(date +%s.%N); $BIN --agent fair --seed_start 400000 --count 200 --threads 4 $ARGS $P 2>/dev/null > research/runs/$name.train2f.tsv
  local t1=$(date +%s.%N); python3 - "$name" "$(echo "$t1-$t0" | bc)" <<'PY'
import sys
n,t=sys.argv[1],float(sys.argv[2]); r=[l.split('\t') for l in open(f'research/runs/{n}.train2f.tsv') if l[0].isdigit()]
print(f"P1S1 {n}: games {len(r)} wins {sum(int(x[1]) for x in r)} floor {sum(float(x[2]) for x in r)/len(r):.2f} aborted {sum(int(x[9]) for x in r)} wall {t:.0f}s", flush=True)
PY
}
run p1s1_ref500 sims=500
for d in 0 1 2; do for c in 1.0 1.5 3.0; do run p1s1_d${d}_c${c} sims=50 prior_mlp_depth=$d prior_c=$c; done; done
echo "P1S1 DONE"
