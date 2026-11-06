#!/usr/bin/env bash
# Simulator switch, step 4 (pre-registered 2026-11-06, see program.md): re-measure the four track bests on the fidelity fork, each
# on its own never-used 1,000-seed slice. Report only (no keep/discard). Leak test per track at its own ascension first.
set -uo pipefail
cd "$(dirname "$0")/../.."
BIN=/home/user/sts_lightspeed/build/fair_eval   # -> /home/user/sts_lightspeed_fid (fork dfda16c)
run_track() {  # TRACK SLICE_START
  local T=$1 S=$2 ASC=0 WM=act3 PF NF X=()
  case $T in a10) ASC=10;; a20) ASC=20;; heart) WM=heart; X=(--param keys=1 --param chest_pol=1);; esac
  if [ "$T" = a0 ]; then PF=research/best_params.txt; NF=research/best_name.txt; else PF=research/best_${T}_params.txt; NF=research/best_${T}_name.txt; fi
  local B; B=$(cat "$NF"); local A; A=$(sed 's/^/--param /' "$PF" | tr '\n' ' ')
  local RF="research/runs/fid_$B.fs${S}n1000.a$ASC.tsv"
  if [ -f "$RF" ] && [ "$(grep -c '^[0-9]' "$RF")" -ge 1000 ]; then echo "== $T: $B already done ($RF), skipped (resume after a restart)"; return; fi
  echo "== $T: $B on fs${S}n1000 (asc $ASC, winmode $WM)"
  $BIN --leaktest --seed_start 100000 --count 2 --asc $ASC $A "${X[@]}" | tail -1
  python3 -u research/eval/run.py "fid_$B" "fs${S}n1000" $A "${X[@]}" --asc $ASC --winmode $WM | sed -n 1p
  python3 research/eval/log.py "fid_$B" "fs${S}n1000" report "[$T] simulator switch: honest level of the $T best $B on the fidelity fork (report only)" --asc $ASC --winmode $WM > /dev/null
}
run_track a0 1210000; run_track a10 1211000; run_track a20 1212000; run_track heart 1213000
echo "SIM REBASELINE DONE"
