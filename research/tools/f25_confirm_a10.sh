#!/usr/bin/env bash
# Phase F25 (pre-registered in program.md F25): confirm f24a10 vs f6a10_norest on a fresh 2,000-seed A10 slice, only if the
# F24 extension kept f24a10 (else nothing to confirm).
set -euo pipefail
cd "$(dirname "$0")/../.."
if [ "$(cat research/best_a10_name.txt)" != f24a10 ]; then echo "F25: f24a10 not kept -> no A10 confirmation"; exit 0; fi
research/tools/f24_measure.sh a10 f6a10_norest f24a10 2000
