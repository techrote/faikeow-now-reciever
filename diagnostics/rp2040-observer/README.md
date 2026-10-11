# FNR-002 temporary input-only observer

This is diagnostic firmware, separate from the production targets. The default
build runs in SRAM. GPIO0–29 stay inputs with pulls disabled. It contains no
radio TX/reset/download command and no flash program/erase operation.
`INFO` reads only JEDEC 0x9f and flash unique-ID 0x4b. These ROM/SSI transactions
temporarily leave XIP mode; all executing code is in RAM.

Commands over USB CDC (newline terminated):

- `INFO`: JEDEC ID, flash unique ID, GPIO levels, output-enable mask.
- `EDGES ms`: sampled transition counts for all GPIOs, 1–10000 ms. This is a
  correlation aid, not a calibrated logic analyser or guaranteed edge counter.
- `RX pin baud ms`: PIO input-only 8N1 capture; 4096-byte bounded storage,
  explicit discarded count and sticky framing-error indication. A sticky flag
  is not an error count. USB output occurs after capture, not per received byte.
- `BOOTSEL`: return to the RP2040 ROM bootloader without changing flash.

Build with the FNR-001 SDK/compiler pins, `PICO_BOARD=pico` (no radio resources
selected), and the prebuilt/pinned picotool CMake package:

```powershell
$env:PICO_SDK_PATH = (Resolve-Path .deps/pico-sdk).Path
# Add the verified Arm 13.3.Rel1 compiler, CMake and Ninja to this shell's PATH.
cmake -S diagnostics/rp2040-observer -B build/observer -G Ninja `
  -DPICO_PLATFORM=rp2040 -DPICO_BOARD=pico -DPICO_NO_FLASH=1 `
  -Dpicotool_DIR=<absolute-picotool-package-directory> -DCMAKE_BUILD_TYPE=Release
cmake --build build/observer
python tools/hardware/check_ram_uf2.py build/observer/fnr_observer.uf2
```

Before loading, independently verify exactly one target and its saved USB
topology, record source commit and image SHA-256, and run the SRAM UF2 guard.
Use the observed flash ID for picotool `--ser` when available. RP2040 BOOTSEL USB
serial strings are not unique; picotool 2.2.0 uses flash ID for that selector.
For an erased flash where ID is not yet available, use the observed libusb bus
and address **only after** verifying the single target's physical USB topology.
The Windows USB port number is not the libusb device address.

`picotool load -v -x build/observer/fnr_observer.uf2 <device-selection>` loads
SRAM only after the guard passes. Cold power loss removes the diagnostic;
the original flash remains installed. Record readback after return to BOOTSEL
before claiming preservation. This tool does not establish radio wiring or
link reliability by merely compiling or enumerating.
