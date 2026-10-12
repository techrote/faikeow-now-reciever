# Building the dual-firmware platform through FNR-007

## Current software state

The RP2040 firmware now connects the accepted layers through FNR-006:

- **FNR-003:** TinyUSB HID framework and relative-mouse backend;
- **FNR-005:** bounded UART0 framed transport and mandatory event barrier;
- **FNR-006:** platform-v1 envelope/session/order/freshness core, generic profile
  dispatch and relative_mouse/v1 adaptation.

The factory receiver configuration remains deliberately unprovisioned and
fail-closed. FNR-007 supplies paired build-time peer/channel/key/profile
provisioning. The public record is embedded in both images; only the ESP image
contains the per-device radio key. RP admission is gated on a matching READY
radio status.

The ESP8266/ESP8285 firmware remains FNR-004/FNR-005 profile-neutral ingress and
transport. It forwards opaque application bytes and does not interpret FNR-006.

FNR-002 hardware and FNR-005 production-link evidence remain accepted inputs.
FNR-006 adds no new physical claim; FNR-008 owns integrated RF/USB/reference
sender acceptance.

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

FNR-005 adds independent CRC/COBS/golden vectors, hostile-stream recovery,
ownership/drop accounting, real ingress-to-transport simulation, and both
production adapters compiled against minimal SDK substitutes. These include
the ESP FIFO boundary and RP queued-frame/timeout/IRQ ordering regressions.

FNR-006 adds platform-envelope/profile golden vectors, session/order/freshness and
USB-lifecycle tests plus a production-adapter integration test. FNR-007 adds
provisioning record/corruption/property tests, secret-separated generator tests,
and actual RP/ESP provisioning adapter regressions. The hostile
envelope test uses fixed seed `0xd6e8feb8` over 12,000 streams.

CI also configures `build/native-asan` with
`-fsanitize=address,undefined -fno-sanitize-recover=all` for both C and C++, then
builds and runs the same suite with strict warnings.
Native USB/RF simulation is software evidence only.

## Provisioned paired build

Keep the provisioning JSON outside the repository and use the same file for both
lanes:

```sh
export FNR_PROVISIONING_CONFIG=/absolute/path/outside/repo/provisioning.json
bash tools/build_rp2040.sh
FNR_SOURCE_ID="$(git rev-parse HEAD)" bash tools/build_esp.sh
```

The generator writes only under ignored `build/provisioning/`:

- `rp_provisioning_image.c` — public record only;
- `esp_provisioning_image.cpp` — public record + per-device key;
- `provisioning-redacted.json` — no key material.

Unset `FNR_PROVISIONING_CONFIG` for factory/unprovisioned artifacts. CI also
cross-builds deterministic non-secret provisioned fixtures in separate build
directories, so generated-source integration is tested without publishing a
production key.

The redacted physical-smoke diagnostic is built with
`tools/build_esp_provisioning_diag.sh`. See
[FNR-007-PROVISIONING.md](FNR-007-PROVISIONING.md).

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
uses the accepted UART0 GPIO0 TX/GPIO1 RX contract. The UART ISR writes a fixed
1024-byte ring; foreground parses at most 512 bytes per service call. USB and
transport remain separate, with no synthetic input or direct HID dispatch.

The CI ELF-symbol gate requires the production descriptor/lifecycle callbacks,
`fnr_usb_adapter_task` and `fnr_hid_device_service` to survive final linking.
`python3 tools/check_transport_symbols.py rp2040` additionally verifies the
production IRQ-buffer/parser service and public consumer seams. The compiler
may inline the standalone decode routine into the linked streaming parser;
the gate checks the actual retained production entry points.

## ESP8266 generic ingress cross-build

```sh
FNR_SOURCE_ID="$(git rev-parse HEAD)" bash tools/build_esp.sh
```

Outputs:

- `build/esp8266/fnr_esp8266_ingress.bin`
- `build/esp8266/fnr_esp8266_ingress.elf`

The default build is fail-closed/unprovisioned. When explicitly configured it
initializes the pinned NONOS ESP-NOW receive path, selects the explicit SDK
channel, performs exact-source admission and exposes a bounded generic datagram
handoff. Channel selection is not a per-datagram physical-channel filter. It does not interpret
HID/profile payloads. FNR-005 takes at most one foreground record per loop and
writes at most 32 bytes, bounded by the pinned UART's usable FIFO space. It
sends generic status every second even when the radio is unprovisioned.
`python3 tools/check_transport_symbols.py esp` verifies that the receive
registration, foreground handoff, TX scheduler and serializer remain linked. See
[FNR-004 radio ingress](FNR-004-RADIO-INGRESS.md).

## CI and artifact identity

`.github/workflows/fnr-001.yml` retains its historical filename but is the
platform CI. On the exact PR head it gates:

1. text/source sanity;
2. native UBSan and ASan/UBSan tests, including both real production adapters;
3. locked RP2040 TinyUSB HID cross-build plus linked-path symbol checks;
4. locked ESP8285 radio-ingress/transport cross-build plus linked-path checks.

Each job explicitly checks out the PR source head (or push commit). Hardware
diagnostic lanes are compile-only regressions; CI never accesses a board.

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
- production FNR-005 frames traversing the real board-internal UART;
- FNR-006 envelope/order/session/profile wire semantics;
- TiltMouse interoperability or end-to-end latency.

FNR-008 owns physical platform/reference-profile acceptance.

The statements above delimit what the build gates prove. Separate FNR-002
physical measurements establish the two boards' identity/recovery/UART contract
and additional USB/RF subsets, with their limitations. Hardware authority now
belongs to [techrote/picoWcloneESP8285N08](https://github.com/techrote/picoWcloneESP8285N08);
use its [operating runbook](https://github.com/techrote/picoWcloneESP8285N08/blob/main/docs/DEVBOARD-OPERATING-RUNBOOK.md)
when deploying these artifacts. [03-HARDWARE.md](03-HARDWARE.md) links the
qualified reference baseline and retained local characterization records.
Dependency/build authority remains this repository's lock and build scripts.
Do not infer complete receiver acceptance from either a build or a subset test.

## Failure handling

- Dependency checksum/source-commit mismatch is a hard stop.
- Do not silently edit `dependencies.lock.json` to make acquisition pass.
- Use `python3 tools/check_deps.py <lane>` to revalidate local identities.
- Keep `.deps/`, `build/`, credentials and flashing artifacts out of Git.
- Successful compilation is not evidence of board operation; target hardware
  faults are not inferred from software-only failures.
