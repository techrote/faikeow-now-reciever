# Programme — faikeow-now-reciever v0.1

## Goal

Produce a reproducible dual-MCU firmware set that turns the target RP2040 + ESP8266/ESP8285 clone Pico-W board into a robust ESP-NOW-to-USB mouse receiver for TiltMouse.

## Dependency graph

```text
FNR-001 foundation / CI
   |
   +--------------------+--------------------+
   |                    |                    |
   v                    v                    v
FNR-002              FNR-003              FNR-004
hardware             RP2040 USB           ESP radio
characterization     HID endpoint          foundation/RX
   |                    |                    |
   +----------+---------+--------------------+
              |
              v
        FNR-005 internal link
              |
              v
        FNR-006 integration
              |
              v
        FNR-007 provisioning/
                recovery policy
              |
              v
        FNR-008 physical acceptance
              |
              v
        FNR-009 v0.1 release
```

FNR-004 may build its ESP-NOW receive abstraction before the transmitter byte format is frozen, but final packet interoperability work in FNR-006 depends on the TiltMouse TM-005B / #16 contract.

## FNR-001 — Reproducible dual-firmware foundation and CI

Create the repository/build skeleton for both MCUs plus target-independent native tests.

Select and pin exact toolchains rather than floating releases. Establish:

- RP2040 Pico SDK/TinyUSB target;
- ESP8266/ESP8285 build target with confirmed ESP-NOW API;
- portable shared library/test target;
- CI for both cross-builds, native tests and text/style sanity;
- manifest/version traceability for produced firmware artifacts.

No physical board behavior is claimed here.

## FNR-002 — Clone-board characterization and flashing path

Use the actual clone board to establish facts that public prior art cannot safely guarantee:

- exact radio silicon identity and flash size;
- RP2040↔radio data pins/peripheral;
- reset and bootloader control;
- behavior of both physical buttons;
- USB-to-serial flashing bridge procedure;
- usable default and higher UART baud rates if UART is confirmed;
- power/reset behavior during independent MCU flashing.

Commit a board contract and reproducible flashing guide. This is a physical evidence issue.

## FNR-003 — RP2040 USB HID endpoint

Implement the RP2040-facing host endpoint independently of ESP-NOW:

- normal USB identity is a standard relative mouse;
- left/right buttons + relative X/Y only;
- deterministic report construction;
- all-buttons-release;
- mount/unmount/suspend/resume/replug handling;
- no synthetic default movement;
- native tests for pure HID/report state.

Use synthetic/internal test reports; no radio dependency.

## FNR-004 — ESP8266/ESP8285 radio receiver layer

Implement the radio MCU as a narrow ESP-NOW front-end:

- initialize only the Wi-Fi state required for ESP-NOW;
- explicit RF channel;
- peer/source filtering;
- receive callback copies into bounded newest-packet state;
- no stale-packet backlog;
- diagnostic counters;
- portable seam for packet handoff;
- cross-build under the pinned ESP toolchain.

Do not give the ESP MCU responsibility for USB HID. Do not add general Wi-Fi/IP features.

## FNR-005 — Framed inter-MCU transport

On the physical link established by FNR-002, implement a bounded, recoverable binary link between the MCUs.

If UART is confirmed, default to COBS + CRC-16/CCITT unless evidence justifies a different framing.

Requirements:

- explicit frame type/version/length;
- bounded payload;
- corruption/truncation/resynchronization tests;
- newest-report semantics;
- no unbounded queues;
- restart detection/status;
- diagnostic/control frames sufficient for bring-up;
- target-independent encoder/parser tests.

The ESP side forwards newest wireless input; the RP2040 remains final state authority.

## FNR-006 — Integrated receiver and TiltMouse semantics

Sync to the exact upstream TiltMouse TM-005B packet contract and implement the complete receiver path:

```text
ESP-NOW packet
 -> ESP radio handoff
 -> framed inter-MCU link
 -> RP2040 validation/order
 -> timeout/state machine
 -> USB mouse report
```

Own:

- exact wireless version/length decode;
- wrap-safe sequence comparison;
- duplicate/stale/out-of-order rejection;
- latest-first movement;
- full button-state application;
- timeout -> all-buttons-released;
- recovery starts from fresh state;
- no stale cursor backlog;
- deterministic fault/replay tests.

## FNR-007 — Provisioning, peer/channel/key and operational recovery

Make deployment repeatable without hard-coded project-wide secrets.

Define and implement:

- transmitter peer MAC configuration;
- receiver MAC discovery/reporting;
- RF channel configuration;
- encryption/key provisioning if upstream TM-005B uses it;
- safe factory/default state;
- controlled reconfiguration path;
- reset/reflash recovery;
- behavior when peer/channel/key is wrong;
- diagnostics usable without changing normal USB mouse identity.

Avoid web UI or infrastructure networking.

## FNR-008 — Physical end-to-end acceptance

Run the complete real chain with exact artifacts:

```text
TiltMouse ESP32-S3
 -> ESP-NOW RF
 -> clone ESP8266/ESP8285
 -> internal link
 -> RP2040
 -> USB HID
 -> host
```

Measure and record:

- enumeration/driver behavior;
- report rate and practical latency/jitter;
- packet/sequence gaps;
- duplicate/stale rejection;
- button press/hold/release;
- mouse movement;
- transmitter loss;
- radio reset;
- RP2040 reset;
- USB replug;
- internal-link corruption/recovery where practical;
- timeout all-buttons-release;
- no stale movement burst after recovery;
- operating range sufficient for intended desktop use.

Physical evidence must identify both repository commits/artifacts and host OS.

## FNR-009 — v0.1 release consolidation

Freeze the accepted board contract, toolchains, protocol version and defaults.

Produce/document:

- RP2040 UF2;
- ESP8266/ESP8285 binary;
- checksums/manifests;
- flashing sequence;
- peer/channel/key setup;
- recovery procedure;
- known limitations;
- exact upstream TiltMouse compatibility contract/version;
- release/tag if tooling permits.

Close v0.1 only if FNR-008 evidence applies to the exact release candidate.

## Programme rules

- Do not collapse the radio and USB responsibilities into one MCU merely for convenience.
- Do not bypass FNR-002 by assuming another clone's pinout.
- Do not merge physical claims based on CI.
- Do not invent an incompatible TiltMouse packet layout.
- Do not allow stale cursor movement to accumulate across any queue/link.
- Keep safety decisions deterministic and testable on the RP2040 side.
