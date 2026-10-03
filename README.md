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

Public work on similar clone boards shows that the radio can often be reflashed by temporarily turning the RP2040 into a USB-to-serial bridge. Exact chip identity, pins, boot/reset wiring and stable link rate on the target boards remain physical FNR-002 evidence rather than assumptions.

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
