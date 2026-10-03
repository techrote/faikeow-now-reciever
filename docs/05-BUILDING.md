# Building and repository layout

## Intended layout

FNR-001 may refine names, but preserve the separation:

```text
/
  README.md
  RAG.md
  AGENTS.md
  CMakeLists.txt
  shared/
    include/
    src/
  rp2040/
    CMakeLists.txt
    src/
    include/
  esp8266/
    src/
    include/
    build metadata/
  tests/
    native/
    fixtures/
  tools/
  docs/
```

Do not mix target SDK headers into portable protocol/state files.

## RP2040 toolchain

Preferred:

- Raspberry Pi Pico SDK;
- CMake + Ninja;
- TinyUSB through the SDK/pinned dependency;
- `arm-none-eabi` GCC supported by the selected Pico SDK.

FNR-001 must select an exact stable Pico SDK revision/release and record all relevant TinyUSB submodule versions. Do not track `master`.

Production output should include a UF2 plus a build manifest/checksum.

## ESP8266/ESP8285 toolchain

Preferred starting point:

- maintained ESP8266 Arduino core or another currently buildable ESP8266 SDK with direct ESP-NOW support;
- reproducible CLI build suitable for CI;
- exact pinned version/commit.

Public ESP8266 Arduino SDK headers expose the classic ESP-NOW receive API and peer/channel controls. FNR-001 must verify the chosen pinned release by compiling a minimal receiver before committing to it.

Do not depend on a desktop Arduino IDE state that CI cannot reproduce.

Production output should include a flashable binary plus manifest/checksum.

## Native tests

Shared packet/state logic must build without either target SDK.

Recommended pattern:

```sh
cmake -S . -B build-native -G Ninja -DFNR_BUILD_NATIVE_TESTS=ON
cmake --build build-native
ctest --test-dir build-native --output-on-failure
```

Exact commands are frozen by FNR-001.

## Hardware flashing

FNR-002 freezes the actual process.

Expected recovery shape from public clone prior art:

1. put RP2040 into BOOTSEL;
2. flash temporary USB-to-serial bridge UF2;
3. put ESP radio into ROM download mode using the radio button/strap;
4. flash ESP binary with `esptool` or pinned equivalent;
5. restore production RP2040 UF2.

Do not publish this as exact for the user's boards until FNR-002 proves it.

## CI

FNR-001 should create jobs equivalent to:

- Native tests;
- RP2040 firmware build;
- ESP radio firmware build;
- Text/style sanity.

Artifacts should carry the source commit and checksums.

## Secrets/configuration

Never commit real production keys as defaults.

Test vectors may use clearly labeled non-secret fixed keys if encryption logic requires deterministic testing.

Receiver MAC/channel/peer/key configuration should be build-time or persisted runtime configuration as defined by FNR-007.
