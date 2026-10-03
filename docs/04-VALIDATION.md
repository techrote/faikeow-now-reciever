# Validation

## Automated checks

FNR-001 establishes named CI gates. Later issues must preserve them.

### 1. Native/shared tests

Run on a normal host compiler with strict warnings.

Cover pure logic including:

- COBS/frame parser if used;
- CRC vectors;
- packet length/version validation;
- wrap-safe sequence comparison;
- duplicate/stale rejection;
- timeout decisions;
- latest-first queue behavior;
- button release behavior;
- HID report construction;
- recovery/reset state machines.

Use sanitizers where practical in host CI.

### 2. RP2040 firmware build

Cross-build the production RP2040 target with the exact pinned Pico SDK/toolchain.

A green build proves compilation only.

### 3. ESP8266/ESP8285 firmware build

Cross-build the production radio target with the exact pinned ESP toolchain/core.

Verify the ESP-NOW API used is actually present in the pinned dependency.

A green build proves compilation only.

### 4. Text/style/static sanity

Enforce at least:

- UTF-8/LF;
- final newline;
- no trailing whitespace;
- strict compiler warnings for native code;
- no committed generated build trees/secrets.

## Deterministic fixtures

Keep small human-readable/binary fixtures for:

- valid wireless packets;
- bad version/length;
- duplicate sequence;
- out-of-order sequence;
- sequence wrap;
- missing sequence gap;
- button press/hold/release;
- simultaneous buttons if upstream supports it;
- movement bursts;
- timeout after button hold;
- corrupted internal frames;
- truncated frames;
- concatenated/recovered frames;
- radio restart status.

Once TM-005B freezes the exact wireless bytes, import compatibility vectors from or against the transmitter repo.

## Physical FNR-002 evidence

Required before hard-coding board resources:

- radio identity;
- internal link/pins;
- button/boot behavior;
- radio flashing success;
- RP2040 recovery success;
- link-rate stress evidence.

## Physical FNR-008 evidence

Required for release:

### USB

- standard OS HID driver binds;
- expected mouse-only descriptors;
- unplug/replug;
- no synthetic cursor movement before valid wireless input.

### RF/end-to-end

- exact TiltMouse transmitter build;
- receiver radio receives packets;
- actual packet/sequence counters;
- motion/button function;
- reasonable desktop operating distance;
- no infrastructure Wi-Fi dependency.

### Faults

Exercise:

- transmitter power loss while a button is held;
- radio reset while a button is held;
- RP2040 reset;
- internal link interruption/corruption where practical;
- USB replug;
- wrong peer/channel/key;
- sequence duplicates/out-of-order injection if tooling permits.

Acceptance requires all uncertain/fault states to converge to zero movement and released buttons.

### Performance

Record rather than over-promise:

- report rate seen at USB;
- end-to-end latency measurement method and observed distribution;
- ESP-NOW sequence gaps/loss;
- internal link drop/error counts;
- recovery time.

No specific latency target is accepted until measured. The goal is subjectively usable mouse behavior plus bounded deterministic safety.

## Evidence identity

Every physical result must identify:

- receiver repo commit;
- RP2040 artifact checksum;
- ESP radio artifact checksum;
- upstream TiltMouse commit/artifact;
- internal protocol version;
- wireless protocol version;
- host OS;
- board/chip identity;
- test date.

## Merge rule

CI may merge software-only issues whose acceptance is entirely automated.

Hardware-gated issues do not merge/close on inference. If hardware is unavailable, leave a clean handoff with exact missing evidence.
