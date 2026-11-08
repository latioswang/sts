#!/usr/bin/env bash
# Phase F18 (a), measurement only (pre-registered in program.md F18): the cost of taking keys. f16heart with keys=0 on the
# slice f16heart (keys=1) already played (fs1297000n1000, paired seeds); act-3 boss beaten = act >= 4 or victory.
set -euo pipefail
cd "$(dirname "$0")/../.."
S=fs1297000n1000
A=$(sed 's/^/--param /' research/best_heart_params.txt | grep -v -- '--param keys=' | tr '\n' ' ')
python3 -u research/eval/run.py f18m_nokeys "$S" $A --param chest_pol=1 --asc 0 --winmode act3 | sed -n 1p
for n in f16heart f18m_nokeys; do
  awk -F'\t' -v n=$n '{c++; b+=($4>=4||$2==1); a4+=($4>=4); f+=$3} END{printf "F18 KEYCOST %s n=%d beat_act3_boss=%.3f act4=%.3f floor=%.2f\n", n, c, b/c, a4/c, f/c}' research/runs/$n.$S.a0.tsv
done
