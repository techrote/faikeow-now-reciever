# Programme — faikeow-now-reciever v0.1

## Goal

Deliver a reusable ESP-NOW-to-USB HID receiver platform on RP2040 + ESP8266/ESP8285 clone Pico-W hardware.

v0.1 proves the platform with one generic **relative-mouse profile** and uses TiltMouse as the first reference sender.

## Dependency graph

```text
FNR-001 foundation / CI
   |
   +--------------------+--------------------+
   |                    |                    |
   v                    v                    v
FNR-002              FNR-003              FNR-004
hardware             generic HID          generic ESP-NOW
characterization     framework             ingress
   |                    |                    |
   +----------+---------+--------------------+
              |
              v
        FNR-005 internal
        datagram transport
              |
              v
        FNR-006 receiver core
        + profile dispatch
        + relative mouse
              |
              v
        FNR-007 generic
        provisioning/recovery
              |
              v
        FNR-008 platform physical
        acceptance + TiltMouse reference
              |
              v
        FNR-009 v0.1 release
```

## FNR-001 — Reproducible dual-firmware foundation and CI

Create the shared/RP2040/ESP-radio repository skeleton, pin both toolchains and establish native tests plus both cross-builds.

The foundation must be profile-neutral.

## FNR-002 — Clone-board characterization and flashing path

Establish actual radio identity, RP2040↔radio transport/pins, boot/reset behavior, reversible flashing and a physically proven internal link rate.

Unchanged by the generalization.

## FNR-003 — Generic RP2040 TinyUSB HID framework

Implement:

- TinyUSB lifecycle abstraction;
- generic HID-profile registration/selection seam;
- profile-owned descriptor/report hooks;
- target-independent HID state helpers;
- v0.1 `relative_mouse` USB backend as the first profile implementation.

Do not make the core USB framework assume that all future profiles are mice.

## FNR-004 — Generic ESP-NOW datagram ingress

Implement the ESP radio as a payload-agnostic ingress layer:

- explicit SDK channel selection/verification and exact-source admission;
- bounded newest-datagram state;
- source metadata/counters;
- no HID semantics;
- no TiltMouse decoder;
- no infrastructure IP networking.

## FNR-005 — Framed inter-MCU datagram transport

Carry generic ESP-NOW datagrams and radio/control status across the board-internal link.

The internal protocol must not name TiltMouse or mouse fields.

The accepted FNR-002 UART supports the
[FNR-005 v1 software implementation](FNR-005-TRANSPORT.md): COBS + CRC16,
bounded producer/consumer storage, foreground ESP TX, RP2040 IRQ RX, and
generic failure/restart events. Its
[production physical campaign](FNR-005-HARDWARE-HANDOFF.md) has 44 passed phases
on one representative identical-design board, including sustained production
framing and independent reset/recovery. See [curated evidence](../evidence/FNR-005/README.md).
PR #20/#6 record final CI, merge and post-merge programme closure. Earlier
diagnostic UART evidence remains an accepted input, separate from these results.

## FNR-006 — Generic receiver core, profile dispatch and relative mouse

Freeze and implement generic platform v1 plus the first production profile.
The accepted software contract is documented in [FNR-006-RECEIVER.md](FNR-006-RECEIVER.md):

- explicit 18-byte little-endian platform header;
- nonzero u64 sender-session restart discriminator;
- modulo-u32 ordering with an explicit half-range rejection rule;
- 250 ms default application freshness, separate from FNR-005 link liveness;
- fail-closed one-peer/profile configuration seam for FNR-007;
- generic profile dispatch with no mouse fields in the receiver core;
- relative_mouse profile ID 1 / schema v1, exact four-byte full-button-state payload;
- transport/session/USB invalidation to neutral output with no stale movement replay.

TiltMouse TM-005B consumes this repository-owned byte contract and vectors; it
does not define a competing receiver protocol.

## FNR-007 — Generic provisioning and recovery

v0.1 uses paired build-time provisioning. One local, untracked configuration
generates both firmware inputs: the RP2040 receives only a public CRC-protected
record, while the ESP8285 additionally receives one per-device 16-byte ESP-NOW
key. Factory firmware remains unprovisioned/fail-closed.

The RP2040 stages peer/profile settings but enables admission only after the
FNR-005 status path reports the ESP radio READY on the expected channel. Radio
or internal-link failure revokes that acknowledgement. Reconfiguration/factory
reset uses the already accepted no-solder flashing paths; there is no permanent
host provisioning service.

See [FNR-007-PROVISIONING.md](FNR-007-PROVISIONING.md). Software qualification
covers corruption, secret separation, radio setup and RP acknowledgement.
Representative-board provisioning/power-cycle/recovery smoke remains required
before issue acceptance.

## FNR-008 — Platform physical acceptance + TiltMouse reference profile

Physically establish:

### Platform

- board flashing/recovery;
- ESP-NOW ingress;
- internal datagram integrity;
- peer/session/order/freshness behavior;
- USB lifecycle;
- provisioning/recovery.

### Relative mouse profile

- standard USB mouse enumeration;
- X/Y/buttons;
- duplicate/loss/reorder behavior;
- timeout release;
- no stale movement replay.

### Reference interoperability

Use exact TiltMouse artifacts to prove the first real sender/profile integration.

The evidence must distinguish platform claims from TiltMouse-specific interoperability claims.

## FNR-009 — v0.1 platform release

Release:

- RP2040 UF2;
- ESP radio firmware;
- generic platform protocol version;
- internal protocol version;
- relative-mouse profile version;
- provisioning/recovery docs;
- TiltMouse compatibility statement;
- reproducible manifests/checksums.

The project README/release must describe TiltMouse as a reference profile, not the only intended use.

## Design rules

1. Core radio ingress stays profile-agnostic.
2. Internal MCU transport stays profile-agnostic.
3. Receiver core handles generic routing/order/freshness; profiles handle payload semantics.
4. HID descriptors/reports belong to profiles, not generic transport code.
5. Adding a keyboard/gamepad/custom profile must not require rewriting the radio or inter-MCU layers.
6. PIO-backed future backends remain possible but out of v0.1 scope.
