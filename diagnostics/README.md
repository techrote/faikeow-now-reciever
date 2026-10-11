# FNR-002 temporary hardware diagnostics

These programs are isolated characterization tools, not accepted production
transport or HID firmware. Both boards passed the FNR-002 identity, recovery
and UART hardware scope. Additional USB/RF subsets retain their separate
acceptance limits in the evidence matrix. For reusable build/deployment
procedures, use the authoritative
[devboard operating runbook](https://github.com/techrote/picoWcloneESP8285N08/blob/main/docs/DEVBOARD-OPERATING-RUNBOOK.md)
and [programming reference](https://github.com/techrote/picoWcloneESP8285N08/blob/main/docs/DEVBOARD-PROGRAMMING-REFERENCE.md).
[Hardware integration](../docs/03-HARDWARE.md) links the qualified reference
baseline. Diagnostic sources and guarded host helpers remain in this repository.

- `rp2040-observer`: SRAM-only input observer. Drives no GPIO.
- `rp2040-candidate-bridge`: SRAM-only CDC/UART0 bridge, GP0 TX and GP1 RX.
  Independent radio ROM communication and backups established this route on A/B.
- `rp2040-link`: SRAM-only generator/checker for the isolated UART integrity test.
- `esp-link/esp_link`: ESP8285 flash-resident counterpart. No networking or
  credentials; Wi-Fi is disabled. **A: write/readback/cold boot, ten-minute
  integrity and software fault/recovery cases passed; original radio image
  previously restored exactly. B also passed write/readback, full original
  restoration, diagnostic redeployment and cold application identity. B's
  sustained/fault qualification is recorded separately in the evidence ledger.**

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

Native Windows build prerequisites and hardware procedures are maintained in
the dedicated devboard reference. Their original campaign identities remain
recorded in the local historical `docs/BOARD-CONTRACT.md` and evidence ledger.
For the ESP diagnostic, use portable Arduino CLI 1.5.1
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
channel, backpressure and restart cases. Both directions passed positive
integrity, admission, backpressure and restart cases. One reverse channel
isolation case failed and reproduced; see the separate RF evidence, not an
overall RF PASS. Optional `--direction` and repeated `--case` selections run
only remaining/focused cases without overwriting earlier captures.

The FNR-004 power-control diagnostic adds `POWER <quarter-dBm-ceiling>` with
range 0–82, accepted only while no transmission is pending. The requested SDK
ceiling is reapplied after RX/TX initialization and reported as
`power_limit_qdbm`; it is not a calibrated measurement of radiated power.
Status also reports SDK mode, station connection status, auto-connect,
auto-reconnect and sleep type without exposing saved credentials. The
production receive adapter remains unchanged.

Focused captures accept `--receive-channel`, `--mismatch-channel`,
`--mismatch-count`, `--separation-cm` and optional `--power-limit-qdbm`.
Channels are bounded to 1–14; negative-case count to 1–20,000. Record only an
operator-confirmed separation and select channels appropriate to the test
location. The power option requires this new diagnostic and its own exact
deployment/readback records on both radios; it cannot be used with the original
FNR-002 image. Omitting it preserves compatibility with that image.

Negative channel assessment still requires zero received/accepted/checked
packets and zero successful transmit acknowledgements, complete callbacks,
unchanged final SDK channels and, when requested, matching final reported power
ceilings. A failed sender callback cannot erase an observed receive event.
Use same-channel positive controls at the same power and geometry, preserve
every failing comparison, and stop transmissions/close serial handles after
each capture. Channel choice is not authentication or a proven exclusion
mechanism at every separation and power.
For two-board access, the candidate bridge can be built with explicit fixture
labels A/B and `FNR_BRIDGE_FLASH_RECOVERY_TEST=1`, `PICO_NO_FLASH=0`. This optional
flash image requires complete originals, measured geometry and the guarded RP
flash helper; the default candidate bridge remains SRAM-only. Labels distinguish
USB interfaces but never replace independent topology/MCU identity checks.

For additional Windows USB observations, unchanged accepted FNR-003 firmware
can be built with `PICO_NO_FLASH=1` and loaded using `load_ram.py`. Its verified
SRAM deployment record is accepted by `hid_probe_windows.py --ram-load-record`;
the persistent path still requires an exact flash readback record.
`usb_descriptors_windows.py` reads device and configuration descriptors from
the mapped hub port. Its Windows hub API cannot retrieve the interface-recipient
HID report descriptor. Parsed HID capabilities remain separate evidence.
`hid_idle_windows.py` observes the exact mapped raw mouse for 20 or 60 seconds
using a process-local Raw Input sink, without injecting or suppressing input.
An idle PASS means no unsolicited motion in that interval; it does not prove
a neutral report, active controls, suspend/resume or timeout behavior.
