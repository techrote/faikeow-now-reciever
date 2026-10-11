# Clone-board contract — FNR-002, partial physical evidence

**FNR-002 is OPEN. Neither board has passed reversible flash deployment or
sustained link qualification. Do not generalize Board A's measurements to B.**
Observations below were made on 2026-10-11 with native Windows 11 Pro USB access.
The source/test ledger is in `evidence/FNR-002/`; original dumps, host inventory,
complete device identifiers and raw serial logs remain outside Git.

## Physical identity and preservation

The user designated two visually identical boards A and B and reported that one
had previously been used. Board markings/revision and electrical measurements
have not been captured. Cable/port isolation established A at USB root port 1
and B at root port 3 on the same observed hub. The complete host paths are in
the private `board-identities.json`; they are checked before hardware commands.
COM numbers and drive letters are enumeration observations, not board identities.

| Property | Board A | Board B |
|---|---|---|
| RP2040 identity | ROM identifies RP2040 B2 | ROM identifies RP2040 B2 |
| BOOTSEL | Physical near-USB button/reconnect observed | Physical near-USB button/reconnect observed |
| BOOTSEL USB | 2e8a:0003, RPI-RP2; UF2 bootloader 3.0 | Same |
| BOOTSEL serial | Shared with B; unsuitable for selection | Shared with A; unsuitable for selection |
| RP flash unique ID | Privately recorded; distinct from B | Privately recorded; distinct from A |
| RP flash JEDEC | `68 40 15`, consistent with 2 MiB; exact part unverified | Not measured |
| RP original contents | Erased (`ff`); two matching 2 MiB reads | Existing `picowutooth`; two matching 2 MiB reads |
| Radio identity | ESP8285N08 from esptool efuse/ROM commands | NOT TESTED |
| Radio crystal | esptool reports 26 MHz | NOT TESTED |
| Radio flash | JEDEC `5e 32 14`, detected 1 MiB embedded flash | NOT TESTED |
| Radio MAC | Read and privately recorded | NOT TESTED |
| Radio backup | Two matching 1 MiB reads, stub MD5 verified | NOT TESTED |
| Persistent writes in this campaign | Radio diagnostic/full original; accepted RP mouse-only image | None |
| Restoration | Radio bytes PASS; factory boot unverified; RP restore pending | NOT TESTED |

The first A preread covered the 16 MiB readable RP2040 XIP window, all `ff`.
This is an address-window observation, **not evidence of a 16 MiB flash chip**.
The later explicit 2 MiB backup uses the capacity consistent with JEDEC `15`.
Exact flash part names and B's independently detected capacity remain pending.
B's existing image declares SDK 2.3.1 and `pico_w`; that metadata does not prove
CYW43 hardware or qualify the installed application for this project.

## Interconnect established on A

| RP2040 function | Physical route | Evidence |
|---|---|---|
| UART0 TX, GPIO0 | Radio UART RX | Successful ROM SYNC, identity and full flash reads |
| UART0 RX, GPIO1 | Radio UART TX | Valid ROM responses and matching full flash reads |
| Radio reset GPIO | Unknown | No pin driven or inferred |
| Radio download GPIO | Unknown | Physical far-button sequence works; RP GPIO mapping unknown |
| Hardware flow control | Unknown wiring; disabled in diagnostic | RTS/CTS are not used |
| Header sharing/voltage | NOT TESTED | No schematic, continuity test or meter measurement |

Successful bidirectional ROM commands prove the GPIO0/1 route on **A only**.
115200 baud, 8N1, no flow control worked for ROM identity and backup. This is not
a selected production rate or a ten-minute transport qualification.

The input-only SRAM observer enabled no GPIO output drivers. It observed GPIO1
low initially, high after holding the far button during a power cycle; GPIO0
was high in both cases. Passive 74880-baud captures did not recover a valid boot
message. Pressing the far button without a power cycle produced no conclusive
reset-pin mapping. Floating input transitions are not a wiring measurement.

## Recovery access observed

For RP2040, unplug the selected board, hold the button beside USB, reconnect
the same cable/port, then release. Expect RPI-RP2. A's erased application flash
also returns to ROM without holding BOOTSEL. B must not be assumed to do so.

For A's radio, unplug A, hold the button furthest from USB, reconnect A to its
mapped port, then release. Load the source-built SRAM bridge into RP2040 and
connect to its CDC interface. The bridge does not control reset/download GPIOs;
USB DTR/RTS have no radio GPIO effect. This sequence successfully exposed radio
ROM and was repeated after interrupted reads and after a running diagnostic.
The radio diagnostic was written, verified, booted and identified by a valid
CRC-protected application response. The full original 1 MiB image was then
restored with unchanged headers and exact full readback. Factory application
boot remains unverified: read-only `AT\r\n` probes received no response after
software boot at 115200, or after a user-confirmed cold boot at 115200/9600/74880.
Offline AT/SDK string markers do not prove a working AT application. No factory
boot response was captured before deployment, so silence cannot be attributed
to the diagnostic. Keep B preserved while A's remaining recovery gates are open.

The SRAM bridge/observer enumerate as SDK CDC 2e8a:000a, observed COM5 on A.
The SRAM serial descriptor is not unique. A 1200-baud CDC request returns the
bridge to RP2040 ROM; Windows reports the expected port-disappearance exception.
This software return has been exercised. A later persistent deployment of the
accepted FNR-003 mouse-only image passed picotool verification, independent
image-region readback and application enumeration. Its original blank RP flash
has not yet been restored after that deployment.

Windows initially lacked the RP2 Boot Interface 1 driver. The user installed
WinUSB for **2e8a:0003, MI 01 only** with Zadig 2.9. Mass-storage remained on
USBSTOR and the composite parent on its original driver. No USB-controller or
unrelated device driver was changed. Native commands did not run elevated.

## Verified tools and build provenance

| Tool | Version / identity |
|---|---|
| picotool | 2.2.0, official Windows x64 release; source `a7eb3988f0645239185fadb4e25d8279478c2dbb` |
| Windows picotool executable SHA-256 | `cba868e55e30bf717196d5b6a3bb5ab3d8ffea8d56afe182d5c4243dfa39d714` |
| Pico SDK | 2.2.0, `a1438dff1d38bd9c65dbd693f0e5db4b9ae91779` |
| TinyUSB | 0.18.0, SDK gitlink `86ad6e56c1700e85f1c5678607a762cfe3aa2f47` |
| Arm GNU | 13.3.Rel1, GCC 13.3.1, native Windows archive verified against publisher digest |
| esptool / pyserial | 4.8.1 / 3.5, private Python environment |
| CMake / Ninja | 3.31.6 / 1.11.1.4 |
| Arduino CLI | 1.5.1 portable Windows x64; archive SHA-256 `fabe42e0eb04d00e776a66178299ff95a46c623dbc260f997e58fd514853dd40` |
| ESP core/compiler | ESP8266 Arduino 3.1.2 / Xtensa GCC 10.3.0; official Windows package index |

The accepted Linux dependency lock remains unchanged. Native Windows tool
archives are supplemental host tools, with provenance in the evidence ledger.
No WSL or cloud environment was used to stand in for physical USB testing.

## Reproducible read-only backup commands

Use absolute paths for the private identities, destination and tools. Refuse an
existing output directory. Keep B unplugged during the A phase. Before each
operation, verify the single present device's full topology against its private
mapping. On RP2040, picotool 2.2.0 `--ser` selects the **64-bit flash unique ID**,
not the shared BOOTSEL USB serial. The helper performs this selection.

```text
python tools/hardware/rp2040_backup.py --board A --identities <private-identities.json>
  --picotool <verified-picotool.exe> --picotool-sha256 <documented-executable-sha256>
  --expected-size 2097152 --explicit-range --output <new-private-directory>
```

For nonblank B, `save -a -v` captured two complete 2 MiB reads. For blank A,
`save -a` cannot determine the image extent; the helper's explicit-range mode
reads `0x10000000..0x10200000`. It does not issue an erase or write.

Validate the SRAM UF2 before loading it:

```text
python tools/hardware/check_ram_uf2.py build/candidate-bridge/fnr_candidate_bridge.uf2
picotool load -v -x build/candidate-bridge/fnr_candidate_bridge.uf2 --ser <A-flash-unique-id>
```

The validator rejects flash-addressed/wrong-family/incomplete UF2 blocks.
Use a fresh inventory to guard the load's physical target; the validator alone
does not identify a connected board. Allow CDC enumeration to complete.

ROM identity was read with pinned esptool:

```text
python -m esptool --chip esp8266 --port <mapped-COM> --baud 115200
  --before no_reset --after no_reset --no-stub chip_id
python -m esptool --chip esp8266 --port <mapped-COM> --baud 115200
  --before no_reset --after no_reset --no-stub flash_id
```

Full reads use the official esptool RAM stub. The generic esptool Windows CLI
deasserts DTR, which leaves TinyUSB's CDC transmit FIFO overwritable. Three full
read attempts with DTR false failed (short blocks); those dumps are invalid.
The accepted helper opens serial with DTR true, checks board topology, radio
MAC/model and flash identity, then performs two full reads with stub MD5 checks:

```text
python tools/hardware/radio_backup.py --board A --identities <private-identities.json>
  --port <mapped-COM> --expected-mac <privately-recorded-A-MAC>
  --expected-flash-id 0x14325e --expected-size 1048576
  --bridge-sha256 1e19bc4de2b110df8023e77009e87440c62444e2d67ade33a20fccae209d3346
  --output <new-private-directory>
```

Both accepted reads match. Bridge error counters remained unchanged across
them: zero overruns/drops/parity/breaks, four framing errors accumulated during
an earlier failed attempt. This is not a claim of a pristine counter baseline
or accepted sustained transport. Raw failed-read logs are retained privately.

## Flash deployment and qualification observations

`radio_flash.py` requires two complete, unchanged matching backup files, exact
board/topology/MAC/model/flash identity and an explicit image SHA-256. Diagnostic
images must be ESP8266 format with a valid checksum, DOUT, 1 MiB and 40 MHz flash
header. Restoration requires the complete original backup hash. There is no
mass-erase command. esptool writes only the image's occupied sectors, keeps all
header fields, then the helper reads back and compares every image byte before
requesting a software boot. Separate observation must prove successful boot.

On A, the far-button power cycle returned the running diagnostic to ROM. The
SRAM bridge was reloaded and the full preserved radio image restored exactly.
The first deployment attempt failed before writing flash because esptool 4.8.1
tried to upload its RAM stub a second time. The corrected helper confirms the
resident stub with SYNC before passing it to esptool's write command. A pinned
fake-serial regression exercises that actual esptool object integration.

`rp2040_flash.py` requires the board's observed JEDEC geometry, full matching
backup pair, topology/flash UID, verified picotool and explicit image hash. It
rejects non-RP2040, SRAM, duplicate or out-of-capacity UF2 destinations. Restore
mode accepts only the complete original binary and writes it at `0x10000000`.
Both paths independently read back every selected image byte before rebooting.
This is a qualified image restoration, with no separate erase-all command.
The physical RP write path has run on A; its restoration path remains pending.

The radio and RP UART diagnostics passed a ten-second active smoke run on A at
115200 8N1. A generated 198 requests and received 198 matching echoes plus 240
independent radio streams, including the two-second drain. All CRC, pattern,
sequence, UART and software-buffer error counters were zero, and the radio's
final counters matched. This is smoke evidence, not the required 600-second /
10,000-frame qualification or a measured production bandwidth guarantee.

The isolated link diagnostics, build configuration and `RUN10`/`RUN600` commands
are described in `diagnostics/README.md`. Required UART interruption/reset/burst
cases, 600-second/10,000-frame tests on each board, RP2040 persistent-image
recovery, full USB lifecycle/HID behavior, RF pair tests, and full FNR-008
reference TiltMouse interoperability remain pending.

## Physical USB HID subset on A

The accepted FNR-003 source was built with the native Windows Arm toolchain and
deployed to A's RP flash. Windows enumerated `cafe:f003` with a HID-compliant
mouse child at A's mapped port, both with status OK. Windows HID parsed usage
page 1 / usage 2, a four-byte Windows input buffer (including its report-ID
prefix), one button-capability range and two value capabilities, with no output
or feature reports. This is an observed descriptor subset, not a raw USB device /
configuration / report descriptor capture. A GET_REPORT probe via
`HidD_GetInputReport` returned Windows error 1; no neutral report was observed.
The cause has not been isolated. The production image has no synthetic input
generator. Physical no-motion, button/axis, suspend/resume and fail-safe behavior
are not inferred from its source or from enumeration.

`hid_probe_windows.py` checks the USB parent topology, opens the single selected
HID collection with zero read/write access, and reads parsed capabilities plus
GET_REPORT. It sends no synthetic input or output report. Raw results remain
private, with allowlisted fields in `usb-enumeration.json`.

## Prior art and boundaries

[JiriBilek's clone library](https://github.com/JiriBilek/RP2040_PicoW_ESP8285_Library)
suggested UART0 at 115200;
[mocacinno's clone notes](https://github.com/mocacinno/rp2040_with_esp8285/tree/852aad1aa6d5156743421fc58b1fd15ae95b6c3d)
suggested the far-button download sequence. These were hypotheses, tested only
where explicitly recorded above. Their prebuilt UF2s were not loaded. All
diagnostics in this campaign were built from repository source. The observer's
read-only SSI implementation retains the Pico SDK license notice locally.

No ESP-NOW, production internal framing, provisioning, HID-host behavior,
electrical voltage/current, latency bound, or TiltMouse interoperability is
established by these backup observations.
