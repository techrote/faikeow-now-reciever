# HID profiles and extension model

## Purpose

Profiles let the receiver platform support different HID semantics without changing:

- ESP-NOW radio ingress;
- board-internal framing;
- peer/session/order core;
- provisioning foundations.

## v0.1

Only one production profile is required:

### relative_mouse

Use case: TiltMouse and similar relative pointing devices.

HID surface:

- relative X/Y;
- left/right buttons.

State policy:

- movement applies once from each accepted fresh message;
- duplicate/stale messages do not move;
- missing messages are not replayed;
- each message carries complete button state;
- timeout/restart releases buttons.

## Future profiles

Not v0.1 requirements:

### keyboard

Likely full/current key-state or carefully designed event semantics.

Safety requirement: timeout/restart must release all keys/modifiers.

### gamepad

Likely newest complete state snapshot.

Timeout policy should neutralize axes/buttons.

### consumer_control

Media/consumer HID controls with explicit state/edge semantics.

### custom/composite HID

May expose project-specific descriptors/interfaces.

Must remain bounded and versioned.

## Profile API principles

Profiles should be registered by stable profile ID.

The generic core should provide accepted envelope metadata plus payload; the profile should not need ESP8266 or internal-link knowledge.

A profile owns:

- supported message types;
- payload parsing;
- profile-specific validation;
- timeout neutralization;
- HID descriptor/report implementation;
- diagnostics meaningful to that profile.

The core owns:

- configured peer/profile permission;
- session/restart state;
- sequence acceptance;
- generic freshness;
- dispatch;
- transport reset notification.

## Versioning

Platform protocol version and profile schema version are distinct.

A future platform envelope revision must not be required merely to add a new profile.

A profile may evolve independently with explicit compatibility rules.

## PIO/native-peripheral extension

The project is primarily an ESP-NOW -> USB HID receiver.

However, RP2040 profile/backend boundaries should permit future work that uses PIO or native peripherals for unusual peripheral adaptation.

Examples may include retro keyboard/mouse protocols, deterministic GPIO protocols or other timing-sensitive bridges.

Such backends are post-v0.1 and must not complicate the first USB HID release.
