# FNR-005 representative-board production transport evidence

**The representative-DUT physical campaign passes.** These are 44 passed
physical phases on FNR-002 Board A. They include ESP CPU reboot after ordinary
cold boot, independent RP reload during active ESP traffic, both partial and
complete pending parser reset, continuing-invalid-input watchdog, and clean
post-fault bursts. Failed attempts are retained. Final exact-head CI, squash
merge, post-merge main verification and issue/tracker closure are separate
programme gates, recorded in PR #20 and issue #6.

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
| Parser resets | 29 undelimited encoded bytes retired by explicit reset. Separate complete-record case held one unconsumed record, reset retired it, and no old payload escaped. One automatic telemetry frame was discarded through the reset delimiter (201 nonzero bytes counted), and the next requested telemetry validated/consumed. |
| Watchdog, invalid delimiters | Original delimiter-only case and four eight-delimiter injections 0.85 s apart: invalid input continued across 3000 ms without refreshing valid-frame liveness; timeout/event and clean recovery observed. |
| Foreground stalls | 3.5 s stall retired 478 buffered bytes before parsing; two-stall case retired 956 bytes and exposed two timeouts. No buffered normal record refreshed the retired phase. |
| Consumer timeout | No held normal record escaped. One replacement and one timeout retirement counted separately; recovery telemetry consumed. |
| Ring overflow | 60 sent; one overflow/discontinuity, 6,080 ring bytes dropped; 37 records delivered as exact fresh suffix 200023..200059. No UART overrun or parser corruption. |
| UART IRQ pause | 60 sent; two actual hardware overruns/discontinuities and 36 ring bytes retired; 58 delivered as fresh suffix 300002..300059. 244 submitted bytes were not read by UART IRQ; 36 additionally retired ring bytes are a separate count. |
| Active radio lifecycle restart | 180 generated/admitted/taken; 179 completed data frames and one active record aborted; one partial frame rejected as bad COBS. New generation/reset event, 57 RP replacements and one old slot retirement; digest exactly 31-record prefix plus fresh 90-record suffix. |
| ESP CPU reboot | After confirmed ordinary cold boot, a complete old slot was held. Reboot produced 20 visible UART framing batches/one break batch, 79 ring bytes retired and 20 link discontinuities before new-generation RESET status sequence 0. Old slot retired once; no old delivery or further fault after that boundary. New telemetry sequence 1 accepted, followed by clean ten-record maximum-size burst. |
| Independent RP reload | ESP generated/admitted/taken/transmitted all 1,800 records and 59 statuses (501,014 submitted bytes). RP held an old slot before ROM entry. New SRAM observer retained ESP generation and delivered exact suffix 600353..601799: 1,447 records, 48 statuses and telemetry; no new observer error/replacement/loss. |
| Recovery bursts | Separate clean maximum-size bursts after TX overload, consumer delay, ring/UART interruption, lifecycle restart, CPU reboot, RP reload and final parser/watchdog cases. |

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

## Reset accounting, retained attempts and limitations

Independent RP reload has distinct receiver epochs. The old pre-ROM snapshot
observed 39 normal records: 31 consumed, seven replacements and one held slot.
The new observer delivered 1,447 fresh records. Another 314 source records fall
between those recorded epochs; the old snapshot is not a final old-epoch count.
They are intentional/unobserved during reset/reload, not inferred receiver or
wire drops. The source remained active without reboot, so its complete 1,800
source/TX records reconcile independently. No old RP slot appears in the new
epoch's exact suffix digest. The new epoch has RESET event visibility, 1,496
validated frames (1,447 data + 48 status + telemetry), and matching UART/ring/parser
byte accounting.

CPU reboot resets volatile ESP counters. They are not subtracted across the
reset. RP observed 41 pre-reboot normal datagrams: 31 consumed, nine replaced and
one pending slot retired at the error/reset boundary; final new telemetry adds
one validated datagram. Old source/TX state at CPU reset is intentionally
unobserved, so no exact whole-reset source-loss count is invented. New ESP epoch
has zero generated normal records, one RESET status (69 bytes including leading
zero), followed by sequence-1 telemetry. The following clean burst reconciles
all source/TX/UART/parser/handoff stages normally. ROM/reboot UART errors belong
to this deliberate interruption phase, never to the clean sustained result.

[retained-failures.json](retained-failures.json) preserves two identity-preflight
rejections before campaign stimuli, the immediate post-upload CPU-reboot failure,
and the first cold-retry capture timing failure. The SRAM UID reader was fixed
using the accepted read-only flash operation and QSPI initialization before all
qualified campaigns.

`ESP.restart()` immediately after serial programming returned no application
telemetry. The pinned core documents a matching
[post-upload restart limitation](https://arduino-esp8266.readthedocs.io/en/3.1.2/faq/readme.html#i-have-observed-a-case-when-esp-restart-doesn-t-work-what-is-the-reason-for-that).
An ordinary cold boot was explicitly performed and identity-checked; CPU restart
then produced new generation/RESET status. That first cold capture nevertheless
failed because its final telemetry request crossed the independent watchdog
interval; cleanup telemetry arrived. The retained bounded retry kept the final
check inside that interval and passed. The separate timeout campaign still
requires retirement at 3000 ms. These attempts were not relabeled as passes;
normal cold boot before CPU-reboot qualification is an explicit fixture
precondition. No unknown reset/download GPIO or unconditional post-upload CPU
restart reliability is claimed.

The [fresh implementation/evidence review](review.md) records the arithmetic,
ownership, concurrency, boundary and false-PASS audit. All 44 retained passed
phases satisfy the final stricter assessment. Final STOP telemetry was received,
generator counters stopped advancing, and parser/ring/consumer state drained.
Board A remains on the exact ESP qualification image and RP SRAM observer;
its persistent RP bridge and original backups remain preserved. Handles closed.

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
reference interoperability (FNR-008). FNR-006 production integration requires FNR-005 acceptance and merge; issue #6
and tracker #1 record the post-merge dependency decision.
