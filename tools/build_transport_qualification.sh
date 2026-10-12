#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
D=${FNR_DEPS:-"$ROOT/.deps"}
: "${FNR_SOURCE_ID:?Exact source commit required}"
case "${1:-}" in
  rp2040)
    python3 "$ROOT/tools/check_deps.py" rp2040
    export PATH="$D/arm/bin:$PATH" PICO_SDK_PATH="$D/pico-sdk"
    cmake -S "$ROOT/diagnostics/transport-qualification/rp2040" -B "$ROOT/build/transport-observer" -G Ninja \
      -DPICO_BOARD=pico -DPICO_PLATFORM=rp2040 -DPICO_NO_FLASH=1 \
      -DPICO_TOOLCHAIN_PATH="$D/arm" -Dpicotool_DIR="$D/picotool-install/picotool" \
      -DFNR_SOURCE_ID="$FNR_SOURCE_ID" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$ROOT/build/transport-observer" --parallel 2
    python3 "$ROOT/tools/hardware/check_ram_uf2.py" "$ROOT/build/transport-observer/fnr_transport_observer.uf2"
    ;;
  esp)
    python3 "$ROOT/tools/check_deps.py" esp
    export LC_ALL=C TZ=UTC SOURCE_DATE_EPOCH=1791068033
    mkdir -p "$ROOT/build/empty-config"
    Q="$ROOT/diagnostics/transport-qualification/esp"
    make -f "$D/makeEspArduino/makeEspArduino.mk" \
      ESP_ROOT="$D/esp8266" COMP_PATH="$D/xtensa" \
      PYTHON3_PATH="$(dirname "$(command -v python3)")" \
      CONFIG_ROOT="$ROOT/build/empty-config" PROJ_CONF=/dev/null \
      ARDUINO_LIBS=ESP8266WiFi BOARD=esp8285 CHIP=esp8266 FLASH_DEF=1M \
      FLASH_MODE=dout FLASH_SPEED=40 F_CPU=80000000L LWIP_VARIANT=lm2f \
      SKETCH="$Q/esp.ino" \
      LIBS="$Q/shared_datagram.c $Q/shared_ingress.c $Q/shared_transport.c $Q/shared_tx.c" \
      USER_INC_DIRS="$ROOT/diagnostics/transport-qualification $ROOT/shared/include $ROOT/shared/src $ROOT/firmware/esp8266 $D/esp8266/libraries/ESP8266WiFi/src" \
      BUILD_EXTRA_FLAGS="-DFNR_SOURCE_ID=$FNR_SOURCE_ID" \
      MK_FS_PATH="$D/mkspiffs/mkspiffs" BUILD_DIR="$ROOT/build/transport-injector" \
      MAIN_NAME=fnr_transport_injector BUILD_DATE=2026-10-03 BUILD_TIME=22:53:53 \
      SRC_GIT_VERSION="$FNR_SOURCE_ID" ESP_ARDUINO_VERSION=3.1.2 \
      USE_CCACHE=0 BUILD_THREADS=2 VERBOSE=1 all
    ;;
  *) echo 'usage: build_transport_qualification.sh rp2040|esp' >&2; exit 2 ;;
esac
