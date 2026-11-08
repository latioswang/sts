#!/usr/bin/env bash
# Phase F22 arm (pre-registered in program.md F22): compute-neutral A20 boss search reallocation from f22_calib.txt; leak test,
# fresh-slice rule; a keep stands only if the challenger's mean search actions per game are <= 1.01 x the base's on the
# same seeds (else the promotion is reverted and logged discard-compute).
set -euo pipefail
cd "$(dirname "$0")/../.."
X=$(sed -n 's/^sims=//p' research/runs/f22_calib.txt); M=$(sed -n 's/^boss_mult=//p' research/runs/f22_calib.txt)
R=$(/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --asc 20 \
    $(sed 's/^/--param /' research/best_a20_params.txt | tr '\n' ' ') --param sims=$X --param boss_mult=$M | tail -1)
echo "leaktest_a20 [sims=$X boss_mult=$M]: $R"; case "$R" in *PASS*) ;; *) exit 1;; esac
B0=$(cat research/best_a20_name.txt); cp research/best_a20_name.txt research/runs/.f22_bn; cp research/best_a20_params.txt research/runs/.f22_bp
NEXT=$(cat research/eval/fresh_next.txt); SPLIT=fs${NEXT}n1000
TRACK=a20 research/tools/exp_fresh.sh f22a20_bm "Phase F22: compute-neutral A20 boss search reallocation (sims=$X boss_mult=$M)" sims=$X boss_mult=$M
python3 - "$B0" "$SPLIT" <<'PY'
import sys
b0,sp=sys.argv[1:3]
def rows(p):
    return {l.split('\t')[0]: float(l.split('\t')[11]) for l in open(p) if len(l.split('\t'))>=12 and l.split('\t')[9]!='2'}
b=rows(f'research/runs/{b0}.{sp}.a20.tsv'); c=rows(f'research/runs/f22a20_bm.{sp}.a20.tsv'); s=set(b)&set(c)
Ab=sum(b[k] for k in s)/len(s); Ac=sum(c[k] for k in s)/len(s)
print(f"F22 COMPUTE n={len(s)} base_actions={Ab:.0f} challenger_actions={Ac:.0f} ratio={Ac/Ab:.4f}")
open('research/runs/.f22_ratio','w').write(f"{Ac/Ab:.4f}")
PY
if [ "$(cat research/best_a20_name.txt)" = f22a20_bm ] && python3 -c "import sys; sys.exit(0 if float(open('research/runs/.f22_ratio').read())>1.01 else 1)"; then
  cp research/runs/.f22_bn research/best_a20_name.txt; cp research/runs/.f22_bp research/best_a20_params.txt
  python3 research/eval/log.py f22a20_bm "$SPLIT" discard-compute "[a20] Phase F22: keep reverted, search actions ratio $(cat research/runs/.f22_ratio) > 1.01" --base "$B0" --asc 20 --winmode act3 > /dev/null
  echo "F22 KEEP REVERTED (compute ratio $(cat research/runs/.f22_ratio))"
fi
