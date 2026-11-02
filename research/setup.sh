#!/usr/bin/env bash
# Reproduce the simulator build used by research/ (sts_lightspeed + leak-free agent).
# STS_SIM_DIR: where to clone and build the simulator (default: the cloud path).
# STS_TARGETS: make targets to build (default: fair_eval).
set -euo pipefail
SIM="${STS_SIM_DIR:-/home/user/sts_lightspeed}"
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ ! -d "$SIM" ]; then
  git clone --recurse-submodules https://github.com/gamerpuppy/sts_lightspeed.git "$SIM"
  (cd "$SIM" && git checkout "$(cat "$HERE/patches/sts_lightspeed.commit")" && git apply "$HERE/patches/sts_lightspeed.patch")
fi
if [ -n "${STS_SIM_DIR:-}" ]; then
  # Local machines keep the build dependency out of the user's Python environment.
  python3 -m venv "$SIM/.pybind-venv"
  PY="$SIM/.pybind-venv/bin/python"
  "$PY" -m pip install -q "pybind11==2.13.6"
else
  PY=python3
  pip install -q "pybind11==2.13.6"
fi
mkdir -p "$SIM/build" && cd "$SIM/build"
# CMAKE_POLICY_VERSION_MINIMUM: the json submodule declares a CMake version that CMake >= 4 refuses.
cmake .. -DCMAKE_BUILD_TYPE=Release -DSTS_RESEARCH_DIR="$HERE" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -Dpybind11_DIR="$("$PY" -c 'import pybind11;print(pybind11.get_cmake_dir())')"
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
make -j"$JOBS" ${STS_TARGETS:-fair_eval}
