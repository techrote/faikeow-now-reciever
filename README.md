# faikeow-now-reciever

Companion ESP-NOW-to-USB HID receiver for the TiltMouse project, targeting clone "Pico W" boards that contain an **RP2040 plus an ESP8266/ESP8285-class Wi-Fi coprocessor** instead of the genuine Pico W CYW43439.

The intended v0.1 data path is:

```text
Waveshare ESP32-S3-Matrix / TiltMouse
        |
     ESP-NOW
        v
ESP8266/ESP8285 radio front-end
        |
 framed board-internal link
        v
RP2040 receiver core
        |
   TinyUSB mouse HID
        v
       PC
```

The host computer does not need Bluetooth, ESP-NOW support, a Wi-Fi connection, or a custom driver. It sees an ordinary USB mouse.

## Why this exists

The target boards were sold as Pico W-compatible hardware but use an ESP8266/ESP8285-class radio instead of the CYW43439 fitted to a genuine Raspberry Pi Pico W. That makes them unsuitable for the separate Bluetooth-HCI `picoWutooth` programme, but potentially useful as ESP-NOW receiver dongles.

Public work on this clone class demonstrates that the ESP8285 can be reflashed by temporarily turning the RP2040 into a USB-to-serial bridge. The exact chip, UART pins, boot/reset wiring and stable baud rate on the user's boards are **not assumed**; FNR-002 establishes them from physical evidence before production firmware depends on them.

## Repository authority

Read these before implementation:

- [RAG.md](RAG.md) — compact authoritative project state and invariants.
- [AGENTS.md](AGENTS.md) — issue execution and evidence rules.
- [docs/00-PROGRAMME.md](docs/00-PROGRAMME.md) — work packages, dependencies and completion definition.
- [docs/01-ARCHITECTURE.md](docs/01-ARCHITECTURE.md) — MCU responsibilities and data flow.
- [docs/02-PROTOCOLS.md](docs/02-PROTOCOLS.md) — wireless and inter-MCU protocol contracts.
- [docs/03-HARDWARE.md](docs/03-HARDWARE.md) — clone-board assumptions, discovery and flashing constraints.
- [docs/04-VALIDATION.md](docs/04-VALIDATION.md) — automated and physical evidence requirements.
- [docs/05-BUILDING.md](docs/05-BUILDING.md) — intended dual-toolchain build structure.
- [docs/06-REFERENCES.md](docs/06-REFERENCES.md) — upstream specifications and useful prior art.
- [docs/07-EXECUTION-PROTOCOL.md](docs/07-EXECUTION-PROTOCOL.md) — branch/PR/merge/handoff procedure.

## Programme status

This repository begins as a planning/bootstrap repository. Implementation is tracked through **FNR-###** GitHub issues.

The first engineering sequence is:

```text
FNR-001 foundation / CI
   |
   +--> FNR-002 physical clone-board characterization
   |
   +--> FNR-003 RP2040 USB HID endpoint
   |
   +--> FNR-004 ESP8266 radio firmware foundation
             |
             +---- waits for the TiltMouse ESP-NOW packet contract
                         |
                         v
                  FNR-005 inter-MCU link
                         |
                         v
                  FNR-006 integrated receiver
                         |
                         v
                  FNR-007 provisioning/security
                         |
                         v
                  FNR-008 physical acceptance
                         |
                         v
                  FNR-009 v0.1 release
```

FNR-002, FNR-003 and the hardware-independent parts of FNR-004 may proceed in parallel after FNR-001.

GitHub programme map:

- [#1 — v0.1 programme tracker](https://github.com/techrote/faikeow-now-reciever/issues/1)
- [#2 — FNR-001 foundation / CI](https://github.com/techrote/faikeow-now-reciever/issues/2)
- [#3 — FNR-002 hardware characterization](https://github.com/techrote/faikeow-now-reciever/issues/3)
- [#4 — FNR-003 RP2040 USB HID](https://github.com/techrote/faikeow-now-reciever/issues/4)
- [#5 — FNR-004 ESP-NOW radio receiver](https://github.com/techrote/faikeow-now-reciever/issues/5)
- [#6 — FNR-005 inter-MCU transport](https://github.com/techrote/faikeow-now-reciever/issues/6)
- [#7 — FNR-006 integrated receiver](https://github.com/techrote/faikeow-now-reciever/issues/7)
- [#8 — FNR-007 provisioning/recovery](https://github.com/techrote/faikeow-now-reciever/issues/8)
- [#9 — FNR-008 physical acceptance](https://github.com/techrote/faikeow-now-reciever/issues/9)
- [#10 — FNR-009 v0.1 release](https://github.com/techrote/faikeow-now-reciever/issues/10)

## Upstream contract

The transmitter is maintained in:

- `techrote/ESP32-QMI8658C-TiltMouse`

Its `docs/WIRELESS.md` is the upstream authority for the wireless semantics. In particular:

- ESP-NOW is the v0.1 wireless transport;
- packets remain within the ESP-NOW v1 **250-byte** interoperability ceiling for ESP8266-class receivers;
- movement is latest-first rather than retransmitted as stale backlog;
- each packet carries complete current button state;
- sequence numbers are used for ordering/diagnostics;
- receiver link timeout must force all buttons released.

The exact byte layout is owned by TiltMouse **TM-005B / #16**. Receiver implementation must sync to that contract rather than inventing a competing wire format.

## Design split

### ESP8266/ESP8285

Keep the radio firmware narrow:

- initialize station-mode radio without infrastructure IP networking;
- initialize ESP-NOW on an explicit channel;
- accept packets only from the configured transmitter/peer;
- copy newest received packet into bounded state;
- forward it to the RP2040 over the discovered board-internal link;
- expose small diagnostics such as source MAC, receive count and drop count;
- do not own USB HID.

### RP2040

The RP2040 is the safety/state authority:

- parse and validate the inter-MCU framing;
- validate the TiltMouse wireless packet contract;
- perform wrap-safe sequence ordering;
- reject duplicate/stale/out-of-order reports;
- implement link timeout;
- release all buttons on timeout/fault;
- discard stale cursor movement rather than replay it;
- generate the standard TinyUSB relative mouse HID report;
- own USB lifecycle/replug state.

## Explicit non-goals for v0.1

- Bluetooth or Bluetooth-HCI.
- General-purpose Wi-Fi modem/AT functionality.
- Infrastructure Wi-Fi, TCP/IP, sockets or web UI.
- Keyboard emulation.
- Mouse wheel, middle button or extra HID functions unless TiltMouse v0.1 changes its logical report contract.
- Updating the transmitter repository from this receiver repository.
- Assuming the clone board exactly matches public ESP8285 examples without physical verification.

## Name

The repository spelling `faikeow-now-reciever` is retained as the project/repository name.
