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
| Radio reset/download controls | FNR-002 | PASS | PASS | Physical button ROM entry and diagnostic app reset verified; no RP-controlled reset/download GPIO claimed |
| Optional RP reset-control wiring / header continuity | FNR-002 | NOT TESTED | NOT TESTED | Unknown pins remain unsupported; no user-built wiring or meter check required |
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
| ESP-NOW positive integrity / admission / restart | FNR-004 | PASS | PASS | Both directions 1/32/128/250 bytes, burst, peer rejection, adapter restart and 6,000-packet sustained runs |
| ESP-NOW deliberate backpressure | FNR-004 | PASS | PASS | Accepted = checked + drops; gaps = intentional drops; zero corruption |
| ESP-NOW software-channel mismatch isolation | FNR-004 | FAIL | PASS | A received 6/100 then 78/1,000 while sampled SDK channels were A=1/B=6; unresolved |
| Production framing/core/provisioning | FNR-005/006/007 | BLOCKED | BLOCKED | Required accepted implementations not established in this campaign |
| Integrated receiver and actual TiltMouse | FNR-008 | BLOCKED | BLOCKED | Integrated software/reference sender acceptance absent |
| FNR-002 physical hardware scope | FNR-002 | PASS | PASS | Independent identity, transport pins, physical recovery, reversible deployment, cold boot, sustained link and reset recovery |
| Programme acceptance | FNR-002 | See issue #3 | See issue #3 | Requires exact-head CI, review, merge and post-merge verification; RF/USB owners remain separate |
