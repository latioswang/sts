#!/usr/bin/env bash
# Phase F19 arm (pre-registered in program.md F19): compute-neutral Heart search reallocation from f19_calib.txt; leak test,
# fresh-slice rule; a keep stands only if the challenger's mean search actions per game are <= 1.01 x the base's on the
# same seeds (else the promotion is reverted and logged discard-compute).
set -euo pipefail
cd "$(dirname "$0")/../.."
X=$(sed -n 's/^sims=//p' research/runs/f19_calib.txt); M=$(sed -n 's/^heart_mult=//p' research/runs/f19_calib.txt)
R=$(/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --asc 0 \
    $(sed 's/^/--param /' research/best_heart_params.txt | tr '\n' ' ') --param keys=1 --param chest_pol=1 --param sims=$X --param heart_mult=$M | tail -1)
echo "leaktest_heart [sims=$X heart_mult=$M]: $R"; case "$R" in *PASS*) ;; *) exit 1;; esac
B0=$(cat research/best_heart_name.txt); cp research/best_heart_name.txt research/runs/.f19_bn; cp research/best_heart_params.txt research/runs/.f19_bp
NEXT=$(cat research/eval/fresh_next.txt); SPLIT=fs${NEXT}n1000
TRACK=heart research/tools/exp_fresh.sh f19heart_hm "Phase F19: compute-neutral Heart search reallocation (sims=$X heart_mult=$M)" sims=$X heart_mult=$M
python3 - "$B0" "$SPLIT" <<'PY'
import sys
b0,sp=sys.argv[1:3]
def rows(p):
    return {l.split('\t')[0]: float(l.split('\t')[11]) for l in open(p) if len(l.split('\t'))>=12 and l.split('\t')[9]!='2'}
b=rows(f'research/runs/{b0}.{sp}.a0.tsv'); c=rows(f'research/runs/f19heart_hm.{sp}.a0.tsv'); s=set(b)&set(c)
Ab=sum(b[k] for k in s)/len(s); Ac=sum(c[k] for k in s)/len(s)
print(f"F19 COMPUTE n={len(s)} base_actions={Ab:.0f} challenger_actions={Ac:.0f} ratio={Ac/Ab:.4f}")
open('research/runs/.f19_ratio','w').write(f"{Ac/Ab:.4f}")
PY
if [ "$(cat research/best_heart_name.txt)" = f19heart_hm ] && python3 -c "import sys; sys.exit(0 if float(open('research/runs/.f19_ratio').read())>1.01 else 1)"; then
  cp research/runs/.f19_bn research/best_heart_name.txt; cp research/runs/.f19_bp research/best_heart_params.txt
  python3 research/eval/log.py f19heart_hm "$SPLIT" discard-compute "[heart] Phase F19: keep reverted, search actions ratio $(cat research/runs/.f19_ratio) > 1.01" --base "$B0" --asc 0 --winmode heart > /dev/null
  echo "F19 KEEP REVERTED (compute ratio $(cat research/runs/.f19_ratio))"
fi
