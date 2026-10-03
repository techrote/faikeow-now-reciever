# RAG — faikeow-now-reciever

This is the compact authoritative context for implementation work.

## Mission

Create a reusable **ESP-NOW -> USB HID receiver platform** for RP2040 + ESP8266/ESP8285 clone Pico-W hardware.

TiltMouse is the first reference sender/profile, not the core product definition.

## Platform data path

```text
ESP-NOW sender
      |
      v
ESP8266/ESP8285
      |
 generic framed datagram transport
      v
RP2040 receiver core
  |
  +-- envelope/session/order/freshness
  +-- profile dispatch
  +-- selected HID profile
      |
   TinyUSB HID
      |
      v
     host
```

## Authority split

### ESP8266/ESP8285 owns

- Wi-Fi state required for ESP-NOW only;
- channel/peer receive configuration;
- ESP-NOW callback;
- bounded newest-datagram handoff;
- radio diagnostics;
- no HID/profile interpretation beyond generic envelope-size sanity required for safe forwarding.

### RP2040 core owns

- internal frame parsing/integrity;
- generic receiver envelope parsing;
- peer/session/order/freshness state;
- profile selection/dispatch;
- USB HID lifecycle;
- fail-safe profile invalidation on radio/internal-link/USB state changes;
- diagnostics/state authority.

### Profiles own

- payload schema and validation;
- HID descriptors/report construction;
- profile-specific packet-loss behavior;
- profile-specific timeout neutralization.

The core must not contain TiltMouse-specific field names.

## Generic receiver envelope

FNR-006 freezes v1.

It must contain enough information to route and order profile messages without understanding profile payloads, including at minimum:

- platform protocol version;
- profile ID;
- message type/flags;
- sender/session identity or equivalent restart discriminator;
- monotonic message sequence;
- payload length;
- profile payload.

Exact field widths/encoding are owned here, not by TiltMouse.

ESP-NOW application payload remains <=250 bytes for ESP8266 interoperability.

## Profiles

v0.1 production profile:

- `relative_mouse`.

Future profile space is reserved for:

- keyboard;
- gamepad;
- consumer control;
- composite/custom HID;
- profile backends that may use RP2040 PIO or other peripherals.

Future profiles must not require changes to radio ingress or inter-MCU framing merely to add new HID semantics.

## Relative mouse profile

The first profile carries complete current button state plus relative X/Y.

Policy:

- movement is event-like and latest/freshness-sensitive;
- a packet contributes movement at most once;
- duplicate/stale movement is rejected;
- no stale movement backlog after loss/recovery;
- link/profile timeout releases all buttons.

TiltMouse maps its logical mouse reports into this generic profile.

## Hardware status

Target boards have RP2040 plus non-CYW43 ESP8266-class radio hardware.

Similar public boards use ESP8285 + UART + RP2040-assisted radio flashing. Those remain hypotheses until FNR-002 establishes the actual board contract.

## Internal link

FNR-002 establishes the physical transport.

If UART is confirmed, preferred v0.1 framing remains:

- COBS;
- explicit version/type/length/local sequence;
- CRC-16/CCITT;
- bounded buffers;
- deterministic resynchronization.

The internal link carries generic ESP-NOW datagrams and radio/control status; it does not carry a mouse-specific frame type.

## USB policy

The RP2040 USB layer is profile-driven.

FNR-003 establishes a generic HID framework plus the v0.1 relative-mouse implementation.

Normal v0.1 release selects one profile at build/configuration time and exposes only the required HID interface(s).

## PIO extension principle

PIO is not required for v0.1.

However, core/profile boundaries must permit a later profile/backend to use RP2040 PIO or native peripherals without redesigning ESP-NOW ingress, internal framing or peer/session handling.

## Toolchain direction

FNR-001 pins exact versions:

- RP2040: Pico SDK + TinyUSB + CMake/Ninja;
- ESP radio: reproducible ESP8266/ESP8285 toolchain with ESP-NOW;
- shared core/profile logic: portable native tests.

## Programme

- FNR-001 — reproducible dual-firmware foundation and CI.
- FNR-002 — clone-board characterization and flashing path.
- FNR-003 — generic RP2040 TinyUSB HID framework and relative-mouse profile backend.
- FNR-004 — generic ESP-NOW datagram ingress.
- FNR-005 — framed inter-MCU datagram transport.
- FNR-006 — generic receiver core, profile dispatch and relative-mouse profile semantics.
- FNR-007 — generic peer/channel/key/profile provisioning and recovery.
- FNR-008 — platform physical acceptance using TiltMouse as the reference sender/profile.
- FNR-009 — generic v0.1 release consolidation.

## v0.1 completion

v0.1 requires physical evidence that:

- both MCU firmwares run on the accepted clone hardware;
- generic ESP-NOW envelope reaches the RP2040 intact;
- peer/session/order/freshness handling is deterministic;
- relative-mouse profile produces standard USB HID;
- loss/restart cannot create stale movement or stuck buttons;
- TiltMouse interoperates as the first reference sender;
- exact artifacts/protocol/profile versions are recorded.

## Non-goals for v0.1

- Bluetooth.
- Infrastructure networking.
- General AT modem functionality.
- Multiple simultaneous HID profiles.
- Production keyboard/gamepad/consumer-control profiles.
- Production PIO-backed outputs.
