# Validation

## Automated gates

FNR-001 establishes:

1. **Native tests**
2. **RP2040 firmware build**
3. **ESP radio firmware build**
4. **Text and style sanity**

## Native/shared coverage

Pure code should cover:

### Platform

- internal frame codec/parser;
- CRC vectors;
- generic envelope version/length validation;
- profile ID dispatch;
- sender/session restart handling;
- wrap-safe sequence comparison;
- duplicate/stale rejection;
- freshness/timeout transitions;
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
