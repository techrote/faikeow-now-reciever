# FNR-006 — Generic receiver envelope, core and relative-mouse profile

This document freezes the software contract implemented by FNR-006. It sits
above FNR-005's board-local framed transport and below profile-owned HID
semantics.

## Version identities

- Platform envelope: **FNR platform v1**
- Message type: **PROFILE_DATA = 1**
- Profile ID: **relative_mouse = 1**
- Relative-mouse schema: **v1**
- Default application freshness timeout: **250 ms**

The ESP-NOW application payload, including this envelope, remains at most 250
bytes for ESP8266-class interoperability.

## Generic platform envelope v1

The wire format is explicit. C structure layout is not an ABI. All multi-byte
integers are little endian.

| Offset | Width | Field | v1 rule |
| ---: | ---: | --- | --- |
| 0 | 1 | platform version | exactly 1 |
| 1 | 1 | profile ID | nonzero; relative_mouse is 1 |
| 2 | 1 | message type | exactly PROFILE_DATA (1) |
| 3 | 1 | flags | exactly 0; no v1 flag bits are defined |
| 4 | 8 | sender session | nonzero u64 restart discriminator |
| 12 | 4 | platform sequence | u32 modulo counter |
| 16 | 2 | profile payload length | exact trailing byte count |
| 18 | N | profile payload | 0..232 bytes generically |

The generic header is 18 bytes, so the maximum profile payload is 232 bytes.
A profile may impose a much smaller exact payload.

### Sender session

A sender must choose a fresh, nonzero 64-bit session value when its application
starts and start its sequence at zero. A platform RNG should be used where
available. The value is a restart discriminator only: it is **not**
authentication, a key, a peer identity, or an anti-spoofing mechanism.

The first valid session observed by a receiver is accepted at any sequence so a
receiver can join a sender already in progress. While the current session is
fresh, a different non-retired session is accepted immediately only when its
sequence is zero. If sequence zero was lost, a different session may establish
after the current application's freshness timeout. The receiver retains a
bounded cache of the four most recently retired session values to reject delayed
packets from recent old sessions.

A session transition neutralizes the active profile before new-session output is
eligible. Because the HID neutral report is a barrier, the transition packet
itself may be accepted for ordering/freshness while its HID output is dropped;
the sender's next full-state packet repairs current button state without replaying
old movement.

### Sequence ordering

Sequence is a u32 modulo counter. One canonical comparator is used:

- delta = candidate - previous modulo 2^32;
- delta == 0: duplicate, reject;
- 0 < delta < 2^31: newer, accept;
- delta == 2^31: ambiguous, reject;
- delta > 2^31: stale/out-of-order, reject.

Accepted gaps are diagnostic only. Missing movement is never requested or
replayed.

## relative_mouse profile v1

Profile ID 1 has an exact four-byte payload:

| Offset | Width | Field | v1 rule |
| ---: | ---: | --- | --- |
| 0 | 1 | schema version | exactly 1 |
| 1 | 1 | complete buttons | bit 0 left, bit 1 right; bits 2..7 zero |
| 2 | 1 | relative X | signed int8, -127..127; -128 invalid |
| 3 | 1 | relative Y | signed int8, -127..127; -128 invalid |

Every accepted packet contains the complete current button state. Relative
movement is event-like and is applied at most once. Duplicate, stale, ambiguous
or unauthorized packets produce no movement and no button transition.

A newer packet after a sequence gap is accepted normally: its movement is
applied once and its complete button state becomes authoritative. There is no
movement retransmission backlog.

The profile feeds the accepted FNR-003 relative-mouse backend. If USB is not
mounted, ordering/freshness may still advance but the movement output is dropped
and local mouse state is kept neutral. It is never replayed when USB returns.

## Application freshness

Application freshness is distinct from FNR-005's 3000 ms internal-link
watchdog.

The default FNR-006 timeout is 250 ms and is configuration data so FNR-007 can
own deployment policy without changing receiver internals. Time comparisons are
wrap-safe over u32 milliseconds and the configured timeout must be in
1..2^31-1 ms.

Only a fully authorized, structurally valid, profile-valid, ordering-accepted
platform message refreshes freshness. The following do not refresh it:

- malformed envelopes;
- wrong or disabled peer;
- wrong profile;
- invalid profile payload;
- duplicate sequence;
- stale/out-of-order sequence;
- half-range ambiguous sequence;
- deferred or retired sessions.

Freshness timeout neutralizes the profile: zero movement and all buttons
released.

## FNR-005 event policy

FNR-005 events are consumed before any owned frame. Board-local transport
sequence/generation are never reused as application sender sequence/session.

| FNR-005 event | FNR-006 action | Neutralize profile | Platform session/sequence |
| --- | --- | --- | --- |
| RESET | invalidate freshness | yes | retain if previously known |
| LINK_ERROR | invalidate freshness | yes | retain |
| GENERATION_CHANGE | invalidate freshness | yes | retain |
| SEQUENCE_DISCONTINUITY | invalidate freshness | yes | retain |
| RADIO_RESET | invalidate freshness | yes | retain |
| RADIO_UNAVAILABLE | invalidate freshness | yes | retain |
| TIMEOUT | invalidate freshness | yes | retain |
| DATAGRAM_REPLACED | diagnostic only | no | retain |
| STATUS_REPLACED | diagnostic only | no | retain |

Retaining the last accepted platform sequence across board-local link faults is
intentional: clearing it would allow an old same-session RF datagram to become
acceptable merely because the UART layer resynchronized. A genuine sender
restart is represented by the independent sender-session field.

FNR-005 status frames remain transport/radio diagnostics; FNR-006 does not
interpret profile data from them.

## USB lifecycle

FNR-003 remains authority for descriptors and USB report ownership. FNR-006
observes the selected HID device state.

- unmount/suspend/reset causes FNR-003 to discard pending output and neutralize;
- FNR-006 also invalidates application freshness on observed USB state changes;
- packets while USB is unavailable do not accumulate movement;
- mount/resume starts behind FNR-003's neutral-report barrier;
- no relative movement is replayed after reconnect.

## Configuration seam

FNR-006 defines a bounded in-memory configuration seam containing:

- whether a peer is enabled;
- one allowed six-byte source MAC for v0.1;
- platform version;
- selected profile ID;
- application freshness timeout.

The shipping/default configuration is deliberately **unprovisioned and
fail-closed**: peer admission is disabled, so RF datagrams cannot drive HID
output merely because the receiver firmware boots.

FNR-007 owns persistent peer/channel/key/profile provisioning and recovery. It
must use this seam rather than reimplement sender/session/order logic.

## Golden vectors

### Representative relative_mouse packet

Logical values:

- platform v1;
- profile ID 1;
- PROFILE_DATA;
- flags 0;
- session 0x0123456789abcdef;
- sequence 0x89abcdef;
- profile length 4;
- schema 1;
- both buttons;
- X = -5;
- Y = +7.

Exact 22 application bytes:

```text
01 01 01 00
ef cd ab 89 67 45 23 01
ef cd ab 89
04 00
01 03 fb 07
```

### relative_mouse extrema payload

Both buttons, X=-127, Y=+127:

```text
01 03 81 7f
```

The native test suite checks these exact bytes, maximum envelope length,
endianness, sequence wrap/half-range behavior, session transitions, freshness,
FNR-005 invalidation, USB lifecycle and the real
FNR-005-frame -> FNR-006-core -> relative_mouse -> FNR-003-HID chain.

## TiltMouse compatibility

FNR-006 was reconciled against
`techrote/ESP32-QMI8658C-TiltMouse@bd610235367de52d41631b0242e6a8a7e6f8dd9a`,
including TM-005A's accepted logical-report contract and TM-005B / #16.

TM-005B must map its transport-neutral CURRENT logical report to the exact
platform/profile bytes above, generate a fresh nonzero u64 session on sender
application restart, start sequence at zero and monotonically advance sequence
for reports offered to the wireless transport. It must not add private receiver
framing or stale-motion retransmission.

## Explicit non-claims

FNR-006 does not establish:

- persistent provisioning, keys or authenticated peers (FNR-007);
- physical ESP-NOW interoperability or RF behavior (FNR-008);
- physical USB-host acceptance (FNR-008);
- the TiltMouse ESP-NOW transmitter implementation (TM-005B);
- security properties for the session discriminator;
- keyboard/gamepad profile behavior.

Those scopes remain separate.
