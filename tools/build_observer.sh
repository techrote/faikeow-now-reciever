#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
D=${FNR_DEPS:-"$ROOT/.deps"}
python3 "$ROOT/tools/check_deps.py" rp2040
export PATH="$D/arm/bin:$PATH"
export PICO_SDK_PATH="$D/pico-sdk"
export LC_ALL=C TZ=UTC SOURCE_DATE_EPOCH=1791068033
unset PICO_TINYUSB_PATH PICO_MBEDTLS_PATH PICO_SDK_FETCH_FROM_GIT
# build_rp2040.sh must have installed the locked picotool CMake package first.
cmake -S "$ROOT/diagnostics/rp2040-observer" -B "$ROOT/build/observer" -G Ninja \
 -DPICO_TOOLCHAIN_PATH="$D/arm" -Dpicotool_DIR="$D/picotool-install/picotool" \
 -DPICO_BOARD=pico -DPICO_PLATFORM=rp2040 -DPICO_NO_FLASH=1 \
 -DCMAKE_BUILD_TYPE=Release -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF
cmake --build "$ROOT/build/observer" --parallel 2
python3 "$ROOT/tools/hardware/check_ram_uf2.py" "$ROOT/build/observer/fnr_observer.uf2"
