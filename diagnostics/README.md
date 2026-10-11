# FNR-002 temporary hardware diagnostics

These programs are isolated characterization tools, not accepted production
transport or HID firmware. Hardware acceptance remains open until the evidence
matrix records completed physical tests for both boards.

- `rp2040-observer`: SRAM-only input observer. Drives no GPIO.
- `rp2040-candidate-bridge`: SRAM-only CDC/UART0 bridge, GP0 TX and GP1 RX.
  Board A ROM communication has established this route; Board B is untested.
- `rp2040-link`: SRAM-only generator/checker for the isolated UART integrity test.
- `esp-link/esp_link`: ESP8285 flash-resident counterpart. No networking or
  credentials; Wi-Fi is disabled. **A: write/readback/cold boot, ten-minute
  integrity and software fault/recovery cases passed; original radio image
  previously restored exactly. A currently runs this diagnostic. B: pending.**

The link test uses bounded SLIP frames, reflected CRC-32/ISO-HDLC, fixed seed
`0x002f4a91`, sequence numbers, and cycling payload lengths 1/32/128/250 bytes.
Both MCUs generate traffic independently at a nominal 20 frames/second; the
radio also echoes validated RP2040 requests. RP2040 validates every echo and
radio-generated payload. UART and software-buffer errors are counted separately.
Radio stats are: accepted requests, pattern errors, sequence discontinuities,
streams sent, UART overruns, UART RX errors, CRC errors, length errors, and parser
overflows. These are diagnostics, unrelated to FNR-005's production framing.
The host `tools/hardware/link_capture.py` checks target topology and the radio
write/readback record, captures bounded JSON lines, and rejects missing final
markers, short duration, insufficient frames, differing MCU counters and any
unexplained error/drop counter. Its outcome is smoke-only for a ten-second run.

`RUN10` starts a smoke test; `RUN600` starts a 600-second test plus a two-second
drain and final stats request. Capture CDC JSON output privately. Acceptance
requires at least 10,000 requests/echoes and radio streams, matching radio stats,
zero unexplained integrity/errors/drops, and exact board/firmware provenance.
`INFO` requests identity; `BOOTSEL` returns RP2040 to its ROM; `RADIORESET`
restarts the radio application only while idle. It does not enter radio ROM.
Physical far-button power cycling remains the observed radio-download method.

Native Windows build prerequisites and their identities are recorded in
`docs/BOARD-CONTRACT.md`. For the ESP diagnostic, use portable Arduino CLI 1.5.1
with core `esp8266:esp8266@3.1.2` and its GCC 10.3.0 Windows package:

```powershell
arduino-cli --config-file <private-portable-config> compile `
  --fqbn 'esp8266:esp8266:esp8285:xtal=80,CrystalFreq=26,eesz=1M,wipe=none' `
  --build-property 'compiler.cpp.extra_flags=-I<absolute-repository-path>/diagnostics/common' `
  --build-path build/esp-link diagnostics/esp-link/esp_link
```

Use the observer CMake procedure for `rp2040-link`, a separate build directory,
and `PICO_NO_FLASH=1`. Validate its UF2 with `check_ram_uf2.py` before loading.
Build success does not establish physical test success. Use `radio_flash.py`
only with reviewed image hashes, two validated complete original backups,
positive target identity, ROM access and an available physical restoration step.

`tools/hardware/load_ram.py` checks the mapped ROM topology, flash UID,
picotool/image SHA-256 and every SRAM UF2 destination before loading.
`uart_fault_capture.py` records bounded burst, damaged/interrupted frame and
independent MCU restart cases against the already-deployed UART diagnostic.
It returns RP2040 to ROM with the bridge's 1200-baud request, reloads SRAM and
checks recovery. Radio restart is an application command. No flash or unknown
GPIO write occurs in this fault campaign; deliberate reset losses are reported
separately from clean integrity runs.

The isolated `esp-rf/esp_rf` diagnostic uses the accepted FNR-004 ingress adapter
with runtime-only exact-peer/channel configuration and bounded deterministic
datagrams. `rf_pair_capture.py` prepares two-direction integrity, admission,
channel, backpressure and restart cases. This RF campaign has not run yet.
For two-board access, the candidate bridge can be built with explicit fixture
labels A/B and `FNR_BRIDGE_FLASH_RECOVERY_TEST=1`, `PICO_NO_FLASH=0`. This optional
flash image requires complete originals, measured geometry and the guarded RP
flash helper; the default candidate bridge remains SRAM-only. Labels distinguish
USB interfaces but never replace independent topology/MCU identity checks.
