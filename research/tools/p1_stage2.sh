#!/usr/bin/env bash
# Phase P1 stage 2 (pre-registered; see program.md): wait for the seven 2,000-sim policy data sets, train prior v4
# (hidden 64 / 128 / 256; pick the lowest validation CE), side-build it on the fork, leak test, then play all arms on the
# pre-allocated fresh slice fs1206500n1000 (paired): v1 vs v4 at sims=50 depth 0, v1 vs v4 at sims=50 depth 2 c3.0, and the
# 500-sim best (v1) as the reference. No keep: P1 measures the 50-sim gap.
set -uo pipefail
cd "$(dirname "$0")/../.."
D=research/vf/data; SL=fs1206500n1000; MAIN=/home/user/sts_lightspeed_fid/build/fair_eval
for w in 16 17 19 20 21 22; do n=poldata_p1_w${w}_z   # amendment 14:25 UTC: w18 optional (stalled; added below if stored)
  for i in $(seq 1 96); do c=$(python3 research/tools/data_sync.py ls 2>/dev/null | awk -v n=$n '$1==n{print $2}'); [ -n "$c" ] && break; echo "waiting for $n"; sleep 300; done
  python3 research/tools/data_sync.py pull $n && rm -rf /home/user/sts/research/vf/data/$n/_hf/.cache /root/.cache/huggingface/xet
  python3 research/vf/runstore.py unpack $D/$n/all $D/poldata_p1/all
done
c18=$(python3 research/tools/data_sync.py ls 2>/dev/null | awk '$1=="poldata_p1_w18_z"{print $2}')
if [ -n "$c18" ]; then python3 research/tools/data_sync.py pull poldata_p1_w18_z && python3 research/vf/runstore.py unpack $D/poldata_p1_w18_z/all $D/poldata_p1/all; else echo "P1: poldata_p1_w18_z not stored -> trained on six sets (24,000 games)"; fi
ls $D/poldata_p1/all | wc -l
best=""; bce=9
for h in 64 128 256; do
  out=$(python3 research/vf/train_policy_mlp.py $D/poldata_p1/all --out research/models/pfm_p1_h$h.inc --hidden $h --epochs 8 2>&1 | tee research/models/pfm_p1_h$h.log | grep val_ce | tail -1)
  echo "P1 v4 h=$h: $out"; ce=$(echo "$out" | sed -n 's/.*val_ce=\([0-9.]*\).*/\1/p')
  python3 -c "import sys; sys.exit(0 if float('$ce')<float('$bce') else 1)" && { bce=$ce; best=$h; }
done
echo "P1 v4 selected: h=$best (val_ce $bce)"
research/tools/p1_side_build.sh v4 research/models/pfm_p1_h$best.inc | tail -1
V4=/home/user/sts_lightspeed_fid/build_v4/fair_eval
$V4 --leaktest --seed_start 100000 --count 2 $(sed 's/^/--param /' research/best_params.txt | tr '\n' ' ') --param sims=50 | tail -1
A=$(sed 's/^/--param /' research/best_params.txt | tr '\n' ' ')
FAIR_EVAL_BIN=$MAIN python3 research/eval/run.py p1_ref500 $SL $A | sed -n 1p
FAIR_EVAL_BIN=$MAIN python3 research/eval/run.py p1_v1d0 $SL $A --param sims=50 --param prior_mlp_depth=0 --base p1_ref500 | grep -E "^p1_|^vs "
FAIR_EVAL_BIN=$V4 python3 research/eval/run.py p1_v4d0 $SL $A --param sims=50 --param prior_mlp_depth=0 --base p1_v1d0 | grep -E "^p1_|^vs "
FAIR_EVAL_BIN=$MAIN python3 research/eval/run.py p1_v1d2 $SL $A --param sims=50 --param prior_mlp_depth=2 --param prior_c=3.0 --base p1_ref500 | grep -E "^p1_|^vs "
FAIR_EVAL_BIN=$V4 python3 research/eval/run.py p1_v4d2 $SL $A --param sims=50 --param prior_mlp_depth=2 --param prior_c=3.0 --base p1_v1d2 | grep -E "^p1_|^vs "
python3 - "$SL" <<'PY'
import sys, random
sl=sys.argv[1]
def load(n):
    d={}
    for l in open(f"research/runs/{n}.{sl}.a0.tsv"):
        r=l.rstrip("\n").split("\t")
        if r[0].isdigit(): d[int(r[0])]=(int(r[1]), min(float(r[2]),51)/51+int(r[1]))
    return d
R=load("p1_ref500")
for n in ("p1_v1d0","p1_v4d0","p1_v1d2","p1_v4d2"):
    A=load(n); ks=sorted(set(A)&set(R)); d=[A[k][1]-R[k][1] for k in ks]; w=sum(A[k][0] for k in ks)/len(ks); wr=sum(R[k][0] for k in ks)/len(ks)
    print(f"P1 {n} vs ref500: n={len(ks)} win {w:.3f} vs {wr:.3f} gap {100*(w-wr):+.1f} pts dscore {sum(d)/len(d):+.4f}")
PY
echo "P1 STAGE 2 DONE"
