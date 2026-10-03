# Architecture

## System boundary

This receiver is a two-MCU appliance:

```text
                 RADIO DOMAIN                 USB / SAFETY DOMAIN

TiltMouse  ~~~>  ESP8266/ESP8285  ======>  RP2040  ----USB----> host
          ESP-NOW                 internal          TinyUSB
                                  framed link
```

The separation is intentional. The radio MCU deals with ESP-NOW timing; the RP2040 deals with mouse state and host safety.

## Why the RP2040 owns final state

The RP2040 physically controls the USB HID device. It therefore owns:

- whether a report is valid;
- which wireless sequence is newest;
- whether a link timeout has occurred;
- when to release buttons;
- whether stale movement must be discarded;
- USB lifecycle/replug state.

This prevents an ESP radio reset or serial glitch from directly leaving the host with a stuck button or replaying old movement.

## ESP8266/ESP8285 firmware

### Responsibilities

- initialize station-mode Wi-Fi only as required for ESP-NOW;
- configure explicit channel;
- initialize ESP-NOW;
- accept configured peer/source;
- copy received bytes into a bounded newest-packet slot/ring;
- forward packets over the internal link;
- expose bounded diagnostics/status;
- recover from radio reset/init failure without flooding the RP2040.

### Non-responsibilities

- USB descriptors;
- HID report generation;
- final sequence acceptance;
- final button timeout safety;
- infrastructure Wi-Fi/IP networking;
- Bluetooth.

### Callback discipline

ESP-NOW receive callbacks must not perform blocking serial writes, dynamic allocation or long parsing.

A callback should copy validated-length data and minimal metadata into preallocated bounded state, then return. Foreground code forwards it over the internal link.

## RP2040 firmware

### Responsibilities

- initialize TinyUSB mouse device;
- receive/parse internal frames;
- verify framing CRC/type/version/length;
- validate the TiltMouse wireless packet contract;
- apply source/peer policy if metadata is forwarded;
- perform wrap-safe sequence ordering;
- reject duplicate/stale/out-of-order input;
- implement link timeout;
- map current accepted report to USB HID;
- release all buttons on invalid/timeout state;
- discard movement when state becomes invalid;
- clear stale state on USB/receiver lifecycle transitions.

## Internal transport

FNR-002 establishes the physical transport.

### UART hypothesis

Public prior art for this clone family uses RP2040 UART to communicate with ESP8285 and can use the RP2040 as a USB-to-serial flashing bridge.

If confirmed on the target board, use UART with:

- a conservative proven bring-up baud first;
- later baud increase only with measured error-free evidence;
- binary self-resynchronizing frames;
- CRC protection;
- bounded receive buffers.

### Preferred UART frame encoding

Default if UART is confirmed:

```text
COBS(
  version
  type
  payload_length
  local_frame_sequence
  payload...
  crc16_ccitt
)
0x00 delimiter
```

Exact field widths are frozen by FNR-005.

Frame types should remain few and explicit, for example:

- RADIO_PACKET — newest raw TiltMouse wireless packet + bounded RX metadata;
- RADIO_STATUS — channel/MAC/counter/reset status;
- CONTROL — bounded configuration/restart requests if needed;
- CONTROL_ACK — explicit result.

Do not tunnel arbitrary AT commands as the production protocol.

## Wireless packet handling

The exact wireless bytes are upstream-owned.

The ESP radio does only minimum length/source sanity before forwarding.

The RP2040 performs final:

1. packet version validation;
2. exact expected length/range checks;
3. wireless sequence ordering;
4. complete button-state update;
5. movement extraction;
6. timeout/recovery policy.

This lets the critical parser/state machine run in native tests without the ESP SDK.

## Latest-first semantics

There are two independent sequences:

- upstream wireless report sequence, owned by TiltMouse;
- local inter-MCU frame sequence, owned by this receiver.

They solve different problems and must not be conflated.

If intermediate capacity is exhausted, drop **older movement** rather than enqueue unbounded stale reports.

Button state is repaired by future full-state reports; timeout is the final safety mechanism.

## USB HID

Normal USB surface:

- one relative mouse HID interface;
- left/right button bits;
- signed relative X/Y;
- no keyboard;
- no automatic movement;
- no required CDC.

A debug build may temporarily expose diagnostics if an issue explicitly owns it, but physical v0.1 acceptance must verify the normal mouse-only path.

## Timeouts

FNR-006/FNR-008 choose evidence-backed timeout values.

The state machine must distinguish:

- no valid wireless packet received yet;
- valid active link;
- inter-MCU link fault;
- wireless timeout;
- USB not mounted/suspended.

Any state that makes button ownership uncertain must converge to all-buttons-released.

## Recovery

Recovery principles:

- do not replay movement accumulated while USB is unavailable;
- do not replay movement received before a radio/internal-link restart;
- after a timeout/restart, require a fresh valid wireless packet before motion resumes;
- full button state from the fresh packet establishes current buttons.

## Concurrency

Prefer simple bounded event loops over unnecessary multicore/shared-state complexity.

Use RP2040 second core only if profiling/physical evidence demonstrates a concrete need. USB + UART + mouse-rate traffic should be small enough that v0.1 correctness is better served by simpler ownership.
