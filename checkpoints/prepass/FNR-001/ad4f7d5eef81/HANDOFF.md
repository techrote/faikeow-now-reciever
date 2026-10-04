# FNR-001 dependency/toolchain prepass handoff

Assessed repository: `techrote/faikeow-now-reciever`, issue #2 / FNR-001; tracker #1.
Assessed `main`: **8cc7a4c109a099466d8389a4e3b4004bfdf44c01**.
Assessed tree: `e1f4d164b88d01dee3cc1c77ecebb98ac25910d8`.
This is a local prepass/proposed additive patch, not a Git checkpoint or accepted implementation.
No GitHub mutations, workflow dispatches, comments, or hardware operations were performed.

## Decisions
Use Pico SDK 2.2.0 and its exact TinyUSB 0.18.0 gitlink, picotool 2.2.0,
Arm GNU 13.3.Rel1; ESP8266 Arduino core 3.1.2 release package with
NONOSDK22x_190703, its GCC 10.3 package, and makeEspArduino 6.7.0.
The lock contains full source IDs and release-package SHA-256 values with provenance.
No ESP32 callback ABI, no floating SDK branch, no PlatformIO resolver, no live package index.

## Intended paths
`proposed/` is a repository-root overlay: root `CMakeLists.txt`, `dependencies.lock.json`,
`.gitignore`; `shared/include/fnr/`, `shared/src/`, `tests/native/`,
`firmware/rp2040/`, `firmware/esp8266/`, `tools/`, and
`docs/prepasses/FNR-001-DEPENDENCIES.md`. `proposed.patch` contains those additions only.
The remaining packet files are evidence, fixtures and handoff material, not active firmware paths.

## Evidence and limits
`python3 run_probes.py --out /tmp/fnr-probes` reruns offline tests.
Native C/C++ tests, SDK isolation, Cortex-M0+ freestanding object compilation and
positive/negative ABI probes passed. The original ESP8266 header matches its upstream Git blob.
Full RP2040 SDK/TinyUSB ELF/UF2 and ESP8266 ELF/BIN builds were **NOT RUN**:
neither toolchain/SDK was locally installed; shell network retrieval failed.
Release digests were verified against published metadata, not downloaded archive bytes.
Do not label FNR-001 complete or flash the inert surrogate images.

## Next work
Apply the patch in a real checkout after checking its base and path collisions. Run both
locked target builds on Linux x86_64; retain complete link/size logs and artifact manifests.
Check makeEspArduino's resolved recipes (especially prebuild hooks) and perform two clean
firmware rebuilds before claiming binary reproducibility. Add real CI jobs only in a write-authorized phase.
FNR-002 still owns clone pinout/flash measurements; FNR-003 USB identity/descriptors;
FNR-004 radio synchronization/role/channel/peer admission; FNR-005/006 framing and schema.
The NONOS callback's precise scheduling/reentrancy contract was not established from public
headers; do not substitute the RTOS Wi-Fi-task guarantee. Licensing review must retain
LGPL and vendor chip-use conditions, including the literal ESP8266-only notice.
