#!/usr/bin/env bash
# P1 stage 2 after amendment 2 (see program.md): every 3rd game of each of the six stored policy sets, v4 at hidden 64 / 128
# (4 epochs), pick the lower validation CE, side build (code only), leak test, then the pre-registered paired arms on
# fs1206500n1000.
set -uo pipefail
cd "$(dirname "$0")/../.."
D=research/vf/data; SL=fs1206500n1000; MAIN=/home/user/sts_lightspeed_fid/build/fair_eval
rm -rf $D/poldata_p1 $D/poldata_p1_tmp; mkdir -p $D/poldata_p1/all
for w in 16 17 19 20 21 22; do
  python3 research/vf/runstore.py unpack $D/poldata_p1_w${w}_z/all $D/poldata_p1_tmp/all > /dev/null
  ls $D/poldata_p1_tmp/all/pol_*.bin | sort | awk 'NR % 3 == 1' | xargs -I{} mv {} $D/poldata_p1/all/
  rm -rf $D/poldata_p1_tmp
done
echo "P1 training files: $(ls $D/poldata_p1/all | wc -l)"
best=""; bce=9
for h in 64 128; do
  python3 research/vf/train_policy_mlp.py $D/poldata_p1/all --out research/models/pfm_p1_h$h.inc --hidden $h --epochs 4 > research/models/pfm_p1_h$h.log 2>&1
  out=$(grep val_ce research/models/pfm_p1_h$h.log | tail -1); echo "P1 v4 h=$h: $(grep decisions= research/models/pfm_p1_h$h.log) | $out"
  ce=$(echo "$out" | sed -n 's/.*val_ce=\([0-9.]*\).*/\1/p'); [ -n "$ce" ] && python3 -c "import sys; sys.exit(0 if float('$ce')<float('$bce') else 1)" && { bce=$ce; best=$h; }
done
echo "P1 v4 selected: h=$best (val_ce $bce)"; [ -n "$best" ] || { echo "P1 TRAINING FAILED"; exit 1; }
research/tools/p1_side_build.sh v4 research/models/pfm_p1_h$best.inc | tail -1
V4=/home/user/sts_lightspeed_fid/build_v4/fair_eval
A=$(sed 's/^/--param /' research/best_params.txt | tr '\n' ' ')
$V4 --leaktest --seed_start 100000 --count 2 $A --param sims=50 | tail -1
FAIR_EVAL_BIN=$MAIN python3 research/eval/run.py p1_ref500 $SL $A | sed -n 1p
FAIR_EVAL_BIN=$MAIN python3 research/eval/run.py p1_v1d0 $SL $A --param sims=50 --param prior_mlp_depth=0 --base p1_ref500 | grep -E "^p1_|^vs "
FAIR_EVAL_BIN=$V4 python3 research/eval/run.py p1_v4d0 $SL $A --param sims=50 --param prior_mlp_depth=0 --base p1_v1d0 | grep -E "^p1_|^vs "
FAIR_EVAL_BIN=$MAIN python3 research/eval/run.py p1_v1d2 $SL $A --param sims=50 --param prior_mlp_depth=2 --param prior_c=3.0 --base p1_ref500 | grep -E "^p1_|^vs "
FAIR_EVAL_BIN=$V4 python3 research/eval/run.py p1_v4d2 $SL $A --param sims=50 --param prior_mlp_depth=2 --param prior_c=3.0 --base p1_v1d2 | grep -E "^p1_|^vs "
python3 - "$SL" <<'PY'
import sys
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
