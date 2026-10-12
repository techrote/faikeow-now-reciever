# Protocol contracts

## 1. Generic ESP-NOW receiver envelope

FNR-006 freezes **platform v1**. See [FNR-006-RECEIVER.md](FNR-006-RECEIVER.md)
for the complete lifecycle and golden-vector contract.

All multibyte fields are little endian. The 18-byte header is:

| Offset | Width | Field |
| ---: | ---: | --- |
| 0 | 1 | platform version = 1 |
| 1 | 1 | profile ID (0 invalid) |
| 2 | 1 | message type = PROFILE_DATA (1) |
| 3 | 1 | flags = 0 |
| 4 | 8 | nonzero sender-session restart discriminator |
| 12 | 4 | platform sequence u32 |
| 16 | 2 | exact profile-payload length |

At most 232 payload bytes follow in the codec, keeping the platform packet
<=250 bytes. Provisioned FNR-007 RF traffic additionally requires the
[authenticated radio wrapper v1](FNR-007-RADIO-AUTH.md), leaving 228 enclosed
platform bytes / 210 profile bytes within the same radio ceiling. Sender session
is not authentication. Sequence comparison is modulo u32:
duplicate delta 0; newer delta 1..2^31-1; half-range exactly 2^31 rejected as
ambiguous; larger deltas stale. Default application freshness is 250 ms and only
an authorized, valid, ordering-accepted profile message refreshes it.

## 2. Profile contracts

Profile IDs and profile payload schemas are versioned repository-owned contracts.

v0.1 implements:

- `relative_mouse`.

TiltMouse consumes this profile; TiltMouse does not own the generic platform envelope.

### Relative mouse v1

Profile ID **1**, schema **1**, exact four-byte payload:

| Offset | Field |
| ---: | --- |
| 0 | schema version = 1 |
| 1 | complete buttons: bit0 left, bit1 right, other bits zero |
| 2 | relative X signed int8, -127..127; -128 invalid |
| 3 | relative Y signed int8, -127..127; -128 invalid |

Accepted movement is applied once. Duplicate/stale/ambiguous input contributes no
output. Gaps are diagnostic only. Timeout/session/transport/USB invalidation
neutralizes to zero movement plus released buttons; no stale movement backlog is
replayed.

## 3. Sequence/session semantics

The generic core owns one canonical wrap-safe sequence comparator. First valid
session observation establishes a baseline; a sender restart uses a fresh nonzero
u64 session. While the current session is fresh, a different session is accepted
immediately only at sequence zero; after application timeout it may establish at
a later sequence if zero was lost. Four recently retired sessions are remembered
to reject delayed old-session packets.

Board-local FNR-005 faults neutralize/invalidate application freshness but retain
the last platform session/sequence so a stale same-session RF packet cannot become
acceptable merely because the UART parser resynchronized. A genuine sender
restart is represented by the independent platform session.

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

FNR-007 freezes public provisioning-record v1 as a 40-byte little-endian,
CRC-32/IEEE-protected record carrying channel, allowed peer, platform/profile
versions, freshness timeout, public config ID and generation. Provisioned v0.1
requires encryption.

The per-device 16-byte ESP-NOW key is **not** part of the public record and is
linked only into the generated ESP8285 image. The RP2040 image contains no radio
key. Factory images contain neither provisioning record nor key.

RP peer/profile admission is enabled only after a matching READY radio status is
received on the expected channel; radio/link loss revokes that acknowledgement.
No FNR-005 frame type or generic platform-envelope field is added for
provisioning. See [FNR-007-PROVISIONING.md](FNR-007-PROVISIONING.md).

No real universal production secret is committed.

## 8. TiltMouse interoperability

TiltMouse is the first reference sender.

Its transmitter task must encode the generic platform envelope + `relative_mouse` profile rather than create a private receiver-only protocol.

Physical compatibility is accepted jointly through TiltMouse and FNR acceptance evidence.
