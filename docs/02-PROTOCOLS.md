# Protocol contracts

## 1. TiltMouse ESP-NOW packet

The exact wireless packet layout is **upstream-owned** by:

- repository: `techrote/ESP32-QMI8658C-TiltMouse`
- document: `docs/WIRELESS.md`
- issue: TM-005B / #16

Do not freeze a competing byte format here before TM-005B does.

### Receiver-required invariants

The upstream contract already requires:

- packet size <=250 bytes;
- protocol version;
- monotonically advancing sequence;
- bounded relative X/Y;
- complete current left/right button state;
- newest movement wins;
- no retransmission backlog to fill old motion gaps;
- explicit peer/channel/provisioning;
- timeout behavior that releases all buttons.

FNR-004 may receive/forward opaque bounded payloads before the exact format freezes. FNR-006 imports/freezes the exact decoder and compatibility vectors once upstream is authoritative.

## 2. Wireless sequence semantics

FNR-006 implements one canonical wrap-safe comparison.

Requirements:

- duplicate sequence -> reject;
- older/out-of-order sequence -> reject;
- newer sequence -> accept;
- wrap at the exact upstream integer width -> handled deterministically;
- gaps are counted but do not trigger replay requests;
- a timeout/restart invalidates old ordering state until a fresh packet is accepted.

Tests must cover values immediately before/after wrap.

## 3. Internal ESP↔RP2040 protocol

FNR-005 freezes this repository-owned protocol after FNR-002 establishes the physical link.

### Requirements

- self-resynchronizing byte stream if UART;
- explicit protocol version;
- explicit frame type;
- explicit bounded payload length;
- local frame sequence;
- integrity check;
- deterministic parser behavior on truncation/corruption/noise;
- no dynamic allocation required.

### Preferred UART encoding

COBS framing with `0x00` delimiter and CRC-16/CCITT over the decoded header+payload.

Rationale:

- delimiter never appears inside encoded frame;
- recovery after byte loss/corruption is bounded to a frame;
- easy native fuzz/replay testing;
- very small overhead for mouse-size reports.

If FNR-002 proves the physical link is not UART, FNR-005 must document the replacement and preserve equivalent bounded/integrity semantics.

### RADIO_PACKET payload

Prefer forwarding:

- raw upstream TiltMouse payload;
- source MAC;
- optional receive metadata that is actually available/useful;
- no interpretation that would prevent the RP2040 from being final decoder/state authority.

Never exceed the fixed internal maximum.

### RADIO_STATUS payload

May include:

- radio firmware protocol version;
- station MAC;
- active channel;
- configured peer;
- receive count;
- dropped/overwritten packet count;
- radio restart count;
- last error code.

Keep diagnostics compact and versioned.

## 4. USB HID contract

RP2040 normal output:

- standard relative mouse;
- two logical button bits;
- relative X/Y;
- no keyboard;
- no required wheel;
- no required vendor/CDC interface.

When no fresh valid report is available:

- X = 0;
- Y = 0;
- buttons = released if the link/state is invalid or timed out.

## 5. Queue/backpressure contract

Radio receive and UART forwarding are latest-first.

Permitted behavior under pressure:

- overwrite/drop older **unforwarded movement-bearing reports**;
- increment a diagnostic drop counter;
- forward the newest complete packet.

Forbidden behavior:

- unbounded queues;
- replaying a long backlog after congestion;
- partial packet forwarding;
- dropping the only known release state and then suppressing timeout release.

## 6. Configuration/provisioning contract

FNR-007 owns storage and user workflow.

Required configurable values:

- RF channel;
- allowed transmitter MAC/peer;
- encryption material if the upstream transport enables encryption.

Secrets must not be committed as universal production defaults.

## 7. Compatibility evidence

The receiver release must record:

- supported upstream wireless protocol version;
- transmitter repository commit used for acceptance;
- receiver internal protocol version;
- RP2040 firmware version/commit;
- ESP radio firmware version/commit.
