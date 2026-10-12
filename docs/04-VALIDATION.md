# Validation

## Automated gates

FNR-001 establishes:

1. **Native tests (UBSan and ASan)**
2. **RP2040 firmware build**
3. **ESP radio firmware build**
4. **Text and style sanity**

## Native/shared coverage

Pure code should cover:

### Platform

- FNR-006 explicit platform-v1 envelope/golden vectors and hostile-byte round trips;
- internal frame codec/parser;
- CRC vectors;
- generic envelope version/length validation;
- profile ID dispatch;
- sender/session restart handling;
- wrap-safe sequence comparison;
- duplicate/stale rejection;
- freshness/timeout transitions and u32 timer wrap;
- FNR-005 event-to-core invalidation with platform ordering retained;
- production FNR-005 consumer-hook -> FNR-006 -> HID integration;
- bounded queue/drop behavior;
- provisioning/config validation.

### HID framework

- profile registration/selection;
- USB mount/unmount/suspend/resume;
- no stale report replay after invalid/unmounted state.

### relative_mouse profile

- X/Y bounds;
- button state;
- duplicate/stale no-motion behavior;
- sequence gaps;
- timeout/restart release;
- no stale movement burst after recovery.

Future profiles add their own deterministic policy tests.

## Physical FNR-002 evidence

Unchanged:

- exact radio identity;
- internal link/pins;
- flashing/recovery;
- proven link rate.

## FNR-005 software and production-link boundary

The [internal v1 tests](FNR-005-TRANSPORT.md) cover independent codec vectors,
deterministic hostile streams, bounded TX/RX ownership, and the actual ESP/RP
production adapter sources against minimal SDK substitutes. Adapter regressions
include the pinned ESP FIFO boundary and queued input across the RP link timeout.
The target builds also check linked production symbols and preserve manifests.
These are software checks, with no attached-device use or physical acceptance.

The [precise FNR-005 physical campaign](FNR-005-HARDWARE-HANDOFF.md) requires
production COBS/CRC16 observations on one representative characterized board.
The two available units are identical hardware designs; a second full campaign
is required only if evidence warrants comparison for a plausible unit-specific
fault. The earlier
SLIP/CRC32 diagnostic runs are accepted FNR-002 inputs, not this evidence.

[FNR-005 representative-DUT evidence](../evidence/FNR-005/README.md) records the
completed production framing, sustained, overload, corruption and interruption
cases, including independent MCU restart/recovery. All 44 physical phases pass;
final review, exact-head CI and merge/post-merge verification remain separate
programme gates recorded in PR #20 and issue #6.

## FNR-006 software acceptance boundary

FNR-006 adds deterministic native tests for the exact 18-byte envelope,
relative_mouse/v1 bytes, peer authorization, session restart/retirement,
modulo-u32 duplicate/stale/gap/wrap/half-range behavior, 250 ms application
freshness, USB lifecycle neutralization and fixed-seed hostile input. A separate
native target compiles the real RP2040 FNR-005 consumer hook together with the
FNR-006 adapter. Both locked target builds remain required. These are software
evidence, not RF or USB-host physical acceptance.

## FNR-007 software acceptance boundary

FNR-007 software checks cover the exact public-record/CRC vector, full-byte
corruption rejection, 20,000 deterministic codec cases, generated-source secret
separation, RP/ESP provisioning loaders, the actual encrypted ESP radio adapter,
and the actual RP FNR-005 status-to-admission gate. UBSan, ASan/UBSan, GCC
`-fanalyzer`, locked RP2040/ESP8285 cross-builds and linked-symbol checks remain
required.

These do not prove physical provisioning. Before FNR-007 acceptance, one
representative characterized board must establish MAC discovery, correct
peer/channel/profile provisioning, intentional wrong peer/channel/profile safe
inactivity, correct-config restoration, power-cycle persistence, and no-solder
reconfiguration/recovery.

## Physical FNR-008 evidence

Separate claims.

### Platform claims

- ESP-NOW datagrams received on target radio;
- internal frames remain synchronized;
- generic envelope/session/order handling works;
- provisioning and restart recovery work;
- selected profile dispatch is correct;
- USB lifecycle is safe.

### relative_mouse profile claims

- standard mouse HID enumeration;
- X/Y/buttons;
- duplicate/reorder/loss semantics;
- timeout releases buttons;
- no stale movement replay.

### TiltMouse reference interoperability

Use exact TiltMouse artifacts to prove the first real sender maps correctly to the generic `relative_mouse` profile.

Do not describe TiltMouse success as proof that every future profile is accepted.

## Fault campaign

Before v0.1:

- radio loss/restart;
- internal-link corruption/restart;
- sender/session change;
- duplicate/out-of-order messages;
- USB unmount/replug;
- wrong peer/channel/key/profile;
- profile timeout.

Every profile must define a safe neutral state.

## Evidence identity

Record:

- receiver commit/artifact hashes;
- platform protocol version;
- internal protocol version;
- selected profile ID/schema version;
- sender artifact/protocol/profile version;
- board/radio identity;
- host OS/date.

## Merge rule

Software-only issues may merge on automated acceptance.

Hardware-gated criteria require physical evidence or a precise open handoff.
