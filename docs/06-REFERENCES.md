# References

## Upstream TiltMouse

- Repository:
  https://github.com/techrote/ESP32-QMI8658C-TiltMouse
- Wireless authority:
  https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/main/docs/WIRELESS.md
- ESP-NOW transmitter/protocol issue:
  https://github.com/techrote/ESP32-QMI8658C-TiltMouse/issues/16

The exact wireless byte contract is upstream-owned. Reconcile it live before implementing FNR-006.

## ESP-NOW

- Espressif ESP-NOW FAQ / version interoperability:
  https://docs.espressif.com/projects/esp-faq/en/latest/application-solution/esp-now.html
- ESP8266 RTOS SDK ESP-NOW API header:
  https://github.com/espressif/ESP8266_RTOS_SDK/blob/master/components/esp8266/include/esp_now.h
- ESP8266 Arduino SDK ESP-NOW API header:
  https://github.com/esp8266/Arduino/blob/master/tools/sdk/include/espnow.h

Important interoperability fact: mixed v1/v2 deployments should keep application packets at or below 250 bytes.

## Clone Pico-W / ESP8285 prior art

- JiriBilek/RP2040_PicoW_ESP8285_Library:
  https://github.com/JiriBilek/RP2040_PicoW_ESP8285_Library
- mocacinno/rp2040_with_esp8285:
  https://github.com/mocacinno/rp2040_with_esp8285

These demonstrate feasibility of UART communication and RP2040-assisted radio flashing on similar boards. They are not accepted as the user's exact board contract.

## RP2040 / USB

- Raspberry Pi pico-examples:
  https://github.com/raspberrypi/pico-examples
- TinyUSB HID composite example:
  https://github.com/hathach/tinyusb/tree/master/examples/device/hid_composite
- Raspberry Pi Pico SDK:
  https://github.com/raspberrypi/pico-sdk

Use the pinned versions selected in FNR-001 rather than copying master-only APIs.

## Flash tooling

- Espressif esptool:
  https://github.com/espressif/esptool

FNR-002 records the exact version/commands proven on the target board.

## Evidence rule

External references are engineering inputs, not substitutes for target-board evidence. Where public prior art conflicts with physical observations, update this repository's board contract from measured evidence.
