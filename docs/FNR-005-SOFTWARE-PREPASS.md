# FNR-005 software prepass evidence and handoff

## Status

Owning issue: [#6 / FNR-005](https://github.com/techrote/faikeow-now-reciever/issues/6).
This is a software-only implementation record dated 2026-10-11.

**Production software is complete for review; physical qualification tooling
and the production framing campaign remain outstanding.** The branch supplies
the codec, bounded producer/transmitter, UART receiver, parser, owned consumer
seam and software qualification. No physical action or new physical acceptance
was performed. Keep #6 open.

The exact final source commit, PR and CI run belong in the PR body and the issue
handoff comment. Those identify the reviewed revision without embedding a
self-referential commit hash in its own source tree. The following documents
define the deliverable and the remaining work:

- [Internal protocol v1 and production APIs](FNR-005-TRANSPORT.md).
- [Exact future physical qualification campaign](FNR-005-HARDWARE-HANDOFF.md).
- [Build commands, locked dependencies and CI gates](05-BUILDING.md).

## Reconciled starting state

Live GitHub state was read before implementation, including current `main`,
issue bodies/comments, tracker #1, branches, PR history, commits, CI and project
authority. The historical reference in the request happened to remain current:

| Item | Reconciled input |
|---|---|
| Receiver `main` | `e427f369f000a3df080c462592228a6836d79742` |
| Receiver source tree | `9048e4b78b64b53d46e95c5bf3d21903140525ae` |
| Baseline CI | [38145596148](https://github.com/techrote/faikeow-now-reciever/actions/runs/38145596148), all four jobs passed |
| Prior PRs | #12 through #18 merged; no existing FNR-005 PR or implementation branch |
| Issue #6 | Open, no comments at the initial reconciliation |
| Tracker #1 | FNR-001, FNR-002 and FNR-003 accepted; FNR-004 software merged; FNR-005 next dependency-ready package |
| FNR-004 exception | Issue #5 remains open for the separate reverse RF channel-isolation anomaly; its stable foreground API is usable here |
| FNR-006 | Issue #7 requires FNR-005 merged before production implementation/integration |

Read authority included `AGENTS.md`, `RAG.md`, `README.md`, docs 00 through 08,
the FNR-004 ingress contract, issue #6 and FNR-002 board/evidence material.
General hardware authority is
[techrote/picoWcloneESP8285N08](https://github.com/techrote/picoWcloneESP8285N08),
at the qualified reference commit
`8d48cdde71ccba824436635858265cb31c4ed88f`. Original FNR-002 acceptance entered
through PR #17 / `2777c90bfb13194feaeb6f7410c8029417af8da4`.

The accepted physical inputs are RP2040 UART0 GP0 TX to ESP RX, GP1 RX from ESP
TX, and 115200 baud / 8N1 / no flow control. No new pin, reset, flow-control,
recovery, radio or USB fact was inferred from a build or simulator.

## Implemented software

`shared/src/transport.c` is SDK-independent C17. It implements COBS, fully
specified CRC-16/CCITT-FALSE, explicit little-endian field encoding, strict
version/type/length/payload validation, streaming recovery and bounded owned
handoff. Decode publishes only after all required checks succeed. The transport
does not interpret application/profile bytes.

The v1 header is 16 bytes: version `u8`, type `u8`, payload length `u16`, local
sequence `u32`, local transport generation `u64`. Only `RADIO_DATAGRAM` and
`RADIO_STATUS` exist. A datagram carries its six-byte source MAC and 1..250
opaque application bytes. Status has a fixed 48-byte diagnostic payload.
CRC is two bytes, little-endian, covering the complete header and payload.
The derived maxima are **274 decoded bytes, 276 COBS bytes and 277 wire bytes**
including the delimiter. Startup/restart begins with a zero delimiter.

`shared/src/transport_tx.c` owns one immutable active frame, one replaceable
pending datagram and one pending status snapshot. The ESP foreground takes the
actual FNR-004 `fnr_radio_take_foreground` copy, stages it and writes at most
32 bytes per loop, limited by usable FIFO space. Pending data use newest-wins
replacement; the partial active frame never changes. Reset retires software
ownership, changes local generation and schedules delimiter plus reset status.
Periodic status is offered every 1000 ms. The ESP-NOW callback still performs
only bounded ingress work; it never serializes or writes UART data.

The RP2040 adapter configures the established UART0 link. Its bounded IRQ reads
at most 32 bytes into a fixed 1024-byte ring. Foreground parses at most 512 bytes
per service call in 64-byte chunks. Ring overflow/error invalidates continuity.
The parser owns one newest datagram slot and one newest status slot; sticky
events form a mandatory barrier before either take can succeed. Late IRQ errors
are synchronized before consumer access. A 3000 ms validated-frame watchdog
retires old buffered data before parsing or consumer retrieval; junk UART input
does not refresh liveness.

Local generation/sequence and link events expose transport discontinuity.
They are not sender session identity, authentication, payload freshness or
profile state. FNR-006 is not implemented. The RP transport has no direct
relative-mouse dispatch and the default consumer is absent. Its public owned-copy
API remains linked for the later receiver core. Default radio provisioning
remains fail-closed.

## Native and target qualification

Local native qualification used GCC/G++ 13.3, CMake 3.31.6 and Ninja 1.11.1.
Project code retains strict warnings (`-Wall -Wextra -Werror -Wconversion
-Wshadow`). Both sanitizer configurations ran all nine CTest groups successfully:

| Gate | Result and relevant scope |
|---|---|
| Native UBSan | 9/9 passed, non-recovering undefined-behavior sanitizer |
| Native ASan + UBSan | 9/9 passed; local LeakSanitizer disabled because this container restricts process inspection; no local leak-check claim |
| FNR-003 regression | Existing HID framework, descriptors, ownership, relative-mouse and lifecycle tests passed |
| FNR-004 regression | Existing ingress admission, copy ownership, newest-wins loss and restart tests passed |
| Portable codec/parser | Independent golden/reference vectors, hostile stream recovery, strict metadata and ownership tests passed |
| Producer/UART pipeline | Partial/zero writes, backpressure, slot replacement, restart, counter saturation, ring loss and 30,000-step real-ingress pipeline accounting passed |
| Actual ESP production adapter | Compiled against minimal software SDK fakes; FIFO edge, single foreground writer, owned ingress, lifecycle/counter wrap and heartbeat tests passed |
| Actual RP production adapter | Compiled against minimal software SDK fakes; bounded IRQ/task, late IRQ error, queued-data timeout, repeated service stall, clock wrap and owned consumer tests passed |
| RP2040 cross-build | Locked Pico SDK 2.2.0, TinyUSB 0.18.0 and Arm GCC 13.3.1; UF2 and ELF produced |
| ESP8285 cross-build | Locked ESP8266 Arduino 3.1.2, Xtensa GCC 10.3 and makeEspArduino 6.7; BIN and ELF produced |
| Production linked-path checks | 13 RP transport/consumer/regression symbols and 10 ESP ingress/transmitter/codec symbols passed |
| Historical diagnostic regression | All five RP diagnostic configurations and both ESP UART/RF diagnostic images compiled; no diagnostic image was deployed or run on a board |
| Offline safety-tool regression | 18 guard tests plus the pinned esptool 4.8.1 / pyserial 3.5 fake-port integration test passed; no physical port access |
| Dependency integrity | Both lane identity/extracted-tree checks passed; lock unchanged |
| Preserved evidence | All 14 original FNR-002 checksum entries passed; evidence unchanged |

The deterministic codec corpus includes 4,100 independent COBS reference cases,
750 datagram cases covering every legal application length, known CRC/golden
byte-order vectors, the actual maximum 277-byte wire frame, every split/chunk,
concatenation, and insertion/deletion/bit changes at every byte of the fixture.
Malformed COBS, CRC-valid illegal headers/status, false lengths, oversized input,
truncation, unsupported version/type, resets and event/slot ownership are covered.

With fixed seed `0xd6e8feb8`, the hostile-stream corpus processes **12,000 streams,
3,023,683 bytes and 2,449 independently validated accepted frames**. Every stream
is followed by an explicit resynchronization boundary and a known valid frame.
Checks assert fixed-buffer invariants, legal accepted metadata and bounded
progress. This is reproducible property-style testing, not an exhaustive proof
against all possible CRC collisions or streams.

The unchanged dependency-lock SHA-256 is:

```text
8f72ecec0c03aea93d8eecabf9b584e218726a5c89c87915714173f677471e5c
```

Local ESP artifacts used an explicit precommit source label and are build
evidence, not release/physical-acceptance artifacts. CI explicitly checks out
the actual PR/push head in every lane and records that full source SHA, lock
hash, compiler identity, configuration, artifact hashes and
`physical_acceptance: false` in manifests. Use the exact successful final-head
CI run recorded in the PR/issue when selecting qualification artifacts.
This document does not substitute a local draft build for that final-head gate.

## Review findings and fixes

Parallel reviews examined worst-case COBS expansion, capacity arithmetic,
signed/unsigned conversion, header/CRC validation, hostile-stream transitions,
reset/generation boundaries, slot lifetime, producer backpressure, ESP callback
ownership and RP interrupt/foreground ordering. GCC `-fanalyzer` also passed on
the three portable transport/ring implementations.

Substantive findings were fixed and covered by targeted regressions:

- The pinned ESP core reports `128 - FIFO occupancy` free bytes, but its writer
  waits at occupancy 127. The production writer now reserves one reported free
  byte; `availableForWrite() <= 1` performs no write. This preserves the bounded
  foreground behavior at the exact FIFO boundary.
- RP buffered frames could otherwise be parsed after a long service stall and
  falsely refresh the watchdog at the current time. Timeout/stall retirement
  now happens before parsing, including a second stall while already timed out.
  Consumer access also checks expiry so a stale pending record cannot bypass it.
- Transmitter initialization/restart now snapshots an aliased status argument
  before clearing its state. Ordinary periodic status offers reject RESET flags;
  only the lifecycle path creates reset status and preserves its priority.
- Fixed-seed test generation no longer depends on C++ argument evaluation order.
  Independent random draws are sequenced, preserving identical corpus coverage
  across compilers.

Review also made the physical handoff explicit about bytes already accepted by
UART: they cannot be recalled, and a later zero may complete a previously sent
frame body before the receiver observes a new generation/reset. No-stale replay
is guaranteed after the observed boundary; it does not revoke previously
delivered data. `tx_aborted` records software retirement, not measured receiver
loss. No unresolved production-code finding remained at handoff.

## Remaining acceptance and next action

| Evidence class | State |
|---|---|
| FNR-002 physical pins/configuration/basic board UART integrity | Already accepted; reused unchanged |
| FNR-005 protocol/native correctness/target compilation/static integration | Established by this software prepass and final-head CI when linked in the handoff |
| Real production COBS/CRC16 traversal, sustained integrity and physical reset/recovery | Not established |
| New RF behavior, channel-isolation resolution, FNR-006 or integrated TiltMouse behavior | Not established; outside this implementation |

The next qualification agent must first reconcile the PR/head and
[physical handoff](FNR-005-HARDWARE-HANDOFF.md), then prepare a separately named,
bounded production-path injector/observer using the existing adapter APIs.
Cross-build and review that instrumentation before any separately authorized
deployment. Existing FNR-002 SLIP/CRC32 commands do not exercise this protocol.

The pending per-board campaign records exact source, artifacts, board identities,
stimulus and counters. It includes 1..250-byte/source/status smoke; at least
600 seconds of clean production framing at an initial 30 maximum-size
datagrams/s; deliberate overload at 50/s; delayed consumers; byte corruption,
truncation and oversize; UART/ring discontinuities; the 3000 ms watchdog and
foreground-stall cases; parser/ESP/RP resets; and post-boundary healthy recovery.
Every physical checklist item remains unchecked until that evidence exists.

A software merge can be assessed under [04-VALIDATION.md](04-VALIDATION.md)'s
precise-open-hardware-handoff rule, with the exact-head green CI, current-main
reconciliation and resolved review requirements of
[07-EXECUTION-PROTOCOL.md](07-EXECUTION-PROTOCOL.md). It is not physical acceptance
and must not automatically close #6. The prepass leaves a reviewable branch/PR
rather than silently merging an incomplete acceptance package.

FNR-006 may begin software design/preparation against this pinned generic
contract. Its production integration still requires FNR-005 merged, as issue #7
states. It must not interpret transport sequence/generation as sender freshness
or bypass the mandatory consumer event barrier.
