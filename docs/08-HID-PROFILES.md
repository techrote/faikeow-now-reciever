# HID profiles and extension model

## Purpose

Profiles let the receiver platform support different host-facing HID semantics
without changing ESP-NOW radio ingress, board-internal framing, peer/session
ordering, or provisioning foundations.

FNR-003 implements this boundary in portable code under:

- `shared/include/fnr/hid.h`;
- `shared/src/hid.c`;
- profile backends such as `hid_relative_mouse.*`;
- `firmware/rp2040/usb_adapter.*` for TinyUSB-specific integration.

Pico SDK and TinyUSB types do not appear in the portable profile contract.

## Generic profile API

A registered `fnr_hid_profile` contains a local configuration name, opaque
context and backend operations. Registration borrows those objects; profile,
context, operations, descriptors and string storage must outlive the HID device.

The profile name is **not** a platform wire profile ID. FNR-006 owns numeric wire
IDs, profile schema versions and dispatch integration.

A backend owns:

- immutable device/configuration/HID report descriptor bytes;
- current logical output state;
- report construction;
- optional safe GET_REPORT construction;
- profile-specific neutralization;
- consumption of event-like state after a report is accepted for submission;
- optional notification of real USB reset/mount/unmount/suspend/resume events.

Profile/device state has one logical owner and is not internally synchronized; callers
must serialize lifecycle, logical-input and service operations.

The generic framework owns:

- a fixed-capacity profile registry;
- detached-only profile selection because selection changes USB identity;
- USB lifecycle state;
- one bounded pending-report slot;
- readiness/backpressure handling;
- transport failure counters;
- neutralization/invalidation policy;
- copying report bytes before target submission.

## Descriptor ownership and selection

Each backend returns a portable `fnr_hid_descriptor_set`. Report descriptor
entry `N` maps to HID instance `N`. Descriptors are immutable borrowed storage
and must remain valid for the selected profile's lifetime.

Normal v0.1 selects one profile before TinyUSB initialization. A selection
change while mounted/suspended is rejected as busy. Future runtime switching
must deliberately disconnect/re-enumerate; it must not silently mutate
identity beneath an enumerated host.

FNR-003 supports multiple registered backends as an extension seam but does not
claim simultaneous composite-profile operation.

## Bounded output and recovery policy

There is exactly one pending report slot. Publishing a newer state while an
ordinary report occupies the slot replaces the older snapshot and increments a
diagnostic counter. There is no unbounded report backlog.

A recovery-neutral report is a safety barrier and cannot be replaced by normal
output. If logical input arrives while that barrier is pending, the framework
neutralizes/drops that output and returns `FNR_HID_STATUS_NEUTRAL_PENDING`; the
host must first be offered the neutral report. This prevents backpressure from
turning recovery into a stale-button/movement continuation.

USB reset, unmount and suspend:

1. discard the pending report;
2. neutralize profile state synchronously;
3. retain no relative movement for later replay.

Mount and resume start from neutral profile state and queue one neutral report.
Generic receiver/transport invalidation uses the same neutralization behavior
without fabricating a USB lifecycle callback.

A target send succeeds when the USB stack accepts the transfer for submission.
The backend may then consume event-like state. If TinyUSB later reports an
asynchronous transfer failure, the adapter invalidates/neutralizes; it does not
retry stale relative movement.

## v0.1 `relative_mouse`

The first production backend exposes one standard boot-compatible mouse HID
interface and no CDC, keyboard, gamepad, MSC or vendor interface.

The input report has no report ID and is exactly three bytes:

| Byte | Meaning |
|---|---|
| 0 | bit 0 left, bit 1 right, bits 2-7 padding |
| 1 | signed 8-bit relative X |
| 2 | signed 8-bit relative Y |

The report descriptor declares X/Y logical range `-127..127`. The logical API
accepts signed 16-bit X/Y and deterministically clamps outside that range,
returning `FNR_HID_STATUS_CLAMPED` and counting each clamped axis.

Each logical input supplies the complete current left/right button state.
Successful report submission consumes X/Y once but retains the button state.
Explicit neutralization clears X/Y and releases all buttons.

GET_REPORT returns current buttons with zero X/Y, so a host control transfer
cannot duplicate a pending relative movement event.

FNR-003 does not define the application-wire relative-mouse payload. FNR-006
will map already-authorized/ordered profile messages into the logical input API.

## TinyUSB adapter

The RP2040 adapter is the only layer that includes TinyUSB/Pico SDK APIs. It:

- initializes TinyUSB 0.18.0 on root hub port 0;
- provides descriptor callbacks from the selected backend;
- services TinyUSB non-blockingly in the production loop;
- maps mount/unmount/suspend/resume to portable lifecycle events;
- observes bus reset through TinyUSB's event hook, deferring the state change
  out of interrupt context;
- uses `tud_hid_n_ready()` and `tud_hid_n_report()` as readiness/send adapters;
- converts asynchronous HID transfer failure into neutralization rather than
  movement retry.

Production firmware contains no test injector and no synthetic cursor motion.

## Adding another HID profile

A future keyboard/gamepad/custom backend should:

1. keep its logical state and policy in portable code;
2. provide immutable descriptors and report builders through `fnr_hid_backend_ops`;
3. define a safe `neutralize()` action (for example release all keys or center
   axes/release buttons);
4. consume any event-like data only after accepted report submission;
5. add deterministic native descriptor/state/fault tests;
6. register/select the profile before USB enumeration;
7. let FNR-006/FNR-007 own wire IDs, authorization and provisioning rather than
   importing radio/internal-transport concepts into the backend.

A profile requiring additional HID interfaces can supply multiple report
descriptors up to the fixed framework capacity, but composite release behavior
requires explicit later design/acceptance.

## Development VID/PID

FNR-003 uses `0xCAFE:0xF003` as a controlled development identity. `0xCAFE` is
used throughout TinyUSB examples with unique example PIDs; it is not claimed as
this project's production vendor identity. FNR-009 must replace the development
pair with a legitimately allocated release identity before distribution or any
certification claim.

## FNR-006 integration handoff

FNR-006 should interact with the USB/profile side through narrow logical hooks:

- select/configure an already registered profile while detached;
- for `relative_mouse`, call `fnr_relative_mouse_apply_input()` only after the
  generic envelope, peer/session/order and profile authorization checks pass;
- call `fnr_hid_device_publish_current()` to offer that logical state;
- call `fnr_hid_device_invalidate_output()` on radio/internal/session/freshness
  invalidation so buttons release and movement is discarded;
- never treat `FNR_HID_STATUS_REPLACED_PENDING` as a request to replay the
  overwritten movement.

No platform envelope fields, wire profile IDs, sequence arithmetic or profile
payload encoding are frozen by FNR-003.

## Physical acceptance boundary

Native lifecycle tests and an RP2040 cross-build qualify software state only.
They do not prove physical enumeration, host polling, clone-board routing or
end-to-end TiltMouse behavior. FNR-008 owns those physical claims.
