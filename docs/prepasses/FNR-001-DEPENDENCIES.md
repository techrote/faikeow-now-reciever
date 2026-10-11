# FNR-001 — dependency and build-boundary decisions

## 1. Assessment and result

The assessed main commit is `8cc7a4c109a099466d8389a4e3b4004bfdf44c01`.
Issue #2, tracker #1, AGENTS.md, RAG.md and all nine programme/reference documents
were read. The assessed tree has documentation only: there is no existing CMake,
firmware, test or workflow implementation to preserve. Open PRs and workflow-run
collections were empty when read. The work below proposes an additive foundation;
it does not execute the issue's publication/merge instructions. [R1–R4]

**Recommendation:** take the exact source and artifact identities in
`proposed/dependencies.lock.json`. Use three independent build invocations rather
than attempting to switch cross-compilers inside one CMake project. Use a canonical
Linux x86_64 build host (WSL2 is an appropriate way to run that host profile on Windows).
No native-Windows reproducibility claim is made.

The decision is a pinned candidate with source-level compatibility evidence and
executed host/object probes, **not a proven complete cross-build combination**.
Neither cross compiler nor either full SDK was available in the container, and
shell DNS/downloads failed. The acquisition and full target-build scripts are
therefore proposed, syntax-checked scripts; they are not recorded successful runs.
This distinction is part of the handoff, not a waived FNR-001 gate.

## 2. Exact selected set

| Component | Selected identity | Reason and boundary |
|---|---|---|
| Pico SDK | 2.2.0, `a1438dff1d38bd9c65dbd693f0e5db4b9ae91779` | Official C/C++ SDK; source pinned, not a moving release branch. |
| TinyUSB | `86ad6e56c1700e85f1c5678607a762cfe3aa2f47` | The SDK's actual gitlink; its version header reports 0.18.0. Do not independently upgrade it. |
| picotool | 2.2.0, `a7eb3988f0645239185fadb4e25d8279478c2dbb` | Host-side UF2 dependency, deliberately prebuilt and passed through `picotool_DIR`. |
| ARM compiler | Arm GNU Toolchain 13.3.Rel1, GCC 13.3.1, Linux x86_64 `arm-none-eabi` archive | Exact archive URL and expected SHA-256 are locked; no OS-default cross compiler. |
| ESP8266 core | 3.1.2; source `210897ef83305496947c4e73c937bab52a33cb48`; released `esp8266-3.1.2.zip` | Use the released package, not a GitHub auto-generated source ZIP. Bundled binary SDK libraries and packaging are part of the input. |
| NONOS binary SDK | `NONOSDK22x_190703` inside that package | Set explicitly; do not let generic-board menus select a different SDK library directory. |
| Xtensa compiler | `3.1.0-gcc10.3-e5f9fec`, GCC 10.3.0 | Exact x86_64 Linux package from the core's official tool-dependency manifest. This is **lx106**, not ESP32 lx6/lx7. |
| ESP build driver | makeEspArduino 6.7.0, `994f259104170f182a623840abe03b742392a275` | Noninteractive Make CLI with explicit core/tool paths. Core 3.1.2's README itself documents this route. |
| mkspiffs | `3.1.0-gcc10.3-e5f9fec`, locked Linux artifact | Required by this Make driver's filesystem-tool validation, even though this scaffold builds no filesystem image. |

Source locators: [P1–P6, E1–E5, M1–M3, A1–A3]. Full hashes and URLs, including
provenance qualifications, are machine-readable in the lock. Core/XTensa/mkspiffs
hashes come from the official ESP8266 package index. The Arm URL is independently
published by Raspberry Pi; the expected Arm archive SHA-256 was read from libhal's
primary packaging recipe. Arm's own checksum URL was located but could not be read
with the available fetch/download tools. **No archive bytes were retrieved and
hashed here.** The acquisition script checks bytes against those expected hashes
before extracting them.

Host probes actually used GCC/G++ 14.2.0 (Debian 14.2.0-19), Clang 17.0.0,
CMake 3.31.6, Ninja 1.12.1, Python 3.13.5, GNU Make 4.4.1 and Perl 5.40.1.
Those versions and executable identities are recorded as observations, not proof
that the old ESP packaging scripts have been tested under every one of them.
The scaffold requires C17/C++17, CMake >=3.20 (use the observed 3.31.6 rather than
silently adopting CMake 4), GNU Make, Perl and a Python with tar extraction filters.
Use Python >=3.12 for the supplied acquisition helper. Record exact host-tool
versions for the first complete build and freeze that tested host image/digest for CI.
A fully pinned OS/container image and target recipe validation remain acceptance work;
the packet does not claim hermetic host reproducibility.

### A dependency easily missed: picotool

SDK 2.2.0's `Findpicotool.cmake` can FetchContent a matching **tag**, then build a
host tool while generating firmware outputs. That is an additional network-resolved
input if left implicit. The proposal explicitly checks out picotool's commit, builds
it outside the ARM build tree and requires its installed package configuration. Its
vendored JSON/whereami sources are already within that commit. Disable libusb,
optional mbedTLS and submodule fetching; keep its checked-in precompiled embedded
data with `USE_PRECOMPILED=ON`. Building that embedded data afresh would enlarge the
cross-toolchain closure for no benefit to this inert RP2040 scaffold. [P3, P5, P6]

## 3. ESP receive interfaces: three different ABIs

| Stack | Supported receive callback | Peer/security interface | Payload and context |
|---|---|---|---|
| Selected Arduino/NONOS ESP8266 | `void (*)(u8 *mac, u8 *data, u8 len)` from `espnow.h` | integer-return APIs, role-based peer arguments; `esp_now_set_self_role`, `esp_now_set_kok` | Platform ceiling 250 bytes. Public header does not establish a precise scheduling or reentrancy contract. |
| ESP8266 RTOS SDK v3.4 | `void (*)(const uint8_t *mac, const uint8_t *data, int len)` from `esp_now.h` | `esp_err_t`, `esp_now_peer_info_t`, `esp_now_set_pmk` | Header defines 250; its actual example says callbacks run in the Wi-Fi task. |
| Modern ESP32 ESP-IDF | Metadata-object receive callback on relevant modern releases | ESP32/IDF-version-specific interfaces | Not an ABI that may be imported into either ESP8266 adapter. v2 1470-byte capability is not this receiver's contract. |

An important correction to a common overgeneralization: **`esp_now.h`,
`esp_now_peer_info_t` and `esp_now_set_pmk` are not universally ESP32-only**;
they exist in the inspected ESP8266 RTOS SDK. They are nevertheless wrong for the
chosen Arduino/NONOS core. Conversely, the modern `esp_now_recv_info_t` metadata
callback is not present in either inspected ESP8266 callback definition. [E1, T1, T2, W1]

For NONOS, include `Arduino.h` or the proper SDK types before `espnow.h`: the latter
requires `u8` and `bool` and does not define them itself. C linkage is already guarded
inside the vendor header. Do not cast an incompatible function pointer to silence
the compiler. The packet preserves the exact fetched header, including its notice,
and verifies Git blob `2e1c2dbf6b35b629459910542045ee32da00c420`. The positive
compile probe accepts its actual callback/peer signatures; two negative probes
reject the RTOS-style and metadata-object-style callbacks.

An 8-bit length does **not** grant a 255-byte payload. The repository's complete
ESP-NOW application datagram, including the future generic envelope, must fit in
250 bytes. ESP-NOW v1 interoperability and the inspected RTOS SDK independently
support that bound. Future envelope overhead must be subtracted from 250, not
added outside it. The prototype rejects zero, 251 and larger sizes before narrowing,
including `SIZE_MAX`. A future RTOS adapter must reject negative `int` lengths before
conversion as well. [R2, E1, T1, W1]

### Callback ownership and scheduling decision

Normalize vendor inputs **inside a target-specific adapter** into an owned, bounded
record containing source MAC, length and bytes. Treat callback pointers as borrowed;
never enqueue the pointers. No target SDK type crosses the shared boundary. Do not
make RSSI, receive-channel metadata, destination MAC, encryption status or an
ESP32 metadata structure mandatory: the chosen callback does not supply them.
Optional future metadata needs explicit presence bits, never invented zero readings.

The implementation contract should be: validate/copy, attempt a nonblocking bounded
handoff, increment a drop counter on full capacity, and return. No `delay`, `yield`,
logging, HID work, framing, dynamic allocation or waiting on space in the callback.
The exact NONOS scheduling priority and reentrancy were **not established** from
public headers and the inspected binary-stack packaging. Calling it a FreeRTOS
Wi-Fi task or promising ISR-safe facilities would substitute evidence from another
stack. FNR-004 must validate the chosen producer/consumer synchronization and reset
lifecycle before installing a real queue. The prepass deliberately contains only the
pure ownership-copy function, not an unproven concurrent queue.

The RTOS example explicitly recommends handing work to a lower-priority task, yet
uses `malloc` and `xQueueSend(..., portMAX_DELAY)` in its example callbacks. Those
are not suitable policies to copy into this bounded generic receiver. [T2]

## 4. Licenses and alternatives that change the decision

Pico SDK has the three-clause BSD notice; TinyUSB uses MIT. Keep component notices,
including picotool's vendored components and compiler runtime terms. The Arduino
core README identifies LGPL core files, a GPL toolchain, and an **Espressif MIT**
NONOS SDK; the actual ESP-NOW header grants use on **ESPRESSIF SYSTEMS ESP8266
only**. It is not ordinary unrestricted MIT, and source availability does not mean
the radio implementation is freely rebuildable. The selected archive is a reproducible
binary input, not a claim to have built the radio stack from source. [L1, E1, E5]

The ESP8285 board definition is supported by this same Arduino core, with DOUT fixed
in its board definition. That is build support, not a substitute for reviewing the
literal chip-use license wording before redistributing an ESP8285 firmware product.
Retain the full vendor terms and review distribution/relink requirements for the
actual binary mixture; do not label the entire receiver or SDK bundle simply MIT.
This is a license inventory, not a legal determination about a particular distribution.

ESP8266 RTOS SDK v3.4 is a real alternative, not ESP32 code in disguise. Its README
specifies the **GCC 8.4.0 / esp-2020r3 lx106** toolchain and the `IDF_PATH`,
`make menuconfig`, `make all` CLI path. Its ESP-NOW header is Apache-2.0, which
must not be generalized to every binary component in that SDK. Selecting it would
replace the scheduler, configuration system, toolchain and adapter ABI. Reject it
for this foundation because none of those extra changes is needed for the current
small radio coprocessor, not because it lacks ESP-NOW. [T1–T3]

Do not choose a floating PlatformIO platform/package resolver in addition to an
already sufficient pinned toolchain. It is not inherently incompatible, but a
platform version alone is not the transitive dependency lock requested here.
makeEspArduino still needs its own source pin and explicit paths; the scaffold sets
an empty user configuration root, disables ccache, fixes board/flash/SDK values and
uses a normal `.cpp` entry point rather than automatic Arduino prototype generation.
Its first-definition-wins recipe parser is why `recipe-overrides.txt` is passed before
the package descriptions. Sketch-global-options processing is explicitly disabled
for this plain-C++ scaffold; that override must be checked against a real first build.
The remaining prebuild/link/elf2bin steps come from the selected package. [M1–M3]

## 5. Architecture and build prototype

```text
Native GCC/CMake -> shared platform + profile headers -> C library/C++ tests
                         ^                         ^
ESP8266 Make -> Arduino/NONOS adapter       RP2040 CMake -> Pico/TinyUSB adapter
  callback bytes -> future bounded slot -> future internal framing -> platform core
                                                             -> profile -> HID adapter
```

`fnr_datagram` is an in-process owned record. It is expressly not a packed wire ABI.
`fnr_profile_view` and `fnr_profile_ops` show SDK-independent dispatch and neutralization
without defining mouse axes, production profile IDs or a new wire format. The core
will perform session/order/freshness admission before profile dispatch; sequencing
must not be smuggled into TinyUSB types or radio-specific headers.

The native root never imports Pico or Arduino. RP2040 gets its own toolchain selection
before `project()`. SDK CMake imports its matching TinyUSB `rp2040` family support and
exports `tinyusb_device`; the adapter checks the TinyUSB version macros. The Pico
entry loops without initializing USB, UART, GPIO or any profile. Null descriptor callbacks
are link-only placeholders, **not valid USB descriptors**. A volatile function-pointer
anchor intentionally makes a real device-stack symbol link-visible without calling it.

The ESP entry similarly references the real registration symbol but never registers
callbacks or starts ESP-NOW. Arduino core 3.x documents Wi-Fi as off at boot unless
legacy startup is deliberately restored; this scaffold does not restore it. `loop()`
yields through a modest delay. There is no AP, peer admission, radio channel choice,
UART pin initialization or artificial HID report in either inert entry. These are
source-level intentions until a physical boot is actually tested. [P2, E1, W2]

Use `PICO_BOARD=pico` only as an inert RP2040 compile surrogate. Use the `esp8285`
variant, 80 MHz CPU, 1 MiB/no-filesystem linker layout, DOUT and 40 MHz flash as a
**compile surrogate**, not as a measured clone specification. The same core and lx106
compiler serve ESP8266; a later measured ESP8266 board can use `BOARD=generic` with
explicit corresponding values. Never flash either surrogate based on this prepass.
Pins, crystal/flash measurements and upload/reset method remain FNR-002 work. [R3, E3]

## 6. Exact acquisition and configuration

Run from the root of a checkout with the proposed overlay applied, on Linux x86_64:

```sh
# Network step; verifies source IDs and archive digests before extraction.
python3 tools/acquire_deps.py --lane all
# Explicit verification; rejects modified extracted trees/checkouts.
python3 tools/check_deps.py all

# SDK-free native lane.
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build build/native --parallel 2
ctest --test-dir build/native --output-on-failure

# Independent ARM lane; prebuilds pinned host picotool, then builds ELF/UF2.
bash tools/build_rp2040.sh

# Independent lx106 lane; plain .cpp input, no Arduino IDE or PlatformIO install.
export FNR_SOURCE_ID="$(git rev-parse HEAD)+$(sha256sum proposed.patch | cut -d' ' -f1)"
bash tools/build_esp.sh
```

The last source-ID example assumes the packet patch is available as `proposed.patch`;
a committed implementation should instead record its actual commit and clean/dirty
state, plus any uncommitted patch digest. Do not silently claim the assessed base is
the implementation commit. `FNR_DEPS` can select an isolated dependency cache.
Neither build wrapper uploads/flashes hardware or mutates GitHub. CMake has no
network bootstrap. The ESP wrapper does not install or resolve a live package index.

The package's Linux `python3` dependency labeled `3.7.2-post1` is a tiny **via-env
wrapper**, not a pinned Python interpreter. Avoid interpreting that label as host
Python reproducibility. Here the build wrapper explicitly resolves the host interpreter;
its exact version must be recorded, and the complete ESP recipe run is still required.
The stock Arduino index also lists mklittlefs: it is unnecessary for this no-filesystem
Make lane, so it is not silently fetched. [E4, M1]

## 7. Executed tests and evidence classification

`run_probes.py` is the finite offline runner; results are in `evidence/probes/results.json`.
Each command has its own log and timeout. The native fixture tests all 513 lengths
from 0 through 512: 250 accepted, 263 rejected. It additionally checks three null
arguments, `SIZE_MAX`, unchanged output on rejected lengths, copied source MAC,
zeroed unused bytes and ownership after the source buffers change. C code links to
C++ consumers with undefined-behaviour sanitizer instrumentation and hard failure.

Poison SDK headers make accidental shared-layer imports fail. Clang produces real
Cortex-M0+ freestanding objects from the portable headers and C source; `readelf`
confirms a 32-bit ARM relocatable object. That proves useful target data-model and
language compatibility, not Pico SDK/TinyUSB linking or electrical compatibility.
The upstream ESP-NOW header is compile-tested without pretending the single `u8`
prelude is an implementation of Arduino or the closed radio libraries.

**Unexecuted:** full SDK+TinyUSB firmware link/UF2 generation, ESP core+NONOS link/BIN
generation, picotool host build, first makeEspArduino recipe resolution against the
full released core, native Windows builds, firmware double-build comparisons,
USB enumeration, RF callbacks, packet loss/latency, inter-MCU link and all clone
hardware checks. Both target preflight attempts failed clearly because dependencies
were absent; those logs are retained. No CI was dispatched or polled.

## 8. Manifest and shortest implementation sequence

`tools/artifact_manifest.py` records source identity, assessed base, dependency-lock
hash, compiler executable hash/full version output, host identity, command/configuration,
artifact sizes/digests and an explicit false physical-acceptance field. The example
configuration carries board/flash provenance, profile selection and three separate
protocol/schema fields; unassigned protocol versions are `null`, not invented `1`s.
Add the actual clean/dirty flag, patch SHA-256, acquired-dependency ledger digest,
resolved CMake cache/Arduino recipes, compile/link flags, map/size output, host-image
digest and license-notice inventory to the release manifest. Never record keys.
Hash ELF/BIN/UF2, test logs and the lock; distinguish `built`, `host-tested`,
`hardware-tested` and `unexecuted`. The supplied writer is an executable minimal
prototype, not a complete release acceptance validator.

1. **Native first:** land the SDK-free C/C++ boundaries, pure ownership helper and
   native build/tests. Keep all wire/profile IDs unset. Preserve the poisoned-header
   and foreign-callback rejection probes as architecture regression checks.
2. **Target builds next, as independent lanes:** acquire the fixed set, verify every
   ledger/commit, build picotool then RP2040 ELF/UF2, and resolve/build the ESP8266
   plain-C++ recipe to ELF/BIN. Inspect required linked symbols, exact NONOS library
   path, flash layout, no unexpected Wi-Fi startup and no SDK includes in shared.
   Repair any actual target recipe/link failures before claiming the set accepted;
   these cannot be ruled out by the host probes. No functional receiver implementation
   is necessary to reach this gate.
3. **Evidence/CI last:** freeze the successful canonical host environment, run two clean
   firmware builds with the same source/tool inputs, investigate byte differences
   (timestamps, paths, build-info and SDK metadata), emit complete manifests/notices,
   and install native/RP2040/ESP plus text/style CI gates in a separately write-authorized
   turn. Until that succeeds, FNR-001 is not complete. Physical configuration and
   receiver behaviour stay with their existing owning issues.

The remaining questions are narrow: complete tool execution and host-image closure;
NONOS callback synchronization and reset ownership; measured board parameters; and
license/distribution treatment of the actual firmware mixture. They are not an invitation
to reopen the SDK, radio stack, callback ABI, generic header boundary or build-driver choices.

## Source locators

- **R1** — https://github.com/techrote/faikeow-now-reciever/issues/2
  Locator/use: FNR-001 issue; tracker #1 also read.
- **R2** — https://github.com/techrote/faikeow-now-reciever/blob/8cc7a4c109a099466d8389a4e3b4004bfdf44c01/docs/02-PROTOCOLS.md
  Locator/use: Platform payload <=250; not a frozen implemented wire ABI.
- **R3** — https://github.com/techrote/faikeow-now-reciever/blob/8cc7a4c109a099466d8389a4e3b4004bfdf44c01/docs/03-HARDWARE.md
  Locator/use: Hardware uncertainty and profile-independent roles.
- **R4** — https://github.com/techrote/faikeow-now-reciever/blob/8cc7a4c109a099466d8389a4e3b4004bfdf44c01/docs/05-BUILDING.md
  Locator/use: Build lanes; also AGENTS/RAG/docs00-08 read.
- **P1** — https://api.github.com/repos/raspberrypi/pico-sdk/git/ref/tags/2.2.0
  Locator/use: SDK exact commit; contents/lib gives TinyUSB gitlink.
- **P2** — https://github.com/raspberrypi/pico-sdk/blob/a1438dff1d38bd9c65dbd693f0e5db4b9ae91779/src/rp2_common/tinyusb/CMakeLists.txt
  Locator/use: PICO_TINYUSB_PATH, family integration, tinyusb_device.
- **P3** — https://github.com/raspberrypi/pico-sdk/blob/a1438dff1d38bd9c65dbd693f0e5db4b9ae91779/tools/Findpicotool.cmake
  Locator/use: Implicit FetchContent and default mutable SDK-version tag.
- **P4** — https://github.com/hathach/tinyusb/blob/86ad6e56c1700e85f1c5678607a762cfe3aa2f47/src/tusb_option.h
  Locator/use: lines32-35 version 0.18.0.
- **P5** — https://github.com/raspberrypi/picotool/blob/a7eb3988f0645239185fadb4e25d8279478c2dbb/CMakeLists.txt
  Locator/use: Flat install, USE_PRECOMPILED, no-libusb and SDK >=2.1.0.
- **P6** — https://github.com/raspberrypi/picotool/blob/a7eb3988f0645239185fadb4e25d8279478c2dbb/lib/CMakeLists.txt
  Locator/use: Vendored dependencies; mbedTLS presence changes features.
- **E1** — https://github.com/esp8266/Arduino/blob/210897ef83305496947c4e73c937bab52a33cb48/tools/sdk/include/espnow.h
  Locator/use: Exact blob 2e1c2dbf6b35b629459910542045ee32da00c420; ABI and chip-use notice.
- **E2** — https://github.com/esp8266/Arduino/blob/210897ef83305496947c4e73c937bab52a33cb48/platform.txt
  Locator/use: C17/C++17, NONOSDK22x_190703 default, -lespnow, elf2bin recipe.
- **E3** — https://github.com/esp8266/Arduino/blob/210897ef83305496947c4e73c937bab52a33cb48/boards.txt
  Locator/use: generic and esp8285 definitions; esp8285 fixed DOUT/40 MHz; 1M no-filesystem layout.
- **E4** — https://arduino.esp8266.com/stable/package_esp8266com_index.json
  Locator/use: 3.1.2 platform plus Linux x86_64 tool records; extracted selected fields, not frozen full live index.
- **E5** — https://github.com/esp8266/Arduino/blob/210897ef83305496947c4e73c937bab52a33cb48/README.md
  Locator/use: makeEspArduino section and License and credits.
- **T1** — https://github.com/espressif/ESP8266_RTOS_SDK/blob/v3.4/components/esp8266/include/esp_now.h
  Locator/use: ESP8266 RTOS API; 250 bytes, typed peers/PMK; Apache header.
- **T2** — https://github.com/espressif/ESP8266_RTOS_SDK/blob/v3.4/examples/wifi/espnow/main/espnow_example_main.c
  Locator/use: lines64-111 Wi-Fi task, malloc/portMAX_DELAY example; blob ede61bdad72424c6eae7bd3fc7e46c8d20721a2b.
- **T3** — https://github.com/espressif/ESP8266_RTOS_SDK/blob/v3.4/README.md
  Locator/use: GCC8.4.0 toolchain, IDF_PATH, make menuconfig/all.
- **M1** — https://github.com/plerup/makeEspArduino/blob/994f259104170f182a623840abe03b742392a275/makeEspArduino.mk
  Locator/use: Explicit paths, mkspiffs validation, configuration isolation and build-info variables.
- **M2** — https://github.com/plerup/makeEspArduino/blob/994f259104170f182a623840abe03b742392a275/tools/parse_arduino.pl
  Locator/use: First-definition-wins property expansion, explicit compiler/Python path fallbacks.
- **M3** — https://api.github.com/repos/plerup/makeEspArduino/git/ref/tags/6.7.0
  Locator/use: Exact build-driver commit.
- **A1** — https://github.com/raspberrypi/pico-vscode/blob/main/data/0.17.0/supportedToolchains.ini
  Locator/use: Arm 13.3.Rel1 canonical URL; inspected blob c2c4eeaf2afe0fa2b8a2bc4e1e50cca25e88d659.
- **A2** — https://github.com/libhal/arm-gnu-toolchain/blob/main/all/conandata.yml
  Locator/use: 13.3/Linux/x86_64 expected archive SHA-256; packaging primary source, not an independently downloaded Arm archive.
- **A3** — https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/13.3.rel1/binrel/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi.tar.xz.sha256asc
  Locator/use: Located but unavailable to content reader; NOT read or byte-verified.
- **L1** — https://github.com/raspberrypi/pico-sdk/blob/a1438dff1d38bd9c65dbd693f0e5db4b9ae91779/LICENSE.TXT
  Locator/use: BSD-3-Clause source and binary notice.
- **W1** — https://docs.espressif.com/projects/esp-faq/en/latest/application-solution/esp-now.html
  Locator/use: v1 vs v2 payload interoperability only; do not import ESP-IDF APIs into NONOS.
- **W2** — https://arduino-esp8266.readthedocs.io/en/3.1.2/esp8266wifi/generic-class.html
  Locator/use: persistent section: core3 disables WiFi at boot unless legacy startup restored.
