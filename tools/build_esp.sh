#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
D=${FNR_DEPS:-"$ROOT/.deps"}
: "${FNR_SOURCE_ID:?Set FNR_SOURCE_ID to the source commit plus proposed patch SHA256}"
python3 "$ROOT/tools/check_deps.py" esp
export LC_ALL=C TZ=UTC SOURCE_DATE_EPOCH=1791068033
mkdir -p "$ROOT/build/empty-config"

TARGET_BUILD_DIR=${FNR_ESP_BUILD_DIR:-"$ROOT/build/esp8266"}
PROVISIONING_SOURCE="$ROOT/firmware/esp8266/provisioning_image.cpp"
if [[ -n "${FNR_PROVISIONING_CONFIG:-}" ]]; then
  python3 "$ROOT/tools/generate_provisioning.py"     --config "$FNR_PROVISIONING_CONFIG" --out-dir "$ROOT/build/provisioning"
  PROVISIONING_SOURCE="$ROOT/build/provisioning/esp_provisioning_image.cpp"
fi

make -f "$D/makeEspArduino/makeEspArduino.mk"   ESP_ROOT="$D/esp8266" COMP_PATH="$D/xtensa"   PYTHON3_PATH="$(dirname "$(command -v python3)")"   CONFIG_ROOT="$ROOT/build/empty-config" PROJ_CONF=/dev/null   ARDUINO_LIBS=ESP8266WiFi BOARD=esp8285 CHIP=esp8266 FLASH_DEF=1M   FLASH_MODE=dout FLASH_SPEED=40 F_CPU=80000000L LWIP_VARIANT=lm2f   ARDUINO_EXTRA_DESC="$ROOT/firmware/esp8266/recipe-overrides.txt"   SKETCH="$ROOT/firmware/esp8266/main.cpp"   LIBS="$ROOT/firmware/esp8266/radio_auth_crypto.cpp $ROOT/shared/src/radio_auth.c $ROOT/firmware/esp8266/radio_adapter.cpp $ROOT/firmware/esp8266/transport_adapter.cpp $ROOT/firmware/esp8266/provisioning_adapter.cpp $PROVISIONING_SOURCE $ROOT/shared/src/datagram.c $ROOT/shared/src/radio_ingress.c $ROOT/shared/src/transport.c $ROOT/shared/src/transport_tx.c $ROOT/shared/src/provisioning.c"   USER_INC_DIRS="$ROOT/shared/include $ROOT/firmware/esp8266 $D/esp8266/libraries/ESP8266WiFi/src"   MK_FS_PATH="$D/mkspiffs/mkspiffs"   BUILD_DIR="$TARGET_BUILD_DIR" MAIN_NAME=fnr_esp8266_ingress   BUILD_DATE=2026-10-03 BUILD_TIME=22:53:53   SRC_GIT_VERSION="$FNR_SOURCE_ID" ESP_ARDUINO_VERSION=3.1.2   USE_CCACHE=0 BUILD_THREADS=2 VERBOSE=1 all
