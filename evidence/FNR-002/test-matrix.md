# Physical acceptance matrix

Date: 2026-10-11. Host: native Windows 11 Pro. Evidence: adjacent JSON ledger.

| Case | Owner | A | B | Meaning / remaining gate |
|---|---|---|---|---|
| USB topology isolation | FNR-002 | PASS | PASS | User unplug/replug correlated exact private port paths |
| RP2040 ROM identity / physical BOOTSEL | FNR-002 | PASS | PASS | Both B2, RPI-RP2 |
| RP flash backup | FNR-002 | PASS | PASS | Two matching 2 MiB reads each; B capacity JEDEC still pending |
| RP flash JEDEC | FNR-002 | PASS | NOT TESTED | A 684015; exact vendor part unverified |
| Input-only SRAM observer | FNR-002 | PASS | NOT TESTED | A zero GPIO output-enable, captured JEDEC/UID |
| SRAM bridge load/verify/run and return ROM | FNR-002 | PASS | NOT TESTED | No persistent RP flash write |
| RP flash unchanged after SRAM diagnostics | FNR-002 | PASS | NOT TESTED | A repeated full reads match original |
| Radio ROM access / model / flash / MAC | FNR-002 | PASS | NOT TESTED | A ESP8285N08, 1 MiB, 26 MHz reported |
| Bidirectional UART route at 115200 | FNR-002 | PASS | NOT TESTED | A RP GP0 TX, GP1 RX; no B generalization |
| Radio full backup with DTR false | FNR-002 | FAIL | NOT TESTED | Three short-read failures; invalid images excluded |
| Radio full backup with DTR true | FNR-002 | PASS | NOT TESTED | Two matching reads; stub MD5 and SHA-256 checks |
| Radio reset/download RP GPIO mapping | FNR-002 | NOT TESTED | NOT TESTED | Physical far-button method only on A |
| Electrical levels/header conflicts | FNR-002 | NOT TESTED | NOT TESTED | No instruments/schematic measurements |
| Radio diagnostic write/readback/boot | FNR-002 | NOT TESTED | NOT TESTED | Image and conservative helper prepared |
| Full radio factory restoration/readback/boot | FNR-002 | NOT TESTED | NOT TESTED | Must prove A first |
| RP persistent UF2 write/restore | FNR-002 | NOT TESTED | NOT TESTED | SRAM recovery alone is insufficient |
| 600-second/10,000-frame deterministic link | FNR-002 | NOT TESTED | NOT TESTED | Both MCU diagnostics compile; not loaded |
| Burst/interruption/reset/soak recovery | FNR-002 | NOT TESTED | NOT TESTED | Separate failure and recovery ledger required |
| Cold USB reconnect to ROM | FNR-002 | PASS | PASS | Does not prove application cold-boot behavior |
| USB HID enumeration/descriptors/lifecycle | FNR-003 | NOT TESTED | NOT TESTED | Accepted source merged PR16; no physical HID load |
| ESP-NOW pair/admission/channel/integrity | FNR-004 | NOT TESTED | NOT TESTED | Accepted ingress software; no diagnostic pair loaded |
| Production framing/core/provisioning | FNR-005/006/007 | BLOCKED | BLOCKED | Required accepted implementations not established in this campaign |
| Integrated receiver and actual TiltMouse | FNR-008 | BLOCKED | BLOCKED | Integrated software/reference sender acceptance absent |
| Overall FNR-002 acceptance | FNR-002 | BLOCKED | BLOCKED | Reversible flash recovery and sustained dual-board evidence missing |
