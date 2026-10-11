# Building the dual-firmware platform after FNR-003 and FNR-004

## Current software state

The two firmware lanes are independently software-functional but are not yet
connected into a complete receiver:

- **RP2040 / FNR-003:** generic TinyUSB HID framework with selected
  `relative_mouse` backend, live USB descriptors and USB service loop.
- **ESP8266/ESP8285 / FNR-004:** profile-neutral ESP-NOW receive initialization,
  fail-closed exact-peer admission and bounded generic foreground handoff;
  shipping/default build remains unprovisioned.

FNR-005 still owns the physical framed inter-MCU transport. FNR-002 owns the
actual clone-board wiring/flashing contract. Neither cross-build is physical
USB/RF acceptance.

The RP2040 target is deliberately renamed from `fnr_rp2040_inert` to
`fnr_rp2040_hid`; consumers of the old artifact name must migrate. The ESP8266
artifact remains the FNR-004 `fnr_esp8266_ingress` target.

## Pinned dependencies

FNR-001 remains authority for acquisition and provenance. Exact versions/hashes
are in `dependencies.lock.json`, including:

- Pico SDK 2.2.0;
- TinyUSB 0.18.0 at the SDK-pinned gitlink;
- Arm GNU Toolchain 13.3.Rel1;
- ESP8266 Arduino 3.1.2 / NONOSDK22x_190703;
- XTensa GCC 10.3 and makeEspArduino 6.7.

Dependencies are fetched only by explicit acquisition commands and installed
under ignored `.deps/`:

```sh
python3 tools/acquire_deps.py --lane rp2040
python3 tools/acquire_deps.py --lane esp
```

Use `--lane all` to acquire both. Build scripts never fetch SDKs implicitly.

## Native qualification

```sh
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=undefined -fno-sanitize-recover=all" \
  -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-sanitize-recover=all"
cmake --build build/native --parallel 2
ctest --test-dir build/native --output-on-failure
```

Project code builds with `-Wall -Wextra -Werror -Wconversion -Wshadow`.
The native suite covers:

- FNR-001 datagram ownership and bounds;
- FNR-004 peer/channel admission, bounded newest-wins ingress, loss accounting
  and restart transitions;
- FNR-003 profile registration/selection, descriptor invariants, relative-mouse
  signed/clamped reports, buttons, USB lifecycle, backpressure, recovery-neutral
  barriers, invalidation and deterministic fault stress.

Native USB/RF simulation is software evidence only.

## RP2040 TinyUSB HID cross-build

```sh
bash tools/build_rp2040.sh
```

Outputs:

- `build/rp2040/fnr_rp2040_hid.uf2`
- `build/rp2040/fnr_rp2040_hid.elf`

The target selects `relative_mouse` before TinyUSB initialization, enables one
HID class instance and disables CDC/MSC/MIDI/vendor classes. Descriptor callbacks
resolve through the selected backend. The TinyUSB task is serviced
non-blockingly; the production loop contains no synthetic mouse generator and
uses no clone-board GPIO assumptions.

The CI ELF-symbol gate requires the production descriptor/lifecycle callbacks,
`fnr_usb_adapter_task` and `fnr_hid_device_service` to survive final linking.

## ESP8266 generic ingress cross-build

```sh
FNR_SOURCE_ID="$(git rev-parse HEAD)" bash tools/build_esp.sh
```

Outputs:

- `build/esp8266/fnr_esp8266_ingress.bin`
- `build/esp8266/fnr_esp8266_ingress.elf`

The default build is fail-closed/unprovisioned. When explicitly configured it
initializes the pinned NONOS ESP-NOW receive path, performs exact-source/channel
admission and exposes a bounded generic datagram handoff. It does not interpret
HID/profile payloads or assume a UART/SPI link to the RP2040. CI verifies that
`esp_now_register_recv_cb` remains linked. See
[FNR-004 radio ingress](FNR-004-RADIO-INGRESS.md).

## CI and artifact identity

`.github/workflows/fnr-001.yml` retains its historical filename but is the
platform CI. On the exact PR head it gates:

1. text/source sanity;
2. native UBSan tests;
3. locked RP2040 TinyUSB HID cross-build plus linked-path symbol checks;
4. locked ESP8266 radio-ingress cross-build plus receive-registration symbol
   check.

Each firmware job writes `SHA256SUMS` and `manifest.json`. Manifests record the
source ID, dependency-lock hash, compiler identity, configuration and artifact
hashes, with `physical_acceptance: false`. Preserve the manifest and exact CI run
together; a binary hash alone does not prove its source commit.

## USB identity policy

FNR-003 uses `VID 0xCAFE` with project-local `PID 0xF003` only as a controlled
development identity. TinyUSB examples use `0xCAFE` with unique example PIDs;
this repository does not claim ownership of that VID or impersonate another
production device. FNR-009 must replace it with a legitimately allocated
release identity before distribution/certification claims.

## Physical non-claims

Current software qualification does **not** establish:

- USB enumeration/input on the actual clone board or any host OS;
- physical ESP-NOW reception/channel behavior on that board;
- clone RP2040/radio pinout or flashing/recovery;
- the board-internal framed link;
- FNR-006 envelope/order/session/profile wire semantics;
- TiltMouse interoperability or end-to-end latency.

FNR-008 owns physical platform/reference-profile acceptance.

## Failure handling

- Dependency checksum/source-commit mismatch is a hard stop.
- Do not silently edit `dependencies.lock.json` to make acquisition pass.
- Use `python3 tools/check_deps.py <lane>` to revalidate local identities.
- Keep `.deps/`, `build/`, credentials and flashing artifacts out of Git.
- Successful compilation is not evidence of board operation; target hardware
  faults are not inferred from software-only failures.
