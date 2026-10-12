# Architecture

## System boundary

```text
                 RADIO DOMAIN                     RP2040 DOMAIN

ESP-NOW sender ~> ESP8266/ESP8285 ======> receiver core ======> profile ======> USB HID
                     generic             generic envelope      semantics
                     ingress              + dispatch
```

The two-MCU separation is intentional:

- ESP radio owns RF timing and bounded datagram ingress;
- RP2040 owns generic receiver state, profile dispatch and host-facing interfaces.

## Layering

```text
ESP-NOW
  |
  v
radio_ingress              profile-agnostic
  |
  v
internal_transport         profile-agnostic
  |
  v
receiver_envelope/core     profile-aware only by ID/dispatch
  |
  +--> relative_mouse      v0.1
  +--> keyboard            future
  +--> gamepad             future
  +--> consumer_control    future
  +--> custom/PIO-backed   future
  |
  v
usb_hid_framework
  |
  v
host
```

## ESP8266/ESP8285 responsibilities

- station-mode Wi-Fi only as required for ESP-NOW;
- explicit RF channel;
- peer/source filtering;
- bounded receive callback;
- newest-datagram handoff;
- radio diagnostics/status;
- no profile decoding;
- no HID report generation.

## Internal transport

FNR-002 established UART0 GP0/GP1, 115200 8N1, no flow control.

FNR-005 internal v1 carries exactly these generic frame types:

- `RADIO_DATAGRAM`;
- `RADIO_STATUS`;

No reverse control plane is required for v1.

Do not define `TILTMOUSE_PACKET` or `MOUSE_PACKET` at this layer.

The [v1 contract](FNR-005-TRANSPORT.md) fixes COBS + CRC-16/CCITT-FALSE,
version/type/length/local sequence and an opaque transport generation. One
immutable active TX frame plus newest pending slots bound the producer;
a 1024-byte IRQ-owned byte ring and two owned frame slots bound reception.
Sticky transport events must be processed before taking data. This generation
and the 3-second link watchdog do not implement FNR-006 sender freshness.

## Generic receiver envelope

FNR-006 freezes an explicit 18-byte little-endian platform-v1 header above the
FNR-005 datagram payload: version, profile ID, message type, flags, nonzero u64
sender session, u32 platform sequence and u16 exact payload length. v1 defines
only PROFILE_DATA type 1 and zero flags. At most 232 profile bytes follow so the
complete ESP-NOW application packet remains <=250 bytes.

The envelope is independent of ESP-NOW source MAC; the core's configuration
seam separately authorizes an allowed source peer and explicitly enables the
selected profile. Sender session is a restart
discriminator, not authentication. See [FNR-006-RECEIVER.md](FNR-006-RECEIVER.md).

## Receiver core

The RP2040 core owns:

- generic envelope validation;
- configured peer/profile authorization;
- sender/session restart detection;
- wrap-safe sequence ordering;
- freshness/timeout state;
- profile dispatch;
- invalidation on radio/internal-link restart;
- USB lifecycle coordination;
- diagnostics.

The core must not interpret mouse buttons, keyboard keys or gamepad axes.

## Profile interface

FNR-006 freezes a narrow profile API: a numeric profile ID, payload validation,
accepted-message handling, neutralization reason and optional USB-state
notification. The generic core passes opaque profile payload bytes and never
reads mouse axes/buttons.

### Relative mouse

Profile ID 1 / schema v1 is exactly four bytes: schema, complete two-bit button
state, signed X and signed Y. Axes are -127..127; -128 is invalid.

- relative movement is applied at most once;
- duplicate/stale/ambiguous message -> no movement;
- sequence gap -> count it, do not replay;
- complete current button state carried by accepted message;
- freshness/session/transport/USB invalidation -> zero movement + released buttons;
- unavailable USB drops movement rather than accumulating a replay backlog.

### Keyboard future example

A future keyboard profile may use complete key-state snapshots and timeout -> release all keys.

### Gamepad future example

A future gamepad profile may treat each message as the newest complete controller state.

These examples justify keeping loss semantics inside profiles.

## Provisioning boundary

FNR-007 keeps provisioning outside the generic application envelope and FNR-005
framing. A CRC-protected public record identifies peer/channel/platform/profile
and freshness policy. The ESP image alone contains the per-device ESP-NOW key.

RP peer/profile admission is staged disabled and becomes active only after a
matching READY radio status crosses FNR-005. Link/radio failure revokes that
acknowledgement. Factory firmware has no configured peer/key and remains inert.
Reconfiguration uses paired image rebuild/reflash through the accepted no-solder
recovery paths.

## USB HID framework

FNR-003 owns a generic TinyUSB framework:

- profile-owned descriptors/report definitions;
- mount/unmount/suspend/resume;
- no stale report replay across invalid/unmounted state;
- one selected production profile for v0.1;
- optional future composite support only when explicitly designed.

The normal v0.1 relative-mouse profile exposes only a standard mouse HID interface.

## PIO extension boundary

PIO is not part of v0.1 acceptance.

Future profile backends may use RP2040 PIO/native peripherals for unusual wired protocols or timing-sensitive adaptation. Such expansion must reuse the generic ingress/core where possible rather than changing the ESP-NOW radio path.

## Safe startup/recovery

- no profile output before configuration and fresh valid platform message;
- radio/internal-link restart invalidates application freshness/output but retains the sender session/sequence baseline so stale same-session packets remain rejectable;
- USB detach/suspend neutralizes profile output without resetting sender ordering/freshness, and reconnect does not replay queued stale output;
- profile timeout neutralizes profile state according to profile policy.

## Concurrency

Prefer bounded event loops and explicit ownership. Use RP2040 multicore only if measured need justifies additional synchronization complexity.
