# faikeow-now-reciever

A flexible **ESP-NOW -> USB HID receiver platform** for clone "Pico W" boards built around an **RP2040 plus ESP8266/ESP8285-class Wi-Fi coprocessor**.

The project turns the unusual clone hardware into a reusable wireless peripheral receiver rather than a single-purpose mouse dongle.

```text
ESP-NOW sender
      |
      v
ESP8266/ESP8285
  radio front-end
      |
 framed board-internal link
      v
RP2040 receiver core
  |
  +-- profile dispatch
  |    +-- relative mouse       <- v0.1 / TiltMouse reference profile
  |    +-- keyboard             <- future
  |    +-- gamepad              <- future
  |    +-- consumer control     <- future
  |    +-- custom HID           <- future
  |
  +-- TinyUSB HID
      |
      v
     host
```

The host does not need ESP-NOW support, Wi-Fi configuration, Bluetooth or a custom driver. It sees the USB HID interface exposed by the selected receiver profile.

## Why this hardware is interesting

These boards were sold as Pico W-compatible hardware but replace the CYW43439 with ESP8266/ESP8285-class Wi-Fi silicon. That makes them unsuitable for software expecting the genuine Pico W radio, but potentially useful as dedicated wireless bridges:

- ESP8266/ESP8285 handles ESP-NOW RF;
- RP2040 handles USB, deterministic state and profile dispatch;
- RP2040 PIO remains available for future unusual peripheral/protocol adapters.

PIO-backed outputs are **not a v0.1 requirement**, but the architecture deliberately avoids making future profile backends impossible.

FNR-002 independently identified both units as RP2040 B2 + ESP8285N08 and proved
the onboard UART0 GP0/GP1 route at 115200 8N1, reversible flashing and recovery.
ESP8285 has Wi-Fi and ESP-NOW, with no Bluetooth controller. RP-controlled
radio reset/download wiring and complete header routing remain unknown.

The authoritative hardware reference now lives in
[techrote/picoWcloneESP8285N08](https://github.com/techrote/picoWcloneESP8285N08).
Use its [programming reference](https://github.com/techrote/picoWcloneESP8285N08/blob/main/docs/DEVBOARD-PROGRAMMING-REFERENCE.md),
[operating runbook](https://github.com/techrote/picoWcloneESP8285N08/blob/main/docs/DEVBOARD-OPERATING-RUNBOOK.md)
and [board contract](https://github.com/techrote/picoWcloneESP8285N08/blob/main/docs/BOARD-CONTRACT.md)
for hardware facts and procedures. [Hardware integration](docs/03-HARDWARE.md)
records the pinned reference baseline. This repository owns receiver firmware,
protocol/profile contracts and application acceptance; its local FNR-002
contract/evidence are retained as historical characterization records.

## Repository authority

Read before implementation:

- [RAG.md](RAG.md)
- [AGENTS.md](AGENTS.md)
- [docs/00-PROGRAMME.md](docs/00-PROGRAMME.md)
- [docs/01-ARCHITECTURE.md](docs/01-ARCHITECTURE.md)
- [docs/02-PROTOCOLS.md](docs/02-PROTOCOLS.md)
- [docs/03-HARDWARE.md](docs/03-HARDWARE.md)
- [docs/04-VALIDATION.md](docs/04-VALIDATION.md)
- [docs/05-BUILDING.md](docs/05-BUILDING.md)
- [docs/06-REFERENCES.md](docs/06-REFERENCES.md)
- [docs/07-EXECUTION-PROTOCOL.md](docs/07-EXECUTION-PROTOCOL.md)
- [docs/08-HID-PROFILES.md](docs/08-HID-PROFILES.md)

## Core architectural rule

The receiver core must not contain TiltMouse-specific field names or assumptions.

The platform owns:

- ESP-NOW ingress;
- generic receiver envelope;
- peer/session/order/freshness handling;
- bounded inter-MCU transport;
- profile dispatch;
- USB HID framework;
- provisioning/recovery.

Profiles own:

- payload semantics;
- HID descriptor/report behavior;
- profile-specific loss/timeout policy.

## v0.1 profile

v0.1 implements one production profile:

- **relative mouse** — left/right buttons plus relative X/Y.

`techrote/ESP32-QMI8658C-TiltMouse` is the first reference sender and physical interoperability target, not the product definition of this repository.

## Developer builds (FNR-003, FNR-004 and FNR-005 software)

The RP2040 build contains the FNR-003 generic TinyUSB HID framework and the
`relative_mouse` backend. The ESP8266 build contains the FNR-004 profile-neutral
ESP-NOW ingress and **defaults unprovisioned**. FNR-005 now connects the radio's
foreground handoff to a bounded COBS/CRC16 UART serializer and an RP2040
interrupt-fed parser. The link uses the already accepted FNR-002 pins and
115200 8N1 configuration. Transport consumers see owned generic frames and
failure/restart events; no transport data feeds HID output yet.

These are software-qualified targets, not a complete or physically accepted
dongle. FNR-002's clone-board pinout/recovery/UART evidence is accepted;
FNR-005's [production transport campaign](evidence/FNR-005/README.md) passes on
one representative board; this does not establish the integrated dongle.
FNR-006 owns
receiver/envelope/profile dispatch, and
FNR-008 owns physical USB/RF/reference-profile acceptance.

A canonical Linux x86_64 environment (including WSL2) can run:

```sh
python3 tools/acquire_deps.py --lane rp2040
bash tools/build_rp2040.sh
python3 tools/acquire_deps.py --lane esp
FNR_SOURCE_ID="$(git rev-parse HEAD)" bash tools/build_esp.sh
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=undefined -fno-sanitize-recover=all" \
  -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-sanitize-recover=all"
cmake --build build/native --parallel 2
ctest --test-dir build/native --output-on-failure
```

Current firmware outputs are `build/rp2040/fnr_rp2040_hid.uf2`/`.elf` and
`build/esp8266/fnr_esp8266_ingress.bin`/`.elf`. See
[exact commands and limitations](docs/05-BUILDING.md),
[HID profile architecture](docs/08-HID-PROFILES.md),
[FNR-003 software evidence](docs/FNR-003-IMPLEMENTATION.md),
[FNR-004 radio ingress](docs/FNR-004-RADIO-INGRESS.md),
[FNR-005 protocol and ownership](docs/FNR-005-TRANSPORT.md),
[FNR-005 software prepass](docs/FNR-005-SOFTWARE-PREPASS.md), and
[the production physical campaign and checklist](docs/FNR-005-HARDWARE-HANDOFF.md).
CI runs all native tests plus both locked target cross-builds and publishes
checksummed manifests with `physical_acceptance: false`.

## Programme

- [#1 — v0.1 programme tracker](https://github.com/techrote/faikeow-now-reciever/issues/1)
- [#2 — FNR-001 foundation / CI](https://github.com/techrote/faikeow-now-reciever/issues/2)
- [#3 — FNR-002 hardware characterization](https://github.com/techrote/faikeow-now-reciever/issues/3)
- [#4 — FNR-003 generic TinyUSB HID framework](https://github.com/techrote/faikeow-now-reciever/issues/4)
- [#5 — FNR-004 generic ESP-NOW ingress](https://github.com/techrote/faikeow-now-reciever/issues/5)
- [#6 — FNR-005 inter-MCU transport](https://github.com/techrote/faikeow-now-reciever/issues/6)
- [#7 — FNR-006 receiver core + relative-mouse profile](https://github.com/techrote/faikeow-now-reciever/issues/7)
- [#8 — FNR-007 generic provisioning/recovery](https://github.com/techrote/faikeow-now-reciever/issues/8)
- [#9 — FNR-008 platform + reference-profile physical acceptance](https://github.com/techrote/faikeow-now-reciever/issues/9)
- [#10 — FNR-009 v0.1 platform release](https://github.com/techrote/faikeow-now-reciever/issues/10)
- [#11 — FNR-010 post-v0.1 HID/PIO expansion](https://github.com/techrote/faikeow-now-reciever/issues/11) — non-blocking

## Explicit v0.1 non-goals

- Bluetooth/Bluetooth-HCI.
- Infrastructure Wi-Fi, TCP/IP or web UI.
- General AT-modem compatibility.
- Multiple simultaneously active HID profiles.
- Keyboard/gamepad/consumer-control production profiles.
- PIO-backed peripheral outputs.

Those are extension points, not requirements for first release.

## Name

The repository spelling `faikeow-now-reciever` is retained.
