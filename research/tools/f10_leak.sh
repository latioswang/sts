#!/usr/bin/env bash
# Phase F10 leak tests: (1) combat leak test with each track best + vr_event=1; (2) event leak test: 20 games per track,
# --inproc, ev_leaktest=1; the event choice must not change when the real RNG streams are re-seeded (0 mismatches,
# >= 50 decisions checked per track). Exit 1 on any failure.
cd "$(dirname "$0")/../.."
B=/home/user/sts_lightspeed/build/fair_eval; F=0
for t in "" _a10 _a20 _heart; do
  A=0; [ "$t" = _a10 ] && A=10; [ "$t" = _a20 ] && A=20
  X=""; [ "$t" = _heart ] && X="--param keys=1 --param chest_pol=1"
  PA=$(sed 's/^/--param /' "research/best${t}_params.txt" | tr '\n' ' ')
  R=$($B --leaktest --seed_start 100000 --count 2 --asc $A $PA $X --param vr_event=1 | tail -1)
  echo "leaktest${t:-_a0}: $R"; case "$R" in *PASS*) ;; *) F=1;; esac
  E=$($B --agent fair --seed_start 101000 --count 20 --asc $A --threads 1 --inproc $PA $X --param vr_event=1 --param ev_leaktest=1 2>&1 >/dev/null \
      | awk '/^EVLEAK/ {split($2,a,"="); split($3,b,"="); split($4,c,"="); n+=a[2]; m+=b[2]; u+=c[2]} END {printf "%d %d %d", n, m, u}')
  read n m u <<< "$E"
  echo "evleak${t:-_a0}: checked=$n mismatches=$m used=$u"
  { [ "$m" = 0 ] && [ "$n" -ge 50 ]; } || F=1
done
exit $F
