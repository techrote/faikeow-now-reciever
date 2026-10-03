# AGENTS.md

These rules apply to repository work unless a newer owning issue explicitly changes them.

## Mandatory read order

Before editing:

1. read `RAG.md`;
2. read `docs/00-PROGRAMME.md`;
3. read `docs/01-ARCHITECTURE.md`;
4. read `docs/02-PROTOCOLS.md`;
5. read `docs/03-HARDWARE.md`;
6. read `docs/04-VALIDATION.md`;
7. read `docs/05-BUILDING.md`;
8. read `docs/06-REFERENCES.md`;
9. read `docs/07-EXECUTION-PROTOCOL.md`;
10. read the complete owning issue body and all comments;
11. reconcile live `main`, relevant branches/PRs and CI.

If an issue changes an architectural or protocol contract, update the relevant authority document in the same PR.

## Scope rules

- One FNR issue owns one bounded implementation scope.
- Resume an existing legitimate branch/PR for the issue rather than duplicating work.
- Keep the ESP8266/ESP8285 radio firmware narrow. It does not own USB HID.
- Keep the RP2040 as the receiver safety/state authority because it controls USB HID.
- Do not turn this project into a general Wi-Fi modem, AT-command shell, web service or Bluetooth project.
- Do not add infrastructure Wi-Fi/IP networking unless a later issue explicitly owns it.
- Do not assume public ESP8285 clone pin mappings are identical to the user's boards before FNR-002 establishes physical evidence.
- Do not invent a wireless packet format that conflicts with the TiltMouse transmitter. The exact TiltMouse ESP-NOW bytes come from the upstream TM-005B contract.
- Movement is latest-first. Never create an unbounded queue of stale cursor movement.
- Complete button state is stateful safety data. Link loss/fault must eventually force all buttons released.
- Do not fabricate hardware, RF, latency or USB-enumeration evidence.

## Engineering rules

- Keep packet parsing, sequence arithmetic, timeout decisions and HID report construction host-testable.
- Keep ISR/callback/high-priority radio work short and bounded.
- Prefer fixed-size buffers and explicit capacity checks on both MCUs.
- Reject malformed lengths/versions before copying into application structures.
- Use wrap-safe sequence comparisons and deterministic tests around wrap boundaries.
- Keep USB identity mouse-only for the normal v0.1 receiver.
- Diagnostic interfaces must not become required for normal operation.
- Preserve an RP2040 BOOTSEL recovery route and an ESP8266/ESP8285 flashing recovery route.

## PR and merge protocol

For implementation issues:

1. implement only the owned scope;
2. add/update deterministic tests and documentation;
3. run all checks required by `docs/04-VALIDATION.md`;
4. commit/push the issue branch;
5. open/update a PR referencing the issue;
6. inspect CI and repair legitimate failures;
7. merge only when required automated checks are green and issue acceptance is genuinely established;
8. verify post-merge `main`;
9. close/update the issue and programme tracker;
10. stop rather than beginning successor work unless the user explicitly asked for it.

Use squash merge unless the issue or repository history gives a concrete reason not to.

## Evidence discipline

Always distinguish:

- host/native-test evidence;
- RP2040 cross-build evidence;
- ESP8266/ESP8285 cross-build evidence;
- physical clone-board evidence;
- ESP-NOW RF evidence;
- USB-host evidence;
- assumptions awaiting verification.

A successful cross-build is not proof that the target board, RF path or host USB behavior works.
