#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
D=${FNR_DEPS:-"$ROOT/.deps"}
python3 "$ROOT/tools/check_deps.py" rp2040
export PATH="$D/arm/bin:$PATH"
export LC_ALL=C TZ=UTC SOURCE_DATE_EPOCH=1791068033
unset PICO_TINYUSB_PATH PICO_MBEDTLS_PATH PICO_SDK_FETCH_FROM_GIT

TARGET_BUILD_DIR=${FNR_RP_BUILD_DIR:-"$ROOT/build/rp2040"}
PROVISIONING_SOURCE="$ROOT/firmware/rp2040/provisioning_image.c"
if [[ -n "${FNR_PROVISIONING_CONFIG:-}" ]]; then
  python3 "$ROOT/tools/generate_provisioning.py"     --config "$FNR_PROVISIONING_CONFIG" --out-dir "$ROOT/build/provisioning"
  PROVISIONING_SOURCE="$ROOT/build/provisioning/rp_provisioning_image.c"
fi

cmake -S "$D/picotool" -B "$ROOT/build/picotool-host" -G Ninja  -DPICO_SDK_PATH="$D/pico-sdk" -DPICOTOOL_NO_LIBUSB=ON  -DPICOTOOL_FLAT_INSTALL=ON -DUSE_PRECOMPILED=ON -DGIT_SUBMODULE=OFF  -DPICO_MBEDTLS_PATH="$D/disabled-mbedtls"  -DCMAKE_INSTALL_PREFIX="$D/picotool-install"
cmake --build "$ROOT/build/picotool-host" --parallel 2
cmake --install "$ROOT/build/picotool-host"
cmake -S "$ROOT/firmware/rp2040" -B "$TARGET_BUILD_DIR" -G Ninja  -DPICO_SDK_PATH="$D/pico-sdk" -DPICO_TOOLCHAIN_PATH="$D/arm"  -Dpicotool_DIR="$D/picotool-install/picotool"  -DPICO_BOARD=pico -DPICO_PLATFORM=rp2040 -DCMAKE_BUILD_TYPE=Release  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF -DPICO_DEBUG_INFO_IN_RELEASE=OFF  -DFNR_PROVISIONING_SOURCE="$PROVISIONING_SOURCE"
cmake --build "$TARGET_BUILD_DIR" --parallel 2
