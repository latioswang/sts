#!/usr/bin/env bash
# Reproduce the simulator build used by research/ (sts_lightspeed + leak-free agent).
# STS_SIM selects the simulator:
#   fidelity (default since 2026-11-06, owner decision): latioswang/sts_lightspeed (real-game-matched fork) pinned at
#            patches/sts_lightspeed_fidelity.commit; the fork already carries the research patch; fair_eval is built from this
#            research/ dir
#   legacy   (the loop's simulator until 2026-11-06): upstream gamerpuppy/sts_lightspeed at patches/sts_lightspeed.commit + the
#            research patch
# Local-machine switches (used by the real-game bridge, research/realgame/README.md); research runs leave them unset:
#   STS_SIM_DIR    where to clone and build the simulator (default: the cloud path /home/user/sts_lightspeed)
#   STS_SIM_BRANCH clone this branch of the fork instead of the pinned commit
#   STS_TARGETS    make targets to build (default: fair_eval; the bridge needs "fair_eval sts_bridge")
set -euo pipefail
SIM="${STS_SIM_DIR:-/home/user/sts_lightspeed}"
HERE="$(cd "$(dirname "$0")" && pwd)"
STS_SIM=${STS_SIM:-fidelity}   # loop switched to the fork 2026-11-06 (owner decision); results tagged via research/eval/sim.txt
if [ ! -d "$SIM" ]; then
  if [ -n "${STS_SIM_BRANCH:-}" ]; then
    git clone --recurse-submodules -b "$STS_SIM_BRANCH" https://github.com/latioswang/sts_lightspeed.git "$SIM"
  elif [ "$STS_SIM" = fidelity ]; then
    git clone --recurse-submodules https://github.com/latioswang/sts_lightspeed.git "$SIM"
    (cd "$SIM" && git checkout "$(cat "$HERE/patches/sts_lightspeed_fidelity.commit")" && git submodule update --init --recursive)
  else
    git clone --recurse-submodules https://github.com/gamerpuppy/sts_lightspeed.git "$SIM"
    (cd "$SIM" && git checkout "$(cat "$HERE/patches/sts_lightspeed.commit")" && git apply "$HERE/patches/sts_lightspeed.patch")
  fi
fi
if [ -n "${STS_SIM_DIR:-}" ]; then
  # Local machines keep the build dependency out of the user's Python environment (and skip the trainers' packages).
  python3 -m venv "$SIM/.pybind-venv"
  PY="$SIM/.pybind-venv/bin/python"
  "$PY" -m pip install -q "pybind11==2.13.6"
else
  PY=python3
  pip install -q "pybind11==2.13.6" "numpy==2.5.3" "huggingface_hub==2.4.0"
  pip install -q "torch==2.15.0" --index-url https://download.pytorch.org/whl/cpu   # trainers (train_*.py); CPU wheel
fi
mkdir -p "$SIM/build" && cd "$SIM/build"
EXTRA=(); { [ "$STS_SIM" = fidelity ] || [ -n "${STS_SIM_BRANCH:-}" ]; } && EXTRA=(-DSTS_RESEARCH_DIR="$HERE")
# CMAKE_POLICY_VERSION_MINIMUM: the json submodule declares a CMake version that CMake >= 4 refuses.
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -Dpybind11_DIR="$("$PY" -c 'import pybind11;print(pybind11.get_cmake_dir())')" "${EXTRA[@]}"
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
make -j"$JOBS" ${STS_TARGETS:-fair_eval}
echo "built ${STS_TARGETS:-fair_eval} with STS_SIM=$STS_SIM ($(cd "$SIM" && git log --oneline -1 | cut -c1-60))"
