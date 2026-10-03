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

- explicit channel/peer filtering;
- bounded newest-datagram state;
- source metadata/counters;
- no HID semantics;
- no TiltMouse decoder;
- no infrastructure IP networking.

## FNR-005 — Framed inter-MCU datagram transport

Carry generic ESP-NOW datagrams and radio/control status across the board-internal link.

The internal protocol must not name TiltMouse or mouse fields.

If UART is confirmed, use the COBS + CRC bounded baseline unless evidence justifies otherwise.

## FNR-006 — Generic receiver core, profile dispatch and relative mouse

Freeze the generic receiver envelope v1 and implement RP2040 core semantics:

- platform version;
- profile ID;
- message type/flags;
- sender/session restart discrimination;
- sequence ordering;
- payload length;
- profile dispatch;
- generic peer/session/freshness state.

Then implement the first production profile:

- relative X/Y;
- complete button state;
- duplicate/stale rejection;
- timeout -> released buttons;
- no stale movement replay.

TiltMouse is a compatibility/reference implementation of this profile, not the protocol owner.

## FNR-007 — Generic provisioning and recovery

Provision/configure:

- allowed peer(s);
- RF channel;
- encryption/key material if used;
- enabled/selected profile;
- compatible platform/profile versions.

Do not hard-code TiltMouse as the only possible sender.

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
