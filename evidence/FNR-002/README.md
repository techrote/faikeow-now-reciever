# FNR-002 partial physical evidence — 2026-10-11

**OPEN / NOT ACCEPTED.** This ledger records measured subsets, not completion of
dual-board recovery, link reliability, USB HID or ESP-NOW acceptance.

Native Windows 11 Pro physical USB/serial access was available. Board A was
isolated at its mapped port; Board B was backed up and unplugged as the reference.
A's radio diagnostic and full original restoration passed exact readback. Its
pre-existing application boot remains unverified after silent AT probes. A's accepted
RP2040 mouse-only image has been written/read back, enumerated and cold-booted;
its full original blank RP image has now been restored and verified. A's approved
radio diagnostic cold-booted and passed 600-second UART integrity and six
software fault/recovery cases. B independently passed full RP and radio original
restoration/readback, accepted HID cold boot, approved radio diagnostic cold boot,
600-second UART integrity and six software fault/recovery cases.
Full original dumps,
complete host paths, flash IDs, MAC and raw logs remain in the private local
`hardware-private/FNR-002/2026-10-11` directory outside Git.

- `board-inventory.json`: independent board identities and backup facts.
- `usb-enumeration.json`: ROM/CDC identities and interface-specific driver change.
- `radio-identification.json`: independent A/B ROM/silicon/flash identities and backups.
- `interconnect-verification.json`: each board's verified GPIO0/1 UART route.
- `flash-recovery.json`: observed ROM/SRAM/deployment/restoration subsets and gaps.
- `link-integrity.json`: failed reads, accepted reads, independent A/B smoke/soak and fault cases.
- `diagnostic-builds.json`: local build images, source content and hashes.
- `raw-log-index.json`: private relative log names and immutable snapshot hashes.
- `test-matrix.md`: acceptance state by case and owning issue.

Each physical result identifies its board, host/date, command/tool configuration,
source/image where applicable and private raw log reference. Unknowns are null
or explicitly NOT TESTED. MAC/unique flash IDs are hashed in public evidence;
the private mapping is required for actual operations. Backup image SHA-256s
are published without their contents.

The current physical phase is additional USB observation and RF pair preparation.
A is unplugged, with original blank RP flash and a cold-boot-proven UART radio
diagnostic. B has a labeled recovery bridge in RP flash and the UART radio
diagnostic; temporary RAM applications do not change its preserved flash image.
See the board contract and diagnostics README for commands and measured scope.

FNR-003/004 software was reconciled through main
`d1def14ea85dcf66220b40249b8e66bd52667e4a`. A physical USB subset passed;
full USB behavior and RF physical tests are pending.
FNR-005/006/007 software and FNR-008's integrated receiver/reference TiltMouse
gates must be assessed independently before their later physical cases run.

CI results belong to the exact draft PR head and are recorded in the PR. Local
cross-builds and safety guard tests do not satisfy hardware acceptance.
