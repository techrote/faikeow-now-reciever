# FNR-003 implementation evidence and boundaries

## Scope

Owner: issue #4, **FNR-003 — Generic RP2040 TinyUSB HID framework and
relative-mouse backend**.

This issue converts the FNR-001 inert RP2040 compile target into a real
TinyUSB HID-capable firmware target while keeping radio/envelope/internal-link
scope out of the USB layer.

## Implemented architecture

Portable framework:

- `shared/include/fnr/hid.h`
- `shared/src/hid.c`

First backend:

- `shared/include/fnr/hid_relative_mouse.h`
- `shared/src/hid_relative_mouse.c`

Target adapter:

- `firmware/rp2040/usb_adapter.h`
- `firmware/rp2040/usb_adapter.c`
- `firmware/rp2040/main.c`
- `firmware/rp2040/tusb_config.h`

The portable framework has fixed capacities, borrowed immutable descriptor
ownership, one copied pending-report slot and no target SDK types. Device/profile
state is single-owner and callers serialize logical input, lifecycle and service
operations. Profile names
are local configuration keys rather than FNR-006 wire IDs.

## Relative-mouse descriptor contract

The development device identity is `VID 0xCAFE / PID 0xF003`; it is explicitly
non-production and must be replaced by FNR-009 with a legitimately allocated
release identity.

Normal identity contains:

- one configuration;
- one boot-compatible HID mouse interface;
- one interrupt IN endpoint (`0x81`, 8-byte maximum packet, 1 ms interval);
- no CDC, keyboard, gamepad, MSC, MIDI or vendor interfaces.

The HID input report has no report ID and is three bytes: complete left/right
button state, signed relative X, signed relative Y. The report descriptor is 50
bytes and declares signed `-127..127` X/Y. Inputs outside that range are clamped
deterministically and counted.

## State and failure policy

- Output before mount is not queued.
- Backpressure retains at most one pending ordinary report; newer ordinary
  state replaces older pending state.
- Recovery-neutral is a non-replaceable barrier; input arriving before the
  neutral report can be submitted is dropped/neutralized rather than bypassing
  recovery.
- A successful target submission consumes relative X/Y at most once while
  leaving complete current button state held.
- USB reset, unmount and suspend discard pending movement and neutralize.
- Mount/resume queue a fresh neutral report before later logical input.
- Generic receiver/transport invalidation neutralizes without inventing a USB
  lifecycle event.
- TinyUSB asynchronous transfer failure neutralizes rather than replaying the
  previously submitted relative motion.
- GET_REPORT never echoes relative movement; it returns current buttons with
  zero X/Y.

## Production integration

`fnr_rp2040_hid` initializes the pinned TinyUSB 0.18.0 device stack and services
it non-blockingly. Descriptor callbacks are live and resolved through the
selected backend. Bus-reset observation is flag-only in TinyUSB's event hook so
portable state is never mutated from that potentially interrupt-context hook.

The production loop contains no synthetic input generator. FNR-006 later feeds
already-authorized/validated logical profile input.

## Deterministic native qualification

`tests/native/test_hid.cpp` covers:

- profile registration, invalid descriptors, duplicate and unknown selection;
- USB lifecycle callback delivery and detached-only selection policy;
- device/configuration/HID report descriptor invariants;
- 3-byte report sizing and signed X/Y representation;
- deterministic X/Y clamping;
- left/right combinations, hold, release and explicit neutral state;
- safe GET_REPORT behavior;
- readiness/backpressure, non-replaceable recovery-neutral barrier and one-slot
  newest-state replacement;
- synchronous send failure replacement;
- mount/unmount/suspend/resume/reset recovery;
- no stale movement replay;
- no report emission while invalid/unready;
- generic invalidation and transport-error neutralization;
- a 100-cycle deterministic fault/stress sequence.

The canonical qualification command uses strict warnings and UBSan as documented
in `docs/05-BUILDING.md`.

## Cross-build and CI evidence

The existing locked FNR-001 toolchains are reused. CI builds:

- `fnr_rp2040_hid.uf2` and `.elf` using Pico SDK 2.2.0 / TinyUSB 0.18.0;
- the FNR-004 ESP8266 generic radio-ingress target, preserved as a regression build;
- all native tests;
- text/source sanity.

The RP2040 job also verifies that the final ELF defines the production descriptor
callbacks, `fnr_usb_adapter_task` and generic HID service symbol. Firmware jobs
publish `SHA256SUMS` and `manifest.json`; exact accepted run IDs, source head and
artifact hashes belong in the PR/issue acceptance record because any later
source change necessarily changes the head being qualified.

## Explicit non-claims

FNR-003 does not establish:

- physical USB enumeration or host input on the clone board;
- clone-board GPIO/radio link identity or flashing path;
- ESP-NOW ingress;
- board-internal transport;
- platform envelope/profile wire schema;
- sender/session sequence/duplicate/reorder handling;
- TiltMouse interoperability.

Native USB lifecycle simulation is software evidence only. Physical platform and
relative-mouse acceptance remain FNR-008 scope.

## FNR-006 handoff

FNR-006 may call `fnr_relative_mouse_apply_input()` after its generic
authorization/order checks, then `fnr_hid_device_publish_current()`. Any
radio/internal/session/freshness invalidation should call
`fnr_hid_device_invalidate_output()` to clear movement and release buttons.
FNR-006 must define wire profile IDs/schema independently; FNR-003 does not
serialize or parse the application profile payload.
