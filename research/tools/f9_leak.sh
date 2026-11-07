#!/usr/bin/env bash
# Phase F9 leak tests: each track best + each new parameter combination (exit 1 on any FAIL).
cd "$(dirname "$0")/../.."
F=0
run() { local t=$1 A=$2; shift 2
  local X=""; [ "$t" = _heart ] && X="--param keys=1 --param chest_pol=1"
  local R; R=$(/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --asc "$A" \
      $(sed 's/^/--param /' "research/best${t}_params.txt" | tr '\n' ' ') $X "$@" | tail -1)
  echo "leaktest${t:-_a0} [$*]: $R"; case "$R" in *PASS*) ;; *) F=1;; esac; }
run _heart 0 --param prior_mlp=1 --param prior_mlp_depth=0
run _heart 0 --param potion_v=8 --param potion_v_boss=4
run _heart 0 --param map_discount=0.85
run _a10 10 --param map_discount=0.85
run _a10 10 --param potion_v=8 --param potion_v_boss=4
run _a20 20 --param map_discount=0.85
run _a20 20 --param potion_v=8
run "" 0 --param elite_hp=0.75
exit $F
