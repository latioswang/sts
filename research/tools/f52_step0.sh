#!/usr/bin/env bash
# F52 step 0 (pre-registered in program.md): local, before any worker phase.
#  (0) rebuild build/ with the mix_sv/mix_lambda params; gates bit-identical on all four tracks; leak test with the mix on
#  (1) wall-clock calibration vs the A0 best of: leaf_sv=16 rollout_turns=0 (pure learned leaf) and mix_sv=16 mix_lambda=0.5
#  (2) re-baseline: sv16 (pure learned leaf) vs the A0 best at equal wall-clock, 300 fresh seeds, measurement only (status scaling)
#  (3) AlphaGo-style mixed leaf: f43_play.sh a0 f52_mix05 at equal wall-clock (sims scaled down by the measured cost), 2,000 fresh seeds, keep rule
set -euo pipefail
cd "$(dirname "$0")/../.."
BIN=/home/user/sts_lightspeed/build/fair_eval
pgrep -x fair_eval >/dev/null && { echo "fair_eval running - refusing to rebuild"; exit 2; }
echo "== F52 step 0 start $(date -u '+%F %T UTC')"
make -C /home/user/sts_lightspeed/build -j4 fair_eval 2>&1 | tail -2
for g in "cb0_a0 fs1428500n2000" "cb0_a10 fs1426500n2000" "cb0_a20 fs1424500n2000" "cb0_heart fs1430500n2000"; do
  set -- $g; research/tools/repro_check.sh "$1" 16 "$2" | tail -1
done
ARGS=$(sed 's/^/--param /' research/best_params.txt | tr '\n' ' ')
"$BIN" --leaktest --seed_start 100000 --count 2 $ARGS --param mix_sv=16 --param mix_lambda=0.5 | tail -1
C1=$(research/tools/calibrate.sh "leaf_sv=16 rollout_turns=0" 8 100010 | tail -1); echo "calib sv16: $C1"
C2=$(research/tools/calibrate.sh "mix_sv=16 mix_lambda=0.5" 8 100010 | tail -1); echo "calib mix05: $C2"
f() { echo "$1" | sed -n 's/.*scale candidate sims by \([0-9.]*\).*/\1/p'; }
S1=$(python3 -c "print(max(50, int(1000*float('$(f "$C1")'))//25*25))")
S2=$(python3 -c "print(min(1000, int(1000*float('$(f "$C2")'))//25*25))")
echo "sims: sv16 $S1, mix05 $S2"
# (2) re-baseline, measurement only
BEST=$(cat research/best_name.txt)
NF=research/eval/fresh_next.txt; S=$(cat $NF); echo $((S + 300)) > $NF; SPLIT=fs${S}n300
pa() { sed 's/^/--param /' "research/runs/$1.params" | tr '\n' ' '; }
{ grep -v -E '^(leaf_sv|rollout_turns|sims)=' "research/runs/$BEST.params"; echo leaf_sv=16; echo rollout_turns=0; echo sims=$S1; } > research/runs/f52_sv16.params
python3 -u research/eval/run.py "$BEST" "$SPLIT" $(pa "$BEST") --asc 0 | sed -n 1p
python3 -u research/eval/run.py f52_sv16 "$SPLIT" $(pa f52_sv16) --base "$BEST" --asc 0 | tail -2
python3 research/eval/log.py f52_sv16 "$SPLIT" scaling "[a0] F52 step 0: re-baseline of the pure learned leaf sv_16 (rollout_turns=0) at equal wall-clock (sims $S1) vs $BEST" --base "$BEST" --asc 0 > /dev/null
# (3) mixed leaf, keep rule (f43_play: dscore>0, P>=0.9, actions ratio<=1.01)
research/tools/f43_play.sh a0 f52_mix05 "mix_sv=16 mix_lambda=0.5 sims=$S2"
echo "== F52 step 0 done $(date -u '+%F %T UTC')"
