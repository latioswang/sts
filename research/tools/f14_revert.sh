#!/usr/bin/env bash
# Phase F14 (a) decision (pre-registered in 20b97b9): promote f2heartp back as the Heart best iff the preceding
# ext_fresh.sh run logged it as a keep-candidate over f6heart_upg.
cd "$(dirname "$0")/../.."
if tail -n 30 research/results.tsv | grep -P "\tkeep-candidate\tf2heartp: " | grep -q "f6heart_upg\|fresh-slice extension"; then
  TRACK=heart research/promote.sh f2heartp; echo "F14 REVERT: Heart best -> f2heartp"
else echo "F14 REVERT: no (Heart best stays $(cat research/best_heart_name.txt))"; fi
