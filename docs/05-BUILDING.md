# Building the FNR-001 dual-firmware foundation

**Scope:** compile-only, inert and profile-neutral. No USB enumeration, ESP-NOW
configuration, board GPIO/pin assertions or real RF/USB behavior is implemented.
The actual clone-board contract remains FNR-002 (#3).

## Layout (implemented)

- `shared/include/fnr/`, `shared/src/` — portable datagram ownership and a
  profile extension seam, with neither target SDK imported.
- `firmware/rp2040/` — Pico SDK 2.2.0 + TinyUSB 0.18.0 inert UF2 target.
- `firmware/esp8266/` — ESP8266 Arduino 3.1.2 (NONOSDK22x_190703) inert
  radio API/link target.
- `tests/native/` — sanitizer-testable portable code.
- `tools/` — verified dependency acquisition, target builds and artifact
  manifests.
- `.github/workflows/fnr-001.yml` — four CI gates and firmware artifacts.

Later FNR issues own full protocol codecs, profile backends, radio callbacks,
inter-MCU transport, and board-specific flashing/recovery. Empty placeholder
modules are not interpreted as implemented functionality.

## Required host

Use Linux x86_64 (Ubuntu 24.04 in CI; WSL2 also supported if it has network).
Install Git, Python 3.11+, CMake, Ninja, GCC/G++, Bash and standard archive tools.

The exact firmware dependencies are in `dependencies.lock.json`. See
`docs/prepasses/FNR-001-DEPENDENCIES.md` for source identities, hashes,
licenses, acquisition risks and original prepass limitations.

Dependencies are fetched **only** by an explicit acquisition command.
They are pinned and SHA-256 verified where binary archives are involved,
and installed under ignored `.deps/`:

```sh
python3 tools/acquire_deps.py --lane rp2040
python3 tools/acquire_deps.py --lane esp
```

Both can be combined with `--lane all`. Acquisition requires internet
access; the actual build scripts do not fetch SDKs.

## Native tests

```sh
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=undefined -fno-sanitize-recover=all" \
  -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-sanitize-recover=all"
cmake --build build/native --parallel 2
ctest --test-dir build/native --output-on-failure
```

These tests currently cover datagram byte ownership, limits and malformed input.
Expanded core/transport/profile coverage belongs to later FNR issues.

## Firmware cross-builds

```sh
bash tools/build_rp2040.sh
FNR_SOURCE_ID="$(git rev-parse HEAD)" bash tools/build_esp.sh
```

Output includes `build/rp2040/fnr_rp2040_inert.uf2` and
`build/esp8266/fnr_esp8266_inert.bin`, with corresponding ELF files.
The build script verifies pinned local dependencies before compilation.

**Do not flash these as functional receiver firmware.** They deliberately
do not initialize TinyUSB or register an ESP-NOW callback. The `pico`
RP2040 board profile and `esp8285` 1 MiB `dout` ESP flash profile are
*compile surrogates*, not physical board acceptance or flash instructions.

## Manifest / CI evidence

Four CI jobs must pass on the exact PR head:

1. Text/JSON/Python sanity and tracked-key-material check.
2. Native UBSan tests.
3. Locked Pico SDK/TinyUSB build and UF2/ELF hashes.
4. Locked ESP8266 NONOS SDK build and BIN/ELF hashes.

The cross-build jobs publish SHA256SUMS and `manifest.json` files
beside firmware images. The manifest records source ID, locked dependency
file hash, compiler identity, build profile and artifact hashes, and
`physical_acceptance: false`. The CI source ID uses the PR source head
rather than GitHub's synthetic merge-commit SHA.

Artifacts may be byte-for-byte compared between clean builds with the
same source identity. Do not claim reproducibility from mere successful
compilation or record a firmware hash without the exact manifest.

## Failure handling

- A checksum mismatch is a hard stop, not a reason to silently update
  `dependencies.lock.json`.
- A wrong upstream source commit is a hard stop.
- Use `python3 tools/check_deps.py <lane>` to revalidate local identities.
- Keep `.deps/`, `build/`, credentials and flashing artifacts out of Git.
- FNR-002 owns the actual board, UART pinout, boot and flash recovery evidence.
