#!/usr/bin/env bash
# Phase C v3 pipeline: train the interventional deck-value model on DATA_DIR, rebuild, prove the build
# still reproduces the best (model off), leak-test with the model on, then run the decision experiments.
# usage: bf_pipeline.sh DATA_DIR TAG   (TAG names the experiments: bfpick_<TAG> ...)
set -uo pipefail
cd "$(dirname "$0")/../.."
DATA=${1:?data dir}; TAG=${2:?tag}
python3 research/vf/train_bf.py "$DATA" --out research/cpp/bf_weights.inc 2>&1 | tail -40 || { echo "training failed"; exit 1; }
cp research/cpp/bf_weights.inc "research/cpp/bf_weights_${TAG}.inc.bak"
( cd /home/user/sts_lightspeed/build && make fair_eval 2>make.err >/dev/null ) || { tail -20 /home/user/sts_lightspeed/build/make.err; exit 1; }
research/tools/repro_check.sh "$(cat research/best_name.txt)" 16 || exit 1
if grep -q "BF_TRAINED = 0" research/cpp/bf_weights.inc; then echo "model untrained (gate failed): no experiments"; exit 0; fi
/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --param end_turn_w=0.05 --param prior_mlp=1 --param bf_pick=1 --param bf_upgrade=1 --param bf_remove=1 || exit 1
R=research/exp.sh
for T in ${TRACKS:-a0}; do   # TRACKS="a0 a10 a20" tests the model on several ladders
  PFX=""; [ "$T" != a0 ] && PFX="${T}_"
  TRACK=$T $R "${PFX}bfpick_$TAG" "interventional deck value: card rewards = argmax predicted fight-value gain (skip unless > margin 0)" bf_pick=1
  TRACK=$T $R "${PFX}bfpick_${TAG}_m02" "interventional deck value: card rewards, margin 0.02" bf_pick=1 bf_margin=0.02
  TRACK=$T $R "${PFX}bfupg_$TAG" "interventional deck value: rest upgrades = argmax predicted fight value" bf_upgrade=1
  TRACK=$T $R "${PFX}bfrem_$TAG" "interventional deck value: removals = argmax predicted fight value" bf_remove=1
done
echo "BF PIPELINE $TAG DONE"
