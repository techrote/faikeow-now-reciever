# FNR-005: board-local transport protocol v1

## Scope and status

[Issue #6 / FNR-005](https://github.com/techrote/faikeow-now-reciever/issues/6)
owns this protocol and its portable codec, transmitter, stream parser and
bounded consumer handoff. The implementation carries complete, opaque FNR-004
datagrams from the ESP8285 foreground to an RP2040 consumer seam. FNR-006 owns
the later receiver envelope, sender/session ordering, freshness and profile
dispatch. FNR-003 owns USB HID. There is no direct transport-to-HID dispatch.

This document freezes the software wire contract. Native tests, target builds
and linked-path checks are separate software evidence; none establishes that
production frames have crossed the physical board UART. The exact remaining
campaign is in [FNR-005-HARDWARE-HANDOFF.md](FNR-005-HARDWARE-HANDOFF.md).

The UART route and 115200 baud, 8N1, no-flow-control configuration are already
accepted FNR-002 inputs. General hardware authority remains the dedicated
[devboard repository](https://github.com/techrote/picoWcloneESP8285N08), linked
and pinned from [03-HARDWARE.md](03-HARDWARE.md). FNR-005 does not infer new
reset/download pins or requalify the board contract in software.

| Established route | Production v1 use |
|---|---|
| ESP UART TX to RP2040 GP1 / UART0 RX | Framed status and datagrams |
| RP2040 GP0 / UART0 TX to ESP UART RX | Reserved route; no reverse v1 messages |

There are exactly two v1 frame types: `RADIO_DATAGRAM` and `RADIO_STATUS`.
There are no `CONTROL`/`CONTROL_ACK` types, acknowledgements, retransmission
requests, arbitrary command tunnel, RSSI field or profile payload decoder.

## Wire encoding

One frame consists of COBS encoding of a decoded header, payload and CRC,
followed by one `0x00` delimiter. A COBS segment contains no zero bytes. All
multibyte integers, including the CRC, are unsigned and **little endian**.
The C structures in the headers are owned in-process records, not packed wire
layouts; serialization uses explicit byte operations.

Let `L` be the declared payload length:

| Decoded offset | Width | Field | Required meaning |
|---|---|---|---|
| 0 | 1 | `version` | `1` |
| 1 | 1 | `type` | `1` = `RADIO_DATAGRAM`; `2` = `RADIO_STATUS` |
| 2 | 2 | `payload_length` | `L`, excluding the 16-byte header and CRC |
| 4 | 4 | `sequence` | Local transport frame sequence |
| 8 | 8 | `generation` | Opaque local radio transport lifecycle token |
| 16 | `L` | `payload` | Type-specific layout below |
| `16 + L` | 2 | `crc` | CRC over decoded bytes `0 .. 15 + L` inclusive |

A receiver rejects unsupported versions and unknown types. There are no
implicit extension fields or permitted trailing bytes. The actual decoded
size must be exactly `16 + L + 2`.

### RADIO_DATAGRAM payload

| Payload offset | Width | Meaning |
|---|---|---|
| 0 | 6 | Source MAC copied from the FNR-004 owned record |
| 6 | 1 through 250 | Complete, unchanged ESP-NOW application payload |

`L` is therefore 7 through 256. Application length is `L - 6`; there is no
second application-length field. A zero-byte application payload is invalid.
The transport does not inspect application bytes or repeat FNR-004 source
admission. A source MAC in this frame is metadata, not an authentication claim.
FNR-006 will parse any receiver envelope inside these application bytes.

### RADIO_STATUS payload

`L` must be exactly 48. The offsets in this table are relative to the payload;
add 16 to obtain decoded-frame offsets.

| Payload offset | Width | Field | Meaning |
|---|---|---|---|
| 0 | 1 | `flags` | Bit 0 = `RESET`; all other bits must be zero |
| 1 | 1 | `state` | 0 = UNCONFIGURED; 1 = STARTING; 2 = READY; 3 = ERROR |
| 2 | 1 | `channel` | 0 through 14; READY requires 1 through 14 |
| 3 | 1 | Reserved | Must be zero |
| 4 | 4 | `received` | FNR-004 receive-call counter |
| 8 | 4 | `accepted` | FNR-004 accepted datagrams |
| 12 | 4 | `rejected_source` | FNR-004 exact-source rejections |
| 16 | 4 | `invalid_length` | FNR-004 invalid record/length inputs |
| 20 | 4 | `dropped` | FNR-004 pending-record replacements or retirement |
| 24 | 4 | `consumed` | FNR-004 owned records taken by foreground |
| 28 | 4 | `starts` | FNR-004 configured starts |
| 32 | 4 | `restarts` | FNR-004 restart diagnostic |
| 36 | 4 | `errors` | FNR-004 error-transition diagnostic |
| 40 | 4 | `tx_replaced` | Replaced pending transport datagrams |
| 44 | 4 | `tx_aborted` | Active/pending transport records retired by restart or unavailable-state handling |

The first nine counters preserve the order and semantics of
`fnr_radio_counters`; see [FNR-004-RADIO-INGRESS.md](FNR-004-RADIO-INGRESS.md).
The transmitter fills the final two fields from its own counters when preparing
a status frame. Status replacement has a separate local diagnostic and is not
included in `tx_replaced`.

Every counter saturates at `UINT32_MAX`. Counters survive software restart
operations on an initialized object; a full MCU boot initializes RAM again.
Counter saturation is never used to detect a lifecycle change. Status carries
no peer secret, key material or profile state.

## CRC and golden bytes

The CRC is **CRC-16/CCITT-FALSE**, with these exact parameters:

| Parameter | Value |
|---|---|
| Width | 16 bits |
| Polynomial | `0x1021` |
| Initial remainder | `0xffff` |
| Input reflection | false |
| Output reflection | false |
| Final XOR | `0x0000` |
| Covered bytes | Complete decoded header and payload |
| CRC storage | Low byte followed by high byte |

CRC calculation excludes its own two bytes, the COBS code bytes and the zero
delimiter. Known checks are empty input -> `0xffff`, ASCII `123456789` ->
`0x29b1`, and the 256 bytes `00` through `ff` in order -> `0x3fbd`.

The minimum-datagram fixture in
[test_transport.cpp](../tests/native/test_transport.cpp) uses application byte
`a5`, source `02:03:04:05:06:07`, local sequence `0x89abcdef`, and generation
`0x0102030405060708`. It has `L = 7` and CRC `0x9a3b`.

Decoded bytes, including the CRC, are 25 bytes:

```text
01 01 07 00 ef cd ab 89 08 07 06 05 04 03 02 01
02 03 04 05 06 07 a5 3b 9a
```

The complete COBS-plus-delimiter wire frame is 27 bytes:

```text
04 01 01 07 16 ef cd ab 89 08 07 06 05 04 03 02 01
02 03 04 05 06 07 a5 3b 9a 00
```

These literals describe one frame. A transmitter also emits an initial zero
boundary before its startup RESET status; that extra synchronization byte is
not part of the fixture above.

## COBS sizes and UART budget

The encoder emits the ordinary terminal empty COBS block after a run of
exactly 254 nonzero bytes. Thus 254 such input bytes encode as code `ff`, the
254 data bytes, and terminal code `01`: **256 encoded bytes**. The decoder
also accepts the valid 255-byte representation without that terminal empty
block. Empty COBS input data encode as `01`; an empty encoded segment is not a
frame. Small examples are `00` -> `01 01` and `11 00 22` -> `02 11 02 22`.

For this encoder's convention:

```text
cobs_bound(n) = n + floor(n / 254) + 1
raw_max       = 16 + (6 + 250) + 2 = 274
encoded_max   = 274 + floor(274 / 254) + 1 = 276
wire_max      = 276 + 1 delimiter = 277
```

`fnr_transport_cobs_bound()` returns zero when the calculation would overflow
`size_t`. Encoders and decoders also check each actual output capacity. No
unchecked declared length controls a buffer copy. The parser can hold exactly
276 nonzero bytes; the next nonzero byte marks that segment oversized and
starts discard-until-delimiter mode.

At the accepted 115200, 8N1 configuration, each UART byte consumes ten line
bits: one start bit, eight data bits and one stop bit. The arithmetic ceiling
is therefore **11,520 wire bytes/s**. These are design budgets, not measured
production throughput or latency guarantees.

| Traffic | Calculated wire budget |
|---|---|
| One maximum datagram | Up to 277 bytes, about 24.05 ms of line time |
| Maximum datagrams alone | At most about 41.59 frames/s at that size |
| One status frame | Exactly 68 bytes: 66 raw, 67 encoded, one delimiter |
| Normal status cadence | One queued snapshot each 1,000 ms, plus startup/restart status |
| Maximum datagrams with one status/s | At most `(11520 - 68) / 277`, about 41.34 frames/s |
| Initial clean campaign at 30 maximum datagrams/s | At most 8,378 bytes/s including one status, about 72.73% of line capacity |
| Deliberate overload at 50 maximum datagrams/s | Up to 13,918 bytes/s including status; bounded loss is required |

Extra startup/restart delimiters and status frames also consume this budget.
The status interval is a foreground scheduling interval, so an active frame
or stalled UART can delay actual emission. The transmitter does not retry
replaced records to satisfy an offered rate. Accepted RF ingress at a faster
rate does not imply loss-free UART forwarding at the same rate.

## Sequence and generation

The local `sequence` is an unsigned 32-bit counter shared by both frame types.
It starts at zero after transmitter initialization/restart and is assigned
when a complete wire frame is prepared. It advances modulo `2^32`, so
`0xffffffff` followed by zero is contiguous. Replacing a still-pending record
does not consume a sequence number; this loss is visible through counters.

The receiver compares a valid frame against the next expected local sequence
within a known generation. Any mismatch raises `SEQUENCE_DISCONTINUITY`,
counts the occurrence, discards older pending slots and accepts the current
validated frame as the new baseline. Duplicates and backward values therefore
remain observable transport frames. FNR-005 does not decide whether a sender
message may produce profile output, and never requests retransmission.

`generation` is an opaque 64-bit local transport token carried in every frame.
The ESP adapter forms its initial token from two SDK random values. It advances
the token when foreground observes a change in the separate radio lifecycle
generation and restarts the transmitter. That lifecycle indicator is separate
from saturating diagnostic counters. The token is neither a persistent unique
identity nor a cryptographic credential, and it is not the FNR-006 sender
session field.

For the portable transmitter, supplying a different generation to
`fnr_transport_tx_restart()` is the caller's responsibility. The production
adapter does this. A changed token remains observable even if the first RESET
status was lost. The receiver treats any different token as a lifecycle
change, clears older pending slots, raises `GENERATION_CHANGE`, and establishes
the new sequence baseline; it keeps no historical token acceptance policy.

RESET status is an additional barrier. It retires earlier pending data even
if its token happens to equal the previously observed token. Replacing a RESET
status with a later ordinary status cannot erase the sticky reset event.

## Fixed capacities and ownership

All hot-path storage is fixed. The portable modules require serialized calls;
they are not internally synchronized or lock-free queues.

| Stage | Capacity and owner | Publication and loss rule |
|---|---|---|
| FNR-004 ingress | One owned datagram, at most 250 application bytes plus source metadata | Callback copies a whole record under the existing adapter critical section; newest accepted record replaces the older pending one |
| FNR-004 foreground take | One caller-owned `fnr_datagram` copy per iteration | The copy remains valid independently of subsequent callbacks |
| FNR-005 TX active frame | One immutable wire buffer, at most 277 bytes | Partial FIFO writes advance an offset; ordinary offers never modify its bytes |
| FNR-005 TX pending datagram | One owned record | Newest valid offer replaces the prior pending record and increments `datagrams_replaced` |
| FNR-005 TX pending status | One owned snapshot | Newest ordinary snapshot replaces the pending snapshot; a pending RESET flag survives |
| RP2040 UART RX ring | 1,024 bytes | ISR pushes; foreground copies while excluding the ISR; overflow/fault flushes queued bytes and blocks new publication until foreground observes the gap |
| RP2040 parser segment | 276 nonzero encoded bytes | No partial payload publication; excess input is discarded through the next delimiter |
| RP2040 decoded datagram slot | One complete owned frame | Newest valid datagram replaces the prior datagram slot |
| RP2040 decoded status slot | One complete owned frame | Newest valid status replaces the prior status slot, subject to lifecycle invalidation |

Ordinary receiver replacement affects only the same type's slot. It increments
both that type's replacement and total-drop counters, and raises a replacement
event. A ready status and a datagram can coexist when transport continuity is
intact. Corruption, loss or lifecycle invalidation retires both older slots.
Taking a frame copies it into caller-owned storage outside the receiver.

The transmitter selects a pending status before a pending datagram when the
active frame finishes. A startup/restart emits a zero delimiter and RESET
status before admitting any subsequent pending datagram to the wire. Ordinary
`fnr_transport_tx_offer_status()` calls require input flags zero; callers must
use `restart()` for a reset. An unavailable status drops a pending datagram and
prevents new datagram offers; production radio lifecycle changes use the full
restart path, which also aborts any active suffix.

`peek()` borrows a span of the active wire buffer only until `advance()` or
`restart()`. The UART adapter must advance by the number actually accepted by
the UART. `frames_completed` means all wire bytes were accepted for UART
transmission; it is not an acknowledgement from the RP2040.

## Platform integration

### ESP8285 foreground transmitter

[transport_adapter.cpp](../firmware/esp8266/transport_adapter.cpp) is the sole
production UART writer. It uses the accepted UART configuration in TX-only
mode, disables application/SDK serial debug output, and emits a zero boundary
to recover from preceding ROM/startup bytes. Enabling `DEBUG_ESP_PORT` is
rejected by the production source.

`fnr_esp_transport_task()` observes radio lifecycle changes, queues periodic
status, takes at most one complete record through
`fnr_radio_take_foreground()`, and offers it to the portable transmitter. No
serialization or UART write runs in the ESP-NOW callback. The FNR-004 callback
remains a bounded, profile-neutral producer.

Each task writes at most 32 bytes, bounded further by usable FIFO space. In
the pinned ESP8266 core the free-space query reports `128 - count`, while the
writer treats count 127 as full. The adapter reserves one reported free byte
and writes at most `min(free_bytes - 1, 32)`, returning when `free_bytes <= 1`.
Together with a single writer and disabled debug output, this avoids entering
the core's full-FIFO wait for the offered chunk. There is no flush or blocking
drain loop. Actual target scheduling under load remains part of the physical
campaign.

### RP2040 UART receiver

[transport_adapter.c](../firmware/rp2040/transport_adapter.c) configures UART0
with the accepted GP0/GP1 route and handles RX plus receive-timeout interrupts.
The ISR reads at most 32 UART words per invocation, records framing/parity/
break/overrun observations, and pushes bytes to the fixed ring. It performs no
frame parsing, USB calls or allocation.

The foreground task consumes 64-byte chunks within a 512-byte service budget.
Short interrupt-masked sections serialize ring operations and consumer reads;
COBS/CRC parsing runs outside those sections. A ring gap retires queued bytes
before any following bytes are published. The adapter checks for a gap again
before every public frame/event operation, including a gap arriving while a
copied foreground chunk was being parsed. Earlier parsed slots are invalidated
before a consumer can take them after that observation.

The adapter has a 3,000 ms link-silence watchdog based on completely validated
frames, including status. Delimiters, malformed frames and arbitrary UART
activity do not refresh it. Timeouts are evaluated before old buffered bytes
are processed and before public consumer reads. A foreground service gap of
at least 3,000 ms also retires buffered state, even if getters have observed
an earlier timeout. Timeout/reset retirement clears the software ring, drains
at most one 32-byte hardware FIFO, and puts the parser into discard-through-
delimiter mode. A valid recovered frame re-arms normal liveness monitoring.
Unsigned elapsed-time arithmetic handles the millisecond counter wrap.

This watchdog reports local link/service continuity. It does not expire a
sender session or decide application/profile freshness; those policies remain
with FNR-006.

## Parser validation and recovery

Only a complete delimited segment can become an output record. The parser
checks the encoded limit, COBS structure, minimum decoded header/CRC size,
declared length and exact total size, version, type, CRC, and type-specific
payload constraints. No payload is published unless every check succeeds.
Decoder and unpack-helper outputs remain unchanged on failure. The lower-level
COBS byte utilities may modify their output before reporting failure, but that
temporary storage is never published as a frame.

| Input or transition | Parser/handoff result |
|---|---|
| Initialization | Clear state/counters, raise RESET, discard through the next zero |
| Partial frame or split input | Retain only bounded encoded bytes until a delimiter |
| Multiple frames in one call | Validate each complete segment; apply fixed slot rules |
| Repeated zero delimiters | Count empty delimiters while synchronized; publish nothing |
| Truncated/corrupt delimited frame | Reject, count the reason, retire older pending slots, raise LINK_ERROR; its delimiter restores the next boundary |
| Lost delimiter | Concatenated bytes form one rejected or oversized segment; a later delimiter restores the boundary |
| Malformed COBS, bad CRC, unsupported version/type or false length | Reject without exposing metadata/payload; record the appropriate diagnostic and LINK_ERROR |
| Invalid status flags/state/channel/reserved byte | Reject as invalid payload and raise LINK_ERROR |
| More than 276 encoded bytes | Count one oversized segment, retire older slots, discard through the next zero |
| Explicit parser reset | Retain counters and sticky events, retire partial/pending data, forget token/sequence, raise RESET, discard through the next zero |
| UART/ring interruption | Retire partial/pending data, retain known token, forget sequence, raise LINK_ERROR, discard through the next zero |
| Timeout | Apply the same parser retirement with a distinct TIMEOUT event/counter; the target also retires buffered UART bytes |
| Changed generation | Retire earlier slots, raise GENERATION_CHANGE, accept the validated new baseline |
| Sequence discontinuity | Retire earlier slots, raise SEQUENCE_DISCONTINUITY, accept the validated current baseline |
| RESET status | Retire earlier slots, raise RADIO_RESET, retain the validated status |
| Non-READY status | Retire earlier slots, raise RADIO_UNAVAILABLE, retain the validated status |

After corruption, recovery is the explicit sequence: a delimiter closes or
discards the damaged segment, then a complete valid frame can be accepted.
After an explicit reset or discontinuity, even a suffix that resembles a frame
is discarded through the next delimiter. If a stream never supplies a
delimiter, the receiver remains bounded and discards it; synchronization cannot
be inferred from missing boundaries. The transmitter's startup/restart zero
lets a fresh parser preserve the following complete RESET status. An RP2040
restarting during traffic may intentionally discard the current whole or
partial frame before recovering at its delimiter.

Reset cannot recall bytes already accepted by the ESP UART or records already
copied to a consumer before the event was observed. An old frame already in
flight can arrive before the receiver observes the subsequent reset/generation
barrier. After an observed reset/loss, the implementation retires its old
buffered state and does not replay those slots. It does not claim historical
sender freshness or arbitrary on-wire replay rejection; FNR-006 must define
those semantics above this transport.

## Events, diagnostics and consumer API

Events are sticky bits, separate from the replaceable frame slots. Both take
functions return false while any event is pending. A consumer must first take
and process the event snapshot, then consider status/datagram frames. A new
fault between those operations creates another event barrier, so a failed take
may require event handling on the next iteration. Consumer code must not
discard event bits merely to unblock payload delivery.

| Bit | Event | Meaning |
|---|---|---|
| 0 | `RESET` | Receiver initialization or explicit reset |
| 1 | `LINK_ERROR` | Rejected segment, oversize input or UART/ring discontinuity |
| 2 | `GENERATION_CHANGE` | Valid frame has a different transport token |
| 3 | `SEQUENCE_DISCONTINUITY` | Valid frame differs from the expected local sequence |
| 4 | `RADIO_RESET` | Valid RESET status observed |
| 5 | `DATAGRAM_REPLACED` | Pending receiver datagram replaced by a newer validated frame |
| 6 | `STATUS_REPLACED` | Pending receiver status replaced by a newer validated frame |
| 7 | `RADIO_UNAVAILABLE` | Valid status reports a non-READY state |
| 8 | `TIMEOUT` | Local link-silence or prolonged foreground-service interruption |

The portable APIs are declared in
[transport.h](../shared/include/fnr/transport.h),
[transport_tx.h](../shared/include/fnr/transport_tx.h), and
[uart_buffer.h](../shared/include/fnr/uart_buffer.h):

| API family | Contract |
|---|---|
| `crc16`, `cobs_bound`, `cobs_encode`, `cobs_decode` | SDK-independent checksum and bounded framing utilities |
| `fnr_transport_encode` / `decode` | Encoder includes the terminating zero; decoder accepts one segment without its delimiter |
| `make_datagram` / `unpack_datagram`, `make_status` / `unpack_status` | Explicit layouts and owned copies, validated before publication |
| `fnr_transport_tx_init`, `restart`, `offer_*`, `peek`, `advance` | Foreground-owned bounded transmitter; initialization/restart safely accept `&tx->status` |
| `fnr_transport_rx_init`, `reset`, `discontinuity`, `timeout`, `feed` | Serialized streaming receiver and explicit invalidation operations |
| `fnr_transport_rx_take_events`, `take_datagram`, `take_status` | Sticky event barrier followed by owned frame copies |
| `fnr_uart_rx_init`, `push`, `fault`, `take` | Serialized bounded UART storage with gap-before-bytes publication |

`fnr_transport_rx_feed()` returns the number of valid frames accepted during
that call. This return value remains useful after a diagnostic counter has
saturated. Receiver counters separately record validation reasons, accepted
types, consumption, replacement, total slot drops, resets and discontinuities.
`bytes_discarded` counts nonzero bytes encountered in discard mode, including
the byte that triggers oversize; it is not the total byte length of every
rejected frame.

The RP2040 target exposes these foreground operations through
[transport_adapter.h](../firmware/rp2040/transport_adapter.h):

- `fnr_transport_take_events_foreground()`;
- `fnr_transport_take_datagram_foreground(fnr_transport_frame *)`;
- `fnr_transport_take_status_foreground(fnr_transport_frame *)`;
- `fnr_transport_snapshot_foreground(fnr_rp_transport_diagnostics *)`;
- `fnr_transport_reset_foreground()`.

An optional `fnr_transport_foreground_consume(const fnr_transport_consumer_api *)`
hook receives these same operations after foreground servicing. FNR-006 can
implement that hook or use the explicit foreground seam. With no hook, the
newest slots and sticky events remain pending with fixed storage; the default
firmware adds no consumer, profile interpretation, test injector or synthetic
USB output. A diagnostics snapshot includes parser/ring counters and separate
UART IRQ error observations. Those IRQ counts are not inferred lost-radio-
datagram counts; the ring's fault count also includes software reset/timeout
retirement.

The ESP target exposes `fnr_esp_transport_init()`, `fnr_esp_transport_task()`
and `fnr_esp_transport_snapshot()` in
[its adapter header](../firmware/esp8266/transport_adapter.h). These operations
remain foreground-only; the snapshot provides TX diagnostics without adding
text or a second writer to UART0.

## Qualification boundary

The portable test sources cover independent COBS/CRC vectors, exact layouts,
maximum lengths, chunking, corruption/resynchronization, ownership and reset
properties. Target builds and static integration checks establish what source
was compiled into each firmware. Record their exact run/head/artifact evidence
separately rather than treating this protocol reference as a CI result.

The accepted FNR-002 pins, rate and diagnostic integrity remain inputs. New
production-frame transit, sustained integrity, overload accounting, physical
restart/recovery and ISR/foreground behavior on one representative board remain the
[physical handoff](FNR-005-HARDWARE-HANDOFF.md). This software contract establishes
no new RF channel-isolation result, USB/profile behavior or FNR-006 sender
semantics. The unrelated FNR-004 channel-isolation anomaly does not change the
framing contract or prevent its software preparation.
