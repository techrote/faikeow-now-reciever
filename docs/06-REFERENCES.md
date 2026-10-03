# References

## faikeow platform authority

The generic ESP-NOW receiver envelope and HID profile contracts are owned by this repository:

- `RAG.md`
- `docs/01-ARCHITECTURE.md`
- `docs/02-PROTOCOLS.md`
- `docs/08-HID-PROFILES.md`
- FNR-006 / issue #7

Senders integrate by implementing a supported platform/profile contract.

## TiltMouse reference sender

- Repository:
  https://github.com/techrote/ESP32-QMI8658C-TiltMouse
- Wireless authority:
  https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/main/docs/WIRELESS.md
- ESP-NOW transmitter task:
  https://github.com/techrote/ESP32-QMI8658C-TiltMouse/issues/16

TiltMouse is the first reference sender for the faikeow `relative_mouse` profile. Its transmitter must reconcile the generic faikeow platform/profile contract rather than define a private receiver wire format.

## ESP-NOW

- Espressif ESP-NOW FAQ / version interoperability:
  https://docs.espressif.com/projects/esp-faq/en/latest/application-solution/esp-now.html
- ESP8266 RTOS SDK ESP-NOW API header:
  https://github.com/espressif/ESP8266_RTOS_SDK/blob/master/components/esp8266/include/esp_now.h
- ESP8266 Arduino SDK ESP-NOW API header:
  https://github.com/esp8266/Arduino/blob/master/tools/sdk/include/espnow.h

Keep the generic application envelope plus profile payload at or below the ESP-NOW v1 250-byte interoperability ceiling for the accepted ESP8266-class receiver.

## Clone Pico-W / ESP8285 prior art

- JiriBilek/RP2040_PicoW_ESP8285_Library:
  https://github.com/JiriBilek/RP2040_PicoW_ESP8285_Library
- mocacinno/rp2040_with_esp8285:
  https://github.com/mocacinno/rp2040_with_esp8285

These demonstrate feasibility of UART communication and RP2040-assisted radio flashing on similar boards. They are not the accepted board contract.

## RP2040 / USB / PIO

- Raspberry Pi pico-examples:
  https://github.com/raspberrypi/pico-examples
- TinyUSB HID examples:
  https://github.com/hathach/tinyusb/tree/master/examples/device/hid_composite
- Raspberry Pi Pico SDK:
  https://github.com/raspberrypi/pico-sdk
- RP2040 datasheet / PIO documentation:
  https://www.raspberrypi.com/documentation/microcontrollers/rp2040.html

Use exact versions pinned by FNR-001 rather than master-only APIs.

## Flash tooling

- Espressif esptool:
  https://github.com/espressif/esptool

FNR-002 records the exact version/commands proven on the target board.

## Evidence rule

External references are engineering inputs, not substitutes for target-board evidence. Where prior art conflicts with physical observations, the measured board contract wins.
