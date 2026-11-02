#!/usr/bin/env bash
# Parameters present in the A0 best but absent (or different) in each other track's best — ports to consider.
cd "$(dirname "$0")/../.."
for T in heart a10 a20; do
  F=research/best_${T}_params.txt; [ -f "$F" ] || continue
  D=$(comm -23 <(sort research/best_params.txt) <(sort "$F") | tr '\n' ' ')
  echo "$T ($(cat research/best_${T}_name.txt)) lacks: ${D:-nothing}"
done
