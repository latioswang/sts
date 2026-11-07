#!/usr/bin/env bash
# Phase F24 arm (pre-registered in program.md F23): the track's best with its three V weights another x1.5; leak test at the track's
# ascension, then the fresh-slice rule. usage: f23_one.sh TRACK NAME k=v [k=v ...]
set -euo pipefail
cd "$(dirname "$0")/../.."
TRACK=$1; NAME=$2; shift 2
case "$TRACK" in a0) ASC=0; PARF=research/best_params.txt;; a10) ASC=10; PARF=research/best_a10_params.txt;; a20) ASC=20; PARF=research/best_a20_params.txt;; *) exit 1;; esac
R=$(/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --asc $ASC \
    $(sed 's/^/--param /' $PARF | tr '\n' ' ') $(for x in "$@"; do echo "--param $x"; done) | tail -1)
echo "leaktest_$TRACK [$*]: $R"; case "$R" in *PASS*) ;; *) exit 1;; esac
TRACK=$TRACK research/tools/exp_fresh.sh "$NAME" "Phase F24: $TRACK best with V weights another x1.5" "$@"
