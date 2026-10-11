# FNR-005 production transport: physical qualification handoff

## Status and scope

Owning issue: [#6 / FNR-005](https://github.com/techrote/faikeow-now-reciever/issues/6).

This document defines future, separately authorized physical work. The FNR-005
software prepass performs no board identification, serial-port access, flashing,
reset/button operation, USB enumeration, UART experiment or RF test. It does
not establish any new physical acceptance. Every campaign item below is pending.
Native tests, target builds and linked-symbol checks remain software evidence.

The frozen wire, ownership and restart contract is
[FNR-005-TRANSPORT.md](FNR-005-TRANSPORT.md). Use that contract when interpreting
the cases and counters below.

The starting receiver reference was
`e427f369f000a3df080c462592228a6836d79742`. Before qualification, reconcile the
actual reviewed branch/head, issue comments, `main` and CI; record the exact
commit and artifact identities tested. Do not deploy a historical reference
merely because it appears in this document.

## Accepted inputs and authority

General board facts and programming/recovery procedures belong to
[techrote/picoWcloneESP8285N08](https://github.com/techrote/picoWcloneESP8285N08),
at the qualified documentation baseline
`8d48cdde71ccba824436635858265cb31c4ed88f`:

- [Board contract](https://github.com/techrote/picoWcloneESP8285N08/blob/8d48cdde71ccba824436635858265cb31c4ed88f/docs/BOARD-CONTRACT.md).
- [Programming reference](https://github.com/techrote/picoWcloneESP8285N08/blob/8d48cdde71ccba824436635858265cb31c4ed88f/docs/DEVBOARD-PROGRAMMING-REFERENCE.md), especially resource ownership, build selection and performance interpretation.
- [Operating runbook](https://github.com/techrote/picoWcloneESP8285N08/blob/8d48cdde71ccba824436635858265cb31c4ed88f/docs/DEVBOARD-OPERATING-RUNBOOK.md), especially deployment, task-specific verification and handoff.

Receiver firmware, dependency locks, protocol definitions and application
acceptance remain owned here. See [03-HARDWARE.md](03-HARDWARE.md) for that split.
The original FNR-002 acceptance merge was
`2777c90bfb13194feaeb6f7410c8029417af8da4` (PR #17).

| Already accepted on each characterized board | Consequence for this campaign |
|---|---|
| RP2040 B2 with 2 MiB flash; ESP8285N08 with 1 MiB embedded flash | Use the qualified two-image build/deployment configuration |
| RP2040 UART0 GP0 TX to ESP UART RX; GP1 RX from ESP UART TX | Use only these established UART pins; reserve GP0/GP1 |
| 115200 baud, 8N1, no flow control | Keep this configuration; higher rates are outside this campaign |
| Independent backups, restoration, physical-button recovery and diagnostic cold boots | Reuse preserved identities and backup pairs; do not repeat full restoration solely for this task |
| Independent 600-second diagnostic UART integrity and fault/recovery runs | Established board capability; production FNR-005 framing still requires its own evidence |

RP-controlled radio reset/download pins and RTS/CTS routing remain unknown.
Do not drive guessed pins, assume USB DTR/RTS controls radio reset, request a
multimeter check for the established link, or use historical COM numbers as
identities. A later hardware session must obtain fresh topology plus private
RP flash UID/radio MAC before the relevant operation. This prepass leaves the
physical fixture state unobserved and unchanged.

## Production path and required observation tooling

Qualify the production path end to end:

```text
owned FNR-004 foreground datagram
    -> FNR-005 serializer and bounded transmitter
    -> established onboard UART
    -> production RP2040 UART receive buffering
    -> FNR-005 parser and bounded consumer handoff
```

Observe `RADIO_STATUS` as well as `RADIO_DATAGRAM`. Source MAC and raw application
bytes must survive unchanged; transport metadata must match the frozen v1
contract. The test must not interpret profile fields or generate HID input.

Existing FNR-002 link diagnostics use SLIP plus reflected CRC-32/ISO-HDLC.
`link_capture.py`, `uart_fault_capture.py`, `RUN10` and `RUN600` exercise those
diagnostics. They do not test the production COBS/CRC-16 protocol and cannot be
relabelled as FNR-005 acceptance.

The first remaining software action for the hardware pass is to inspect the
final production APIs and prepare a separately named, bounded injection and
observation harness if none exists. Cross-build and review it before deployment.
There is no production-framing physical capture command promised by this
handoff. Do not invent command-line compatibility with the old diagnostics.

The harness must reuse the production serializer, transmitter, UART adapter,
parser and handoff code. Deterministic injection at the foreground datagram seam
can isolate UART qualification from RF behavior. Also retain an integration
check that the production loop consumes the actual FNR-004 owned-record seam.
Record precisely which boundary a test injects at; injection is not physical
ESP-NOW reception evidence. No UART writes may originate in a receive callback.

Observation must expose payload verification and the relevant counter/state
snapshots without inserting text into UART0. If an instrumented firmware adds
an observer or injector, give it separate artifact names/hashes, record the
source/configuration differences, and distinguish its evidence from any smoke
performed using the final production artifacts. Instrumentation must not replace
the UART driver, buffering, parser or ownership policy being qualified. The
normal production USB profile must not be silently changed into a CDC fixture.

## Wire-rate budget

For application length `A`, derive raw size `R` from the frozen header, metadata,
application bytes and CRC. A safe COBS capacity for this encoder convention is:

```text
encoded_capacity(R) = R + floor(R / 254) + 1
wire_capacity(R)    = encoded_capacity(R) + 1 zero delimiter
serial_capacity    = 115200 / (1 start + 8 data + 1 stop) = 11520 bytes/s
wire_time_seconds  = actual_wire_bytes / 11520
```

The software design budget is a 16-byte header, up to 256 payload bytes
(six-byte source MAC plus at most 250 application bytes), and two CRC bytes:
`R_max = 274`, COBS capacity `276`, and maximum wire capacity `277` bytes.
Confirm these against the final protocol constants and golden vectors before
running the campaign; if the frozen layout changes, recalculate the campaign
budget. The maximum-frame wire time at this budget is about 24.05 ms, with a
calculated ceiling of about 41.59 such frames/s before status/start delimiters,
software service time or other traffic. These are arithmetic bounds, not
measured throughput guarantees.

Budget the complete stream, including status frames and synchronization bytes:

```text
sum(actual_wire_size_i * frames_per_second_i) + extra_delimiter_bytes_per_second
    < 11520
```

Use 30 maximum-size datagrams/s for the initial clean sustained case, reserving
the remaining budget for status and service margin. At the 277-byte bound that
is 8310 wire bytes/s before status. Confirm the actual scheduler's status cadence
and byte budget; reduce the offered rate if necessary. Do not increase UART baud
to hide a failed assumption.

Use at least 50 maximum-size datagrams/s for a deliberate overload case. Even
250 application bytes at 50 Hz exceed UART capacity before metadata/framing.
Expected bounded loss must follow the documented drop rule and be counted; it
is separate from clean-run integrity. Earlier FNR-002 RF runs at that offered
payload rate do not prove loss-free production UART forwarding.

## Representative-DUT campaign

Run the following on one representative characterized board. The two available units are identical hardware designs. Do not repeat the full campaign on the second unit unless a plausible unit-specific anomaly requires comparison. A short optional smoke check is supplementary. Start each case
with recorded counter/state snapshots, a deterministic stimulus seed and an
explicit expected outcome. Include a drain interval and final snapshots so
pending/in-flight data are accounted for. Stop on unexplained corruption or
accounting failure; preserve failed captures instead of replacing them with a
later pass.

| Case | Stimulus and observation | Required result |
|---|---|---|
| Startup and minimum frame smoke | Start each MCU independently; send valid status and minimum permitted datagrams after arbitrary startup bytes | No delivery before full validation; startup/reset state observable; clean next frame accepted |
| Payload/metadata smoke | Application lengths 1, 32, 128 and 250; zero, `0xff`, incrementing and fixed-seed patterns; distinct valid source bytes | Exact source, length and application bytes at consumer; correct v1 metadata and CRC/framing checks |
| Sustained clean traffic | At least 600 active seconds at the reviewed safe rate, including maximum-size data and the production status cadence | No unexplained corruption, UART/ring errors, gaps, drops or metadata mismatch; final accounting reconciles |
| Whole-frame bursts | Bounded bursts at several lengths, then drain; observe back-to-back status/data | Whole-frame ownership preserved; any expected replacement/drop follows policy and is counted |
| Deliberate overload | At least 50 maximum-size datagrams/s for a recorded interval; then return to safe rate | Bounded storage, deterministic loss accounting, no byte interleaving or partial-frame mutation; healthy recovery |
| Delayed consumer | Pause consumer retrieval long enough to fill its slot while parser keeps running; resume | Documented replacement/drop behavior, exact counters, no dangling storage, no stale queued backlog |
| Corrupt byte stream | Insert, remove and change bytes; damage CRC and COBS; unsupported version/type and impossible length; repeated delimiters | Rejected data never reach consumer; rejection diagnostics match observable events; next delimited valid frame accepted |
| Truncation and oversized stream | Cut frames at representative offsets, exceed encoded capacity, then delimiter and valid frame | Fixed bounds respected, discard/resynchronization deterministic, no indefinite desynchronization |
| UART interruption/error | Interrupt active traffic; exercise/report UART framing, parity, break and overrun paths where the fixture can do so safely | Error becomes observable, affected partial/pending data invalidated according to contract, valid post-boundary recovery |
| Link watchdog and foreground stalls | At least 3000 ms without a complete valid frame while junk bytes continue; separately stall foreground for more than 3000 ms with a few queued frames and no ring overflow, then repeat while already timed out | TIMEOUT is observable; queued old bytes cannot refresh liveness or release old pending records; a new delimiter and complete valid frame restore healthy operation |
| IRQ ring overflow | Deliberately stall foreground draining while IRQ RX continues until the ring is full; then resume | Overflow counted, damaged continuity observable, no fragment or pre-loss stale record emitted as fresh; bounded recovery |
| Explicit parser reset | Reset with a partial frame and with an unconsumed complete record; then send suffix/noise and a fresh frame | Parser and owned pending state clear; reset event available; no pre-reset record replay |
| ESP restart during traffic | Restart radio application with pending data and during partial emission; separately exercise a qualified whole-ESP restart | Restart/status boundary observable; old pending data discarded; next valid production frames recover without stale replay |
| RP2040 restart during traffic | Restart RP while ESP continues, including a partial frame; resume production parser/consumer | Prior RP state cleared, first partial fragment rejected/resynchronized, fresh complete frames usable |
| Recovery with delayed consumer | Combine an unconsumed frame with link loss or either restart, then resume consumer after fresh traffic | Reset/failure evidence survives data replacement; no old-generation pending handoff escapes after the receiver observes the reset/generation/error boundary; healthy steady state restored |

Physical fault injection is limited to the qualified pins and recovery methods.
If a particular electrical UART error cannot be induced safely, retain its
native-test evidence and mark that physical subcase untested; do not fabricate
an electrical observation. An instrumented error-path trigger establishes the
software response on target, not the physical cause of an error. Record these
distinctions per case.

Compare restart behavior with the frozen local transport identity/sequence
contract. Sequence gaps are diagnostics. This campaign must not implement or
accept FNR-006 sender/session freshness, payload semantics or HID neutralization.

Bytes already accepted by the ESP UART cannot be recalled. A new startup zero
can complete an old frame body that was already in flight before the receiver
observes the new reset/generation boundary. The no-stale-replay guarantee starts
at that observed boundary; it cannot revoke a record already delivered. Likewise,
`tx_aborted` counts software-retired records, not proven receiver drops. Include
this in-flight distinction in restart accounting instead of requiring impossible
retroactive invalidation.

## Evidence identity and accounting

Record the following before each deployment/case as applicable:

- Receiver source branch, full commit, tree cleanliness and any instrumentation patch hash.
- PR head and relevant successful CI run; exact dependency-lock hash and compiler/build configuration.
- Both firmware artifact names and SHA-256 hashes, manifest hashes, internal protocol version and harness/tool hashes.
- Hardware-reference commit, board label and guarded private RP UID/radio MAC/topology identity; public summaries must not expose private backups or identifiers.
- Native host OS, UTC timestamp, stimulus seed, offered cadence, payload schedule, status cadence, active/drain duration and injected fault schedule.
- Exact deployment/readback records; a cold-boot claim requires an actual cold boot observation.
- Before/after counter snapshots, accepted payload digest/sequence log, final pending/in-flight state and capture hashes.

Map actual implementation counter names into the result. Preserve separate
counts for offered stimuli, FNR-004 admissions/rejections/replacements, foreground
takes, FNR-005 pending replacements/reset discards, frames/bytes transmitted,
UART hardware errors, IRQ ring losses, parser rejects by reason, validated
frames, consumer replacements/reset discards and delivered records. Do not add
unrelated counters from different stages as if they represented distinct loss.

For a case without resets or saturation, reconcile each stage with its next
stage plus its explicit drops and change in pending/in-flight ownership. Count
status and data frames separately; the local sequence may cover both. Distinguish
bytes accepted by a UART API from bytes physically emitted, and emitted frames
from consumer delivery. Saturating counters are lower bounds after saturation;
use a fresh bounded measurement interval or additional harness accounting so
saturation cannot conceal losses.

Clean cases require zero unexplained mismatch/loss and matching final state.
Overload/reset/fault cases require explicit expected-loss categories and healthy
recovery; do not waive unexplained corruption because some loss was intentional.
Any stale delivery after the documented reset/loss boundary is a failure.

Preserve bulky raw captures and complete device identities privately. Commit
concise curated representative-DUT results and a checksum manifest under an issue-owned
evidence directory. Preserve the original FNR-002 evidence and checksums unchanged.

## Acceptance checklist and programme handoff

These checkboxes are deliberately unchecked. They represent future physical
evidence, not a record of the software prepass.

- [ ] Final source/artifact/protocol/reference identities recorded for the representative DUT.
- [ ] Production serializer/UART/parser/handoff path established by the observation method.
- [ ] Minimum and maximum payload/metadata smoke passes on the representative DUT.
- [ ] Sustained production integrity at a documented wire budget passes on the representative DUT.
- [ ] Overload and delayed-consumer cases demonstrate bounded loss with reconciled counters.
- [ ] Corruption, truncation and oversized input recover at a documented frame boundary.
- [ ] UART errors and IRQ ring overflow are observable and cannot release damaged/stale handoff state.
- [ ] Parser reset, ESP restart and RP restart clear old pending ownership without stale replay.
- [ ] Reset/failure notifications survive delayed consumption and data-slot replacement.
- [ ] Healthy post-fault/restart traffic recovers; failures and untested subcases remain explicit.
- [ ] Curated evidence and raw-capture hashes reviewed; exact PR/main state reconciled.

The FNR-004 reverse channel-isolation anomaly remains separately owned by
[issue #5](https://github.com/techrote/faikeow-now-reciever/issues/5). This transport
campaign does not establish channel isolation, new RF capability, integrated
USB behavior or TiltMouse compatibility. FNR-002's accepted identity, recovery
and UART facts remain accepted throughout.

Keep #6 open while its required acceptance is incomplete. A software merge may
be assessed under [04-VALIDATION.md](04-VALIDATION.md)'s precise-open-handoff rule
only with the normal reviewed-head/CI requirements from
[07-EXECUTION-PROTOCOL.md](07-EXECUTION-PROTOCOL.md); it is not physical acceptance.
Record the final branch/head, established software evidence, unrun campaign
items and exact next action in the issue handoff. Do not close the issue through
an automatic PR closing phrase while this gate remains open.

FNR-006 may perform software design/preparation against the exact reviewed
FNR-005 contract. [Issue #7](https://github.com/techrote/faikeow-now-reciever/issues/7)
explicitly requires FNR-005 merged before production implementation/integration;
an open preparatory PR does not satisfy that dependency. Keep FNR-006 envelope,
profile dispatch and HID behavior outside this transport campaign.

At the end of a later hardware session, stop test traffic, close handles and
record the actual firmware/fixture state. Do not restore or reflash solely to
match a historical snapshot. The immediate next step left by this software
prepass is review/cross-build of production-path qualification tooling, followed
by a separately authorized hardware session using the dedicated board runbook.
