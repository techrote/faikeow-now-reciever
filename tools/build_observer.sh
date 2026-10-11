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
for item in "rp2040-candidate-bridge:candidate-bridge:fnr_candidate_bridge" "rp2040-link:rp-link:fnr_link_diag"; do
  IFS=: read -r source build target <<< "$item"
  cmake -S "$ROOT/diagnostics/$source" -B "$ROOT/build/$build" -G Ninja \
    -DPICO_TOOLCHAIN_PATH="$D/arm" -Dpicotool_DIR="$D/picotool-install/picotool" \
    -DPICO_BOARD=pico -DPICO_PLATFORM=rp2040 -DPICO_NO_FLASH=1 \
    -DFNR_ALLOW_CANDIDATE_UART=1 -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF
  cmake --build "$ROOT/build/$build" --parallel 2
  python3 "$ROOT/tools/hardware/check_ram_uf2.py" "$ROOT/build/$build/$target.uf2"
done
# These explicit flash-resident bridges are for already-preserved, characterized
# boards. Labels avoid duplicate CDC instances; topology remains authoritative.
for label in A B; do
  cmake -S "$ROOT/diagnostics/rp2040-candidate-bridge" -B "$ROOT/build/bridge-flash-$label" -G Ninja \
    -DPICO_TOOLCHAIN_PATH="$D/arm" -Dpicotool_DIR="$D/picotool-install/picotool" \
    -DPICO_BOARD=pico -DPICO_PLATFORM=rp2040 -DPICO_NO_FLASH=0 \
    -DFNR_BRIDGE_FLASH_RECOVERY_TEST=1 -DFNR_DIAG_BOARD_LABEL="$label" \
    -DFNR_ALLOW_CANDIDATE_UART=1 -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF
  cmake --build "$ROOT/build/bridge-flash-$label" --parallel 2
  python3 - "$ROOT/build/bridge-flash-$label/fnr_candidate_bridge.uf2" "$ROOT" <<'PY'
import pathlib, sys
sys.path.insert(0, str(pathlib.Path(sys.argv[2]) / 'tools/hardware'))
from rp2040_flash import flash_blocks
assert flash_blocks(pathlib.Path(sys.argv[1]).read_bytes(), 2097152)
print('PASS: compile-only flash bridge addresses fit the 2 MiB surrogate')
PY
done
