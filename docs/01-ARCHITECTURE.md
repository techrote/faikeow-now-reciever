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

FNR-002 establishes physical transport.

FNR-005 carries generic frame types such as:

- `RADIO_DATAGRAM`;
- `RADIO_STATUS`;
- bounded `CONTROL` / `CONTROL_ACK` if required.

Do not define `TILTMOUSE_PACKET` or `MOUSE_PACKET` at this layer.

If UART is confirmed, preferred encoding remains COBS + CRC-16/CCITT with explicit version/type/length/local sequence.

## Generic receiver envelope

FNR-006 freezes exact encoding.

Required logical fields:

- platform protocol version;
- profile ID;
- message type/flags;
- sender/session identifier sufficient to distinguish sender restart/session reset;
- monotonic message sequence;
- payload length;
- profile payload.

The platform envelope is independent of ESP-NOW source MAC, though peer policy may bind/configure allowed senders separately.

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

A profile should expose a narrow contract roughly equivalent to:

```text
profile_id()
validate_message(type, flags, payload)
on_message(sequence, payload, now)
on_timeout(now)
on_transport_reset()
on_usb_state(...)
build/submit HID state
descriptor/report hooks
```

Exact API is owned by FNR-003/FNR-006.

Profiles may define different reliability semantics.

### Relative mouse

- relative movement is applied at most once;
- duplicate/stale message -> no movement;
- sequence gap -> count it, do not replay;
- complete current button state carried by accepted message;
- timeout/reset -> zero movement + released buttons.

### Keyboard future example

A future keyboard profile may use complete key-state snapshots and timeout -> release all keys.

### Gamepad future example

A future gamepad profile may treat each message as the newest complete controller state.

These examples justify keeping loss semantics inside profiles.

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
- radio/internal-link restart invalidates prior session/freshness state;
- USB reconnect does not replay queued stale output;
- profile timeout neutralizes profile state according to profile policy.

## Concurrency

Prefer bounded event loops and explicit ownership. Use RP2040 multicore only if measured need justifies additional synchronization complexity.
