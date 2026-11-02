#!/usr/bin/env bash
# Reproduce the simulator build used by research/ (sts_lightspeed + leak-free agent).
set -euo pipefail
SIM=/home/user/sts_lightspeed
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ ! -d "$SIM" ]; then
  git clone --recurse-submodules https://github.com/gamerpuppy/sts_lightspeed.git "$SIM"
  (cd "$SIM" && git checkout "$(cat "$HERE/patches/sts_lightspeed.commit")" && git apply "$HERE/patches/sts_lightspeed.patch")
fi
pip install -q "pybind11==2.13.6" "numpy==2.5.3" "huggingface_hub==2.4.0"
mkdir -p "$SIM/build" && cd "$SIM/build"
cmake .. -DCMAKE_BUILD_TYPE=Release -Dpybind11_DIR="$(python3 -c 'import pybind11;print(pybind11.get_cmake_dir())')"
make -j"$(nproc)" fair_eval
