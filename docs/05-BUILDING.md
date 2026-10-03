# Building and repository layout

## Intended layout

```text
/
  README.md
  RAG.md
  AGENTS.md
  shared/
    platform/
    internal_protocol/
    profiles/
  rp2040/
    usb_hid/
    receiver_core/
    profiles/
  esp8266/
    radio_ingress/
  tests/
    native/
    fixtures/
  tools/
  docs/
```

Exact directories may evolve, but dependency direction must remain:

```text
radio_ingress -> internal transport -> generic receiver core -> profile -> HID framework
```

Target SDK headers must not leak into portable platform/profile state logic.

## RP2040 toolchain

FNR-001 pins:

- Pico SDK;
- TinyUSB revision inherited/selected;
- supported Arm GCC;
- CMake/Ninja flow.

Production output: UF2 + manifest/checksum.

## ESP radio toolchain

FNR-001 pins a reproducible ESP8266/ESP8285 toolchain with ESP-NOW receive support.

Production output: flashable radio image + manifest/checksum.

## Native tests

Native tests should build:

- generic envelope/core;
- internal transport;
- profile dispatch;
- relative-mouse profile;
- USB report helpers where portable.

No target SDK should be required.

## Build/profile selection

v0.1 may select one production HID profile at build time or through bounded configuration established by FNR-007.

The build system must not hard-code repository-wide assumptions that the only possible profile is TiltMouse.

## Secrets/configuration

Never commit real universal production keys.

Configuration may include:

- peer(s);
- channel;
- keys;
- selected/allowed profile IDs;
- compatible platform/profile versions.

## PIO

No PIO code is required for v0.1.

Keep build/layout boundaries clean enough that later PIO-backed profile/backend modules can be added without restructuring radio ingress.
