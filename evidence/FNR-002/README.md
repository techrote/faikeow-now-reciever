# FNR-002 partial physical evidence — 2026-10-11

**OPEN / NOT ACCEPTED.** This ledger records measured subsets, not completion of
dual-board recovery, link reliability, USB HID or ESP-NOW acceptance.

Native Windows 11 Pro physical USB/serial access was available. Board A was
isolated at its mapped port; Board B was backed up and unplugged as the reference.
No persistent MCU flash write has occurred in this campaign. Full factory dumps,
complete host paths, flash IDs, MAC and raw logs remain in the private local
`hardware-private/FNR-002/2026-10-11` directory outside Git.

- `board-inventory.json`: independent board identities and backup facts.
- `usb-enumeration.json`: ROM/CDC identities and interface-specific driver change.
- `radio-identification.json`: A's ROM/silicon/flash identity and matching backups.
- `interconnect-verification.json`: A's GPIO0/1 UART route; B remains unknown.
- `flash-recovery.json`: observed ROM/SRAM recovery versus untested flash restore.
- `link-integrity.json`: failed reads, accepted-read condition and untested soak.
- `diagnostic-builds.json`: local build images, source content and hashes.
- `raw-log-index.json`: private relative log names and immutable snapshot hashes.
- `test-matrix.md`: acceptance state by case and owning issue.

Each physical result identifies its board, host/date, command/tool configuration,
source/image where applicable and private raw log reference. Unknowns are null
or explicitly NOT TESTED. MAC/unique flash IDs are hashed in public evidence;
the private mapping is required for actual operations. Backup image SHA-256s
are published without their contents.

The next physical phase is A-only: validate diagnostic image and restoration
helper, load the temporary radio diagnostic, observe boot/smoke behavior, return
the radio to ROM with the far-button power cycle, restore the complete original
image and verify full readback/boot. Only after that proof should B undergo
independent radio discovery and diagnostic deployment. See the board contract
and diagnostics README for commands. No production pins should be inferred for B.

FNR-003/004 software was reconciled through main
`d1def14ea85dcf66220b40249b8e66bd52667e4a`. Their physical tests are pending.
FNR-005/006/007 software and FNR-008's integrated receiver/reference TiltMouse
gates must be assessed independently before their later physical cases run.

CI results belong to the exact draft PR head and are recorded in the PR. Local
cross-builds and safety guard tests do not satisfy hardware acceptance.
