# FNR-005 representative-board production transport evidence

**Acceptance is incomplete.** These are 37 passed physical phases on the one
representative DUT, FNR-002 Board A. The failed ESP CPU-reboot attempt is retained;
its cold-boot retry and independent RP reload while ESP traffic continues are
still required. PR #20 and issue #6 must remain open until those gates, final
review, exact-head CI, merge and post-merge verification are satisfied.

Board B is an identical unit of the same hardware design. It was unplugged after
initial read-only inventory and received no programming or FNR-005 qualification.
No comparative campaign is warranted by the observations so far.

## Provenance and observation method

Firmware source is `d6766e8add3a7a659dcb8ea4d87d4f68d628995a`, tree
`b217fa0feeb5f07b7200c68a44b88e8c7e1dcb1d`, on `fnr-005-software-transport`.
The canonical dependency-lock SHA-256 is
`8f72ecec0c03aea93d8eecabf9b584e218726a5c89c87915714173f677471e5c`.
Windows qualification RP SRAM UF2:
`4e6e5c8081a7e9094c684665b5ac9ae415c62b70fda328801c8f92f833581148`.
ESP BIN: `4e2fed0dfe9d9d1ccb5252dcbf6e54d5df01691fce188ea6e04beaaf776e6391`.

[artifact-provenance.json](artifact-provenance.json) records all eight production
and qualification artifacts, tool hashes, deployment/readback records and public
Board A identity hashes. Hardware authority is `techrote/picoWcloneESP8285N08`
at `e8a80ae21e6432d30f6ec6146ff6754605a5d469`. Raw identifiers, host inventory,
captures, local manifests and preserved originals remain outside Git.

The separately named [fixture](../../diagnostics/transport-qualification/README.md)
includes the actual ESP ingress/TX adapters and RP UART/IRQ/ring/parser/consumer
adapters. Normal datagrams traverse the real production serializer and bounded
ownership machinery over GP0/GP1 at the accepted 115200 8N1/no-flow-control rate.
Faults use a bounded separate writer while normal TX is idle, and traverse the
same production receiver. They do not qualify normal TX ownership by themselves.
Opaque deterministic application bytes and exact synthetic source MACs are
checked at the generic consumer; there is no profile interpretation or HID output.

[physical-results.json](physical-results.json) contains every passed case's
source/artifact/DUT identity, time, plan, raw-capture hash, counters and bounded
boundary observations. `test_transport_assessment.py` independently replays
those observations and rejects synthetic mutations. Stored PASS strings alone
are never sufficient. Checksums cover the curated data; synthetic tests are not
new physical evidence.

## Completed physical phases

| Campaign | Observed result |
| --- | --- |
| Smoke, 1/2/31/32/127/128/249/250 bytes | Ten records at each length, exact opaque payload/source metadata, no unexplained errors or drops. Separate slow 32-byte phase consumed four production statuses. |
| Maximum wire boundary | Ten 250-byte records starting at ID 16843011; observed aggregate wire bytes prove one actual 277-byte valid frame. |
| Sustained production framing | 18,002 generated/admitted/taken/transmitted/parsed/handed-off 250-byte records over 600.027468 s, 599 normal statuses, no replacements or unexplained errors. |
| TX overload, 50 maximum datagrams/s | 1,500 generated/admitted/taken; 253 ESP pending replacements; 1,247 transmitted and delivered. No RP consumer loss, abort, corruption or UART/ring loss. |
| Delayed consumer, 2 s | All 120 records transmitted/parsed; 57 RP slot replacements; 63 delivered, exactly IDs 57..119. No ESP replacement or wire corruption. |
| Corruption cases 0..12 | Repeated delimiters, truncation, insertion/removal/change, CRC damage, malformed COBS, CRC-valid unsupported version/unknown type/impossible length, exact encoded-size segment, oversized segment and garbage. No invalid application data escaped; next production telemetry accepted in each phase. |
| Partial parser reset | 29 undelimited encoded bytes actually held; parser reset retired them; recovery telemetry accepted without old delivery. |
| Watchdog, delimiters only | Repeated delimiters did not refresh valid-frame liveness; timeout/event exposed and clean recovery observed. |
| Foreground stalls | 3.5 s stall retired 478 buffered bytes before parsing; two-stall case retired 956 bytes and exposed two timeouts. No buffered normal record refreshed the retired phase. |
| Consumer timeout | No held normal record escaped. One replacement and one timeout retirement counted separately; recovery telemetry consumed. |
| Ring overflow | 60 sent; one overflow/discontinuity, 6,080 ring bytes dropped; 37 records delivered as exact fresh suffix 200023..200059. No UART overrun or parser corruption. |
| UART IRQ pause | 60 sent; two actual hardware overruns/discontinuities and 36 ring bytes retired; 58 delivered as fresh suffix 300002..300059. Hardware wire loss is separate from ring retirement. |
| Active radio lifecycle restart | 180 generated/admitted/taken; 179 completed data frames and one active record aborted; one partial frame rejected as bad COBS. New generation/reset event, 57 RP replacements and one old slot retirement; digest exactly 31-record prefix plus fresh 90-record suffix. |
| Recovery bursts | Separate clean maximum-size bursts after TX overload, consumer delay, ring/UART interruption and lifecycle restart. |

The sustained interval reconciles **5,009,486 wire bytes** at ESP submission,
RP UART receipt, ring pop and parser feed. There are **18,602 validated frames**:
18,002 normal datagrams, 599 statuses and one 202-byte telemetry frame. These
deterministic data frames occupy 276 wire bytes each:
`18002*276 + 599*68 + 202 = 5009486`. Normal payload bytes total 4,500,500.
CRC/COBS/header/parser errors, UART overruns, ring loss, consumer/TX replacement,
duplicate/backward delivery, sequence/generation discontinuities and unexplained
gaps are all zero within this clean interval.

The 115200 8N1 line provides 11,520 wire bytes/s. The conservative budget is
`30*277 + 68 = 8378` bytes/s, or 83,780 bits/s (72.7% of line rate), including a
status per second. The single 600-second representative-board campaign is complete;
host-tool or evidence-only commits do not require another identical soak.

Active lifecycle restart does not recall transmitted bytes. Its safety boundary
begins when RP observes the generation/reset/error event. The 1 sender abort is
not a receiver-drop count. Its partially submitted bytes reconcile separately
from complete frames; pending RP retirement/replacement uses receiver counters.

## Retained failures and remaining gates

[retained-failures.json](retained-failures.json) retains two identity-preflight
rejections before any campaign stimulus and the CPU-reboot failure. The SRAM
observer UID reader was corrected to use the accepted read-only flash operation
with QSPI initialization; the actual Board A UID then passed the guard.

`ESP.restart()` immediately after serial programming produced no new application
telemetry. RP retired held data, exposed a timeout and delivered no stale normal
record, but **ESP recovery failed**. The pinned core documents a matching
[post-upload restart limitation](https://arduino-esp8266.readthedocs.io/en/3.1.2/faq/readme.html#i-have-observed-a-case-when-esp-restart-doesn-t-work-what-is-the-reason-for-that).
That is a hypothesis for this attempt, not a successful recovery result. An
ordinary cold boot using the hardware runbook, followed by the same guarded
SRAM observer load and CPU-reboot retry, is still required. No unknown reset or
download pin may be used. Independent RP reload with ESP traffic continuing is
also still unrun. Volatile counters across a CPU reset must be treated as distinct
epochs; subtracting them or treating sender aborts as receiver losses is invalid.

Software CI at firmware source d6766e8 passed all four jobs in independent
[PR run 38184969657](https://github.com/techrote/faikeow-now-reciever/actions/runs/38184969657)
and [push run 38184964916](https://github.com/techrote/faikeow-now-reciever/actions/runs/38184964916).
Downloaded manifests/source identities/checksums were verified; corresponding
Linux binaries and linked-path outputs are identical between these two runs.
Those Linux outputs are separate from the explicitly hashed Windows images used
physically. Later host/evidence commits need their own exact-head CI.

Previously accepted identity/recovery/pins/UART and FNR-002 general integrity are
inputs. This campaign newly establishes only the completed production transport
claims listed above. It does not resolve FNR-004 channel isolation, implement
FNR-006 sender/session/freshness/profile semantics, establish incoming-radio HID
behavior or mouse/TiltMouse interpretation, provisioning/security (FNR-007), or
reference interoperability (FNR-008). FNR-006 production integration remains
dependency-blocked until FNR-005 acceptance and merge.
