#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
D=${FNR_DEPS:-"$ROOT/.deps"}
python3 "$ROOT/tools/check_deps.py" rp2040
export PATH="$D/arm/bin:$PATH"
export LC_ALL=C TZ=UTC SOURCE_DATE_EPOCH=1791068033
unset PICO_TINYUSB_PATH PICO_MBEDTLS_PATH PICO_SDK_FETCH_FROM_GIT
cmake -S "$D/picotool" -B "$ROOT/build/picotool-host" -G Ninja \
 -DPICO_SDK_PATH="$D/pico-sdk" -DPICOTOOL_NO_LIBUSB=ON \
 -DPICOTOOL_FLAT_INSTALL=ON -DUSE_PRECOMPILED=ON -DGIT_SUBMODULE=OFF \
 -DPICO_MBEDTLS_PATH="$D/disabled-mbedtls" \
 -DCMAKE_INSTALL_PREFIX="$D/picotool-install"
cmake --build "$ROOT/build/picotool-host" --parallel 2
cmake --install "$ROOT/build/picotool-host"
cmake -S "$ROOT/firmware/rp2040" -B "$ROOT/build/rp2040" -G Ninja \
 -DPICO_SDK_PATH="$D/pico-sdk" -DPICO_TOOLCHAIN_PATH="$D/arm" \
 -Dpicotool_DIR="$D/picotool-install/picotool" \
 -DPICO_BOARD=pico -DPICO_PLATFORM=rp2040 -DCMAKE_BUILD_TYPE=Release \
 -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF -DPICO_DEBUG_INFO_IN_RELEASE=OFF
cmake --build "$ROOT/build/rp2040" --parallel 2
