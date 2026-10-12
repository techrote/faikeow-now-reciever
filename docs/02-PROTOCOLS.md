# Protocol contracts

## 1. Generic ESP-NOW receiver envelope

This repository owns the platform envelope.

FNR-006 freezes exact byte encoding and golden vectors.

Logical v1 contents must include:

- platform version;
- profile ID;
- message type/flags;
- sender/session identifier or restart discriminator;
- monotonic message sequence;
- payload length;
- profile payload.

Total ESP-NOW application payload must remain <=250 bytes for ESP8266-class interoperability.

The envelope routes/orders data without understanding profile payload semantics.

## 2. Profile contracts

Profile IDs and profile payload schemas are versioned repository-owned contracts.

v0.1 implements:

- `relative_mouse`.

TiltMouse consumes this profile; TiltMouse does not own the generic platform envelope.

### Relative mouse v1

Freeze exact payload layout in FNR-006.

It must carry:

- bounded relative X;
- bounded relative Y;
- complete current logical button state;
- any profile-specific reserved/version bits required for safe evolution.

Policy:

- accepted movement is applied once;
- duplicate/stale -> no movement;
- gaps are diagnostic, not retransmission requests;
- timeout/reset -> zero movement and released buttons.

## 3. Sequence/session semantics

The generic core owns wrap-safe platform sequence comparison.

Requirements:

- first valid message in a new sender/session establishes baseline;
- duplicate -> reject;
- older/out-of-order -> reject;
- newer -> accept;
- wrap at frozen integer width -> deterministic;
- session/restart discriminator change resets ordering baseline safely;
- radio/internal-link restart invalidates freshness and may require fresh session/message.

Profiles receive only messages accepted by generic ordering unless a profile explicitly defines another class of message.

## 4. Internal ESP↔RP2040 protocol

FNR-005 freezes [internal protocol v1](FNR-005-TRANSPORT.md), using the accepted
FNR-002 onboard UART at 115200 8N1 without flow control.

Requirements:

- self-resynchronizing if byte-stream based;
- explicit protocol version;
- frame type;
- bounded payload length;
- local frame sequence;
- integrity check;
- deterministic corruption recovery.

Encoding: COBS + `0x00` delimiter + CRC-16/CCITT-FALSE (poly `0x1021`, init
`0xffff`, no reflection, xorout zero). Multibyte fields and CRC are little
endian. Header: version u8, type u8, payload length u16, local sequence u32,
transport generation u64. The CRC covers the entire header plus payload.
Maximum raw/encoded/wire lengths are 274/276/277 bytes. No empty datagram is
permitted; a datagram contains six MAC bytes plus 1-250 opaque application bytes.

### Frame types

Use profile-neutral names:

- `RADIO_DATAGRAM` — raw ESP-NOW application payload + bounded RF/source metadata;
- `RADIO_STATUS` — fixed 48-byte radio/loss snapshot and sticky RESET marker.

Only type values 1 and 2 are accepted. `CONTROL`/`CONTROL_ACK` are not defined
in v1 because the bounded one-way transport requires neither acknowledgements
nor retransmission. The transport generation is a local restart discriminator,
not the generic sender/session identifier. Full offsets, golden vectors,
ownership, recovery rules and APIs are in the v1 contract. Physical acceptance
is tracked in the [FNR-005 handoff](FNR-005-HARDWARE-HANDOFF.md).

## 5. USB HID/profile contract

The generic USB framework does not impose one report descriptor.

Each profile owns:

- descriptor/report schema;
- neutral/fail-safe output;
- message-to-HID state logic.

v0.1 `relative_mouse` exposes:

- left/right buttons;
- relative X/Y;
- no keyboard;
- no required wheel/pan.

## 6. Queue/backpressure

Radio and inter-MCU layers are bounded.

Generic rule:

- never create an unbounded datagram queue;
- preserve whole-message boundaries;
- expose drops diagnostically.

Drop/coalescing policy may depend on message type/profile. The radio layer may use a small bounded queue/newest slot, but must not itself interpret profile payload fields.

## 7. Provisioning/configuration

FNR-007 owns:

- allowed sender peer(s);
- RF channel;
- keys/encryption when used;
- enabled/selected profile;
- compatible platform/profile versions.

No real universal production secret is committed.

## 8. TiltMouse interoperability

TiltMouse is the first reference sender.

Its transmitter task must encode the generic platform envelope + `relative_mouse` profile rather than create a private receiver-only protocol.

Physical compatibility is accepted jointly through TiltMouse and FNR acceptance evidence.
