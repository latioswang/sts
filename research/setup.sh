#!/usr/bin/env bash
# Reproduce the simulator build used by research/ (sts_lightspeed + leak-free agent).
# STS_SIM selects the simulator:
#   legacy   (default until the switch): upstream gamerpuppy/sts_lightspeed at patches/sts_lightspeed.commit + the research patch
#   fidelity : latioswang/sts_lightspeed (real-game-matched fork) pinned at patches/sts_lightspeed_fidelity.commit; the fork
#              already carries the research patch, so nothing is applied on top; fair_eval is built from this research/ dir
set -euo pipefail
SIM=/home/user/sts_lightspeed
HERE="$(cd "$(dirname "$0")" && pwd)"
STS_SIM=${STS_SIM:-legacy}
if [ ! -d "$SIM" ]; then
  if [ "$STS_SIM" = fidelity ]; then
    git clone --recurse-submodules https://github.com/latioswang/sts_lightspeed.git "$SIM"
    (cd "$SIM" && git checkout "$(cat "$HERE/patches/sts_lightspeed_fidelity.commit")" && git submodule update --init --recursive)
  else
    git clone --recurse-submodules https://github.com/gamerpuppy/sts_lightspeed.git "$SIM"
    (cd "$SIM" && git checkout "$(cat "$HERE/patches/sts_lightspeed.commit")" && git apply "$HERE/patches/sts_lightspeed.patch")
  fi
fi
pip install -q "pybind11==2.13.6" "numpy==2.5.3" "huggingface_hub==2.4.0"
pip install -q "torch==2.15.0" --index-url https://download.pytorch.org/whl/cpu   # trainers (train_*.py); CPU wheel
mkdir -p "$SIM/build" && cd "$SIM/build"
EXTRA=(); [ "$STS_SIM" = fidelity ] && EXTRA=(-DSTS_RESEARCH_DIR="$HERE")
cmake .. -DCMAKE_BUILD_TYPE=Release -Dpybind11_DIR="$(python3 -c 'import pybind11;print(pybind11.get_cmake_dir())')" "${EXTRA[@]}"
make -j"$(nproc)" fair_eval
echo "built fair_eval with STS_SIM=$STS_SIM ($(cd "$SIM" && git log --oneline -1 | cut -c1-60))"
