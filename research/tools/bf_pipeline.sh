#!/usr/bin/env bash
# Phase C v3 pipeline: train model <id> on DATA (comma-separated dirs) into research/models/bf_g<id>.txt,
# verify the build still reproduces the best (models are runtime data, so no rebuild is needed), leak-test
# with the model on, then run the decision experiments with bf_model=<id>.
# usage: [TRAIN=0] [EXP_SET="pick rem bonus10"] [TRACKS="a0 a10"] bf_pipeline.sh DATA TAG MODEL_ID
set -uo pipefail
cd "$(dirname "$0")/../.."
DATA=${1:?data dir(s)}; TAG=${2:?tag}; ID=${3:?model id (integer)}
MODEL=research/models/bf_g$ID.txt
if [ "${TRAIN:-1}" = 1 ]; then
  python3 research/vf/train_bf.py "$DATA" --out "$MODEL" 2>&1 | tail -40 || { echo "training failed"; exit 1; }
fi
[ -s "$MODEL" ] || { echo "no model file $MODEL"; exit 1; }
research/tools/repro_check.sh "$(cat research/best_name.txt)" 16 || exit 1
if [ "$(head -c 64 "$MODEL" | awk '{print $4}')" = 0 ]; then echo "model $ID untrained (gate failed): no experiments"; exit 0; fi
/home/user/sts_lightspeed/build/fair_eval --leaktest --seed_start 100000 --count 2 --param end_turn_w=0.05 --param prior_mlp=1 --param bf_model=$ID --param bf_pick=1 --param bf_upgrade=1 --param bf_remove=1 || exit 1
R=research/exp.sh
for T in ${TRACKS:-a0}; do   # TRACKS="a0 a10 a20" tests the model on several ladders
  PFX=""; [ "$T" != a0 ] && PFX="${T}_"
  for E in ${EXP_SET:-pick pick_m02 upg rem}; do   # EXP_SET picks the decision experiments
    case "$E" in
      pick)     TRACK=$T $R "${PFX}bfpick_$TAG" "interventional deck value ($TAG): card rewards = argmax predicted fight-value gain (skip unless > margin 0)" bf_model=$ID bf_pick=1 bf_bonus=0;;
      pick_m02) TRACK=$T $R "${PFX}bfpick_${TAG}_m02" "interventional deck value ($TAG): card rewards, margin 0.02" bf_model=$ID bf_pick=1 bf_margin=0.02 bf_bonus=0;;
      upg)      TRACK=$T $R "${PFX}bfupg_$TAG" "interventional deck value ($TAG): rest upgrades = argmax predicted fight value" bf_model=$ID bf_upgrade=1;;
      rem)      TRACK=$T $R "${PFX}bfrem_$TAG" "interventional deck value ($TAG): removals = argmax predicted fight value" bf_model=$ID bf_remove=1;;
      bonus*)   W=${E#bonus}; TRACK=$T $R "${PFX}bfbonus${W}_$TAG" "card picks: KB value + $W x predicted fight-value gain (model $TAG)" bf_model=$ID bf_bonus=$W;;
    esac
  done
done
echo "BF PIPELINE $TAG DONE"
