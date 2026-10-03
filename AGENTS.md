# AGENTS.md

These rules apply unless a newer owning issue explicitly changes them.

## Mandatory read order

Before editing:

1. `RAG.md`
2. `docs/00-PROGRAMME.md`
3. `docs/01-ARCHITECTURE.md`
4. `docs/02-PROTOCOLS.md`
5. `docs/03-HARDWARE.md`
6. `docs/04-VALIDATION.md`
7. `docs/05-BUILDING.md`
8. `docs/06-REFERENCES.md`
9. `docs/07-EXECUTION-PROTOCOL.md`
10. `docs/08-HID-PROFILES.md`
11. owning issue/comments
12. live `main`, relevant branches/PRs and CI

## Scope rules

- One FNR issue owns one bounded scope.
- Resume legitimate existing issue work rather than duplicating it.
- ESP radio firmware stays profile-agnostic.
- Internal MCU framing stays profile-agnostic.
- Generic receiver core may know profile IDs but must not contain profile payload field names.
- HID semantics belong to profiles.
- TiltMouse is a reference sender/profile user, not the repository's core product definition.
- Do not add infrastructure networking, Bluetooth, AT-modem functionality or web UI unless explicitly owned by a later issue.
- Do not assume public clone pinouts before FNR-002 evidence.
- ESP-NOW application payload remains <=250 bytes for the accepted ESP8266 interoperability target.
- Do not create unbounded packet/report queues.
- Do not fabricate hardware/RF/USB evidence.

## Engineering rules

- Keep envelope parsing, sequence/session arithmetic, profile dispatch, timeout decisions and HID state host-testable.
- Keep radio callbacks short/bounded/allocation-free in hot paths where practical.
- Use fixed capacities and explicit bounds.
- Separate platform protocol version from profile schema version.
- Adding a future profile should not require rewriting radio ingress or internal framing.
- Preserve RP2040 BOOTSEL and ESP radio recovery.
- PIO expansion is an architectural extension point, not v0.1 implementation scope.

## PR/merge protocol

Follow `docs/07-EXECUTION-PROTOCOL.md`.

Use squash merge unless a concrete reason dictates otherwise.

## Evidence discipline

Distinguish:

- native-test evidence;
- RP2040 build evidence;
- ESP-radio build evidence;
- physical clone-board evidence;
- ESP-NOW RF evidence;
- USB-host evidence;
- profile interoperability evidence.

Compilation is not physical acceptance.
