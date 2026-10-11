# Physical acceptance matrix

Date: 2026-10-11. Host: native Windows 11 Pro. Evidence: adjacent JSON ledger.

| Case | Owner | A | B | Meaning / remaining gate |
|---|---|---|---|---|
| USB topology isolation | FNR-002 | PASS | PASS | User unplug/replug correlated exact private port paths |
| RP2040 ROM identity / physical BOOTSEL | FNR-002 | PASS | PASS | Both B2, RPI-RP2 |
| RP flash backup | FNR-002 | PASS | PASS | Two matching 2 MiB reads each |
| RP flash JEDEC | FNR-002 | PASS | PASS | Each independently 684015 / 2 MiB; exact vendor part unverified |
| Input-only SRAM observer | FNR-002 | PASS | PASS | Zero GPIO output-enable, captured independent JEDEC/UID |
| SRAM bridge load/verify/run and return ROM | FNR-002 | PASS | PASS | SRAM loads preserve flash; later HID deployment is a separate case |
| RP flash unchanged after SRAM diagnostics | FNR-002 | PASS | NOT TESTED | A repeated full reads match original |
| Radio ROM access / model / flash / MAC | FNR-002 | PASS | PASS | Each ESP8285N08, 1 MiB, 26 MHz reported |
| Bidirectional UART route at 115200 | FNR-002 | PASS | PASS | Independently RP GP0 TX, GP1 RX on each |
| Radio full backup with DTR false | FNR-002 | FAIL | NOT TESTED | Three short-read failures; invalid images excluded |
| Radio full backup with DTR true | FNR-002 | PASS | PASS | Two matching reads; stub MD5 and SHA-256 checks |
| Radio reset/download RP GPIO mapping | FNR-002 | NOT TESTED | NOT TESTED | Physical far-button method observed independently on A/B; RP control wiring unknown |
| Electrical levels/header conflicts | FNR-002 | NOT TESTED | NOT TESTED | Multimeter available; no measurements yet, schematic unavailable |
| Radio diagnostic write/readback/software boot | FNR-002 | PASS | PASS | A CRC-protected identity response and smoke |
| Full radio original restoration/readback | FNR-002 | PASS | PASS | Each exact complete original 1 MiB; unchanged headers |
| Pre-existing radio application behavior | FNR-002 | NOT TESTED | NOT TESTED | Both silent AT probes after cold boot; intended application protocol unknown; not a project acceptance gate |
| RP persistent UF2 write/readback/application boot | FNR-002 | PASS | PASS | A accepted mouse-only image, Windows enumeration |
| RP original-image restoration | FNR-002 | PASS | PASS | Each full 2 MiB exact original readback; A blank ROM, B original no USB |
| Ten-second deterministic link smoke | FNR-002 | PASS | PASS | Each 198 requests/echoes; A 240 and B 239 streams including drain; zero errors |
| 600-second/10,000-frame deterministic link | FNR-002 | PASS | PASS | Each independently 11,998 requests/echoes, 12,039 streams including drain; zero errors |
| Burst/interruption/software MCU reset recovery | FNR-002 | PASS | PASS | Six cases each; intentional damage/reset losses kept separate |
| Approved radio diagnostic cold boot | FNR-002 | PASS | PASS | Each CRC-protected identity after user-confirmed power cycle |
| Persistent HID application cold USB reconnect | FNR-003 | PASS | PASS | Each returned with matching parsed capabilities |
| Cold USB reconnect to ROM | FNR-002 | PASS | PASS | Does not prove application cold-boot behavior |
| USB HID mouse enumeration / parsed caps subset | FNR-003 | PASS | PASS | A Windows mouse usage, two value caps, one button-cap range |
| HID neutral GET_REPORT observation | FNR-003 | BLOCKED | BLOCKED | Windows error 1; no report observed, cause not isolated |
| HID raw device/configuration descriptors | FNR-003 | NOT TESTED | PASS | B mapped hub capture; HID descriptor included; raw report descriptor blocked by hub API |
| HID unsolicited motion, 20-second idle | FNR-003 | NOT TESTED | PASS | B exact target Raw Input observation; zero events/removals/errors |
| HID active controls / suspend-resume / neutral behavior | FNR-003 | NOT TESTED | NOT TESTED | No deterministic production input; neutral GET_REPORT separately blocked |
| ESP-NOW pair/admission/channel/integrity | FNR-004 | NOT TESTED | NOT TESTED | Ingress software merged PR15; #5 remains open; no diagnostic pair loaded |
| Production framing/core/provisioning | FNR-005/006/007 | BLOCKED | BLOCKED | Required accepted implementations not established in this campaign |
| Integrated receiver and actual TiltMouse | FNR-008 | BLOCKED | BLOCKED | Integrated software/reference sender acceptance absent |
| Overall FNR-002 acceptance | FNR-002 | BLOCKED | BLOCKED | Core UART/recovery proofs complete; additional USB/RF campaign in progress; meter checks optional |
