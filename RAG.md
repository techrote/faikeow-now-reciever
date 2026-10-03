# RAG — faikeow-now-reciever

This is the compact authoritative context for implementation work.

## Mission

Turn a clone "Pico W" board containing an RP2040 plus ESP8266/ESP8285-class Wi-Fi silicon into a dedicated **ESP-NOW mouse receiver USB dongle** for `techrote/ESP32-QMI8658C-TiltMouse`.

The host PC sees a standard USB HID mouse. It does not need Bluetooth, Wi-Fi configuration, ESP-NOW support or a custom driver.

## Product data path

```text
TiltMouse ESP32-S3
   |
 ESP-NOW
   v
ESP8266/ESP8285
   |
 board-internal framed link
   v
RP2040
   |
 TinyUSB relative mouse HID
   v
host PC
```

## Authority split

### ESP8266/ESP8285 owns

- station-mode radio initialization required for ESP-NOW;
- fixed/explicit RF channel;
- ESP-NOW receive callback;
- source-peer filtering;
- a bounded newest-packet handoff to the RP2040;
- radio-side counters/status;
- no USB HID behavior.

### RP2040 owns

- board-internal frame parsing and integrity checks;
- TiltMouse packet version/length validation;
- wrap-safe wireless sequence ordering;
- duplicate/stale/out-of-order rejection;
- receiver link timeout;
- all-buttons-release on timeout/fault;
- latest-first movement/no stale replay;
- USB HID report construction;
- TinyUSB lifecycle/replug state;
- receiver diagnostic/state authority.

The RP2040 owns final safety because it owns the USB mouse presented to the host.

## Hardware status

The target boards are known to have a genuine RP2040 and non-CYW43 Wi-Fi silicon identified by the user as ESP8266-class.

Public projects for visually/functionally similar clone "Pico W" boards report ESP8285 and demonstrate:

- reflashing the radio through an RP2040 USB-to-serial bridge;
- a second button placing the ESP8285 into bootloader mode;
- RP2040↔radio communication through UART;
- 115200 baud as an existing bring-up rate.

These are **discovery hypotheses**, not accepted facts for the user's exact boards. FNR-002 must establish the real chip identity, flash size, UART/pin/reset/boot wiring and reliable baud rate from physical evidence.

## Upstream wireless contract

The transmitter repository is:

- `techrote/ESP32-QMI8658C-TiltMouse`

Relevant authority:

- `docs/WIRELESS.md`
- TM-005B / issue #16

Receiver invariants already fixed upstream:

- ESP-NOW v0.1 wireless transport;
- application packet <=250 bytes for ESP-NOW v1/ESP8266 interoperability;
- full current button state in every packet;
- monotonically advancing wireless sequence;
- newest movement wins;
- stale movement is not replayed to fill sequence gaps;
- receiver timeout must release all buttons;
- RF channel/peer/key configuration is explicit.

The exact transmitter packet byte layout is **not owned here**. FNR-004/FNR-006 sync to the upstream TM-005B contract once frozen.

## Internal link strategy

The physical inter-MCU transport is established by FNR-002.

If UART is confirmed, the default v0.1 internal framing is:

- binary;
- self-resynchronizing;
- explicit type/version/length;
- CRC-protected;
- bounded;
- no dynamic allocation required.

COBS + CRC-16/CCITT is the preferred baseline unless physical evidence shows a better reason to choose another framing method.

The radio side forwards the newest complete received wireless packet plus bounded metadata; the RP2040 performs final protocol/order/fault decisions.

## USB policy

Normal receiver identity is mouse-only HID:

- left/right buttons;
- relative signed X/Y;
- no keyboard;
- no wheel/pan unless upstream v0.1 changes;
- no required CDC/vendor interface.

On radio/inter-MCU timeout or invalid state, the RP2040 must stop movement and send/retain all-buttons-released state.

## Toolchain direction

FNR-001 selects and pins exact versions.

Preferred implementation split:

- RP2040: Pico SDK + CMake/Ninja + TinyUSB;
- ESP8266/ESP8285: maintained ESP8266 Arduino core or another demonstrably reproducible ESP8266 toolchain with working ESP-NOW;
- shared protocol/state logic: portable C/C++ native tests independent of target SDKs.

Do not leave toolchains floating on `latest` or `master`.

## Programme

- FNR-001 — reproducible dual-firmware foundation and CI.
- FNR-002 — clone-board hardware characterization and flashing path.
- FNR-003 — RP2040 TinyUSB mouse endpoint and safe lifecycle.
- FNR-004 — ESP8266/ESP8285 ESP-NOW radio receiver layer.
- FNR-005 — framed inter-MCU transport and bounded handoff.
- FNR-006 — integrated receiver state machine and TiltMouse packet semantics.
- FNR-007 — peer/channel/key provisioning and operational recovery.
- FNR-008 — physical end-to-end acceptance, latency and robustness.
- FNR-009 — v0.1 release consolidation.

FNR-002, FNR-003 and hardware-independent FNR-004 work may proceed in parallel after FNR-001. FNR-004's final packet decoder must wait for the upstream TiltMouse TM-005B packet contract.

## v0.1 completion

v0.1 requires physical evidence that:

- the actual clone board can run both target firmwares;
- the ESP radio receives TiltMouse ESP-NOW packets;
- the internal link remains synchronized under loss/corruption/restart tests;
- the RP2040 enumerates as a standard USB mouse;
- motion/buttons work end-to-end;
- duplicate/stale wireless packets do not replay movement;
- wireless/inter-MCU loss releases all buttons;
- recovery does not create a stale cursor burst;
- exact transmitter and receiver source/artifacts are recorded.

## Non-goals

- Bluetooth/Bluetooth-HCI.
- General Internet/network services.
- Recreating genuine Pico W CYW43 APIs.
- General AT-command compatibility.
- Web configuration UI.
- Keyboard emulation.
- Multi-device routing in v0.1.
