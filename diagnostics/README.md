# FNR-002 temporary hardware diagnostics

These programs are isolated characterization tools, not accepted production
transport or HID firmware. Hardware acceptance remains open until the evidence
matrix records completed physical tests for both boards.

- `rp2040-observer`: SRAM-only input observer. Drives no GPIO.
- `rp2040-candidate-bridge`: SRAM-only CDC/UART0 bridge, GP0 TX and GP1 RX.
  Board A ROM communication has established this route; Board B is untested.
- `rp2040-link`: SRAM-only generator/checker for the isolated UART integrity test.
- `esp-link/esp_link`: ESP8285 flash-resident counterpart. No networking or
  credentials; Wi-Fi is disabled. **Not yet flashed or physically tested.**

The link test uses bounded SLIP frames, reflected CRC-32/ISO-HDLC, fixed seed
`0x002f4a91`, sequence numbers, and cycling payload lengths 1/32/128/250 bytes.
Both MCUs generate traffic independently at a nominal 20 frames/second; the
radio also echoes validated RP2040 requests. RP2040 validates every echo and
radio-generated payload. UART and software-buffer errors are counted separately.
Radio stats are: accepted requests, pattern errors, sequence discontinuities,
streams sent, UART overruns, UART RX errors, CRC errors, length errors, and parser
overflows. These are diagnostics, unrelated to FNR-005's production framing.

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
