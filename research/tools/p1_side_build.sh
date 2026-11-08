#!/usr/bin/env bash
# Build a side fair_eval on the fidelity fork with a candidate move prior, without touching research/cpp/pfm_weights.inc
# (consistency rule: the compiled prior is part of the agent). usage: p1_side_build.sh TAG PRIOR_INC
#   -> /home/user/p1side/TAG/research (copy of research/{cpp,eval,vf} with the candidate prior) and
#      /home/user/sts_lightspeed_fid/build_TAG/fair_eval
set -euo pipefail
TAG=$1; INC=$(realpath "$2"); SHA=$(sha1sum "$INC" | cut -c1-12)   # absolute: the script cds into the build dir
R=/home/user/p1side/$TAG/research; F=/home/user/sts_lightspeed_fid
rm -rf "$R"; mkdir -p "$R"
for d in cpp eval; do cp -r /home/user/sts/research/$d "$R/"; done
mkdir -p "$R/vf" && find /home/user/sts/research/vf -maxdepth 1 -type f -exec cp {} "$R/vf/" \;   # code only: never research/vf/data (GBs)
cp "$INC" "$R/cpp/pfm_weights.inc"
mkdir -p "$F/build_$TAG" && cd "$F/build_$TAG"
cmake .. -DCMAKE_BUILD_TYPE=Release -DSTS_RESEARCH_DIR="$R" -Dpybind11_DIR="$(python3 -c 'import pybind11;print(pybind11.get_cmake_dir())')" > cmake.log 2>&1
make -j"$(nproc)" fair_eval 2>&1 | grep -E "error|Built target fair_eval"
echo "SIDE BUILD $TAG: $F/build_$TAG/fair_eval (prior $SHA)"
