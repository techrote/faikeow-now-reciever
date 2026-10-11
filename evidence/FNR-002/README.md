# FNR-002 partial physical evidence — 2026-10-11

**FNR-002 physical hardware scope PASS on both boards.** Programme acceptance
is recorded in issue #3 after PR/main verification. USB and RF entries are
separate measured subsets; the reproducible RF channel-isolation failure remains
open under FNR-004.

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
- `rf-preparation.json`: per-board diagnostic deployment and live identity.
- `rf-integrity.json`: original 18-case run, including its retained failure.
- `rf-completion.json`: two unreached reverse cases completed separately.
- `rf-channel-anomaly.json`: focused reproduction with live SDK-channel samples.
- `raw-log-index.json`: private relative log names and immutable snapshot hashes.
- `test-matrix.md`: acceptance state by case and owning issue.

Each physical result identifies its board, host/date, command/tool configuration,
source/image where applicable and private raw log reference. Unknowns are null
or explicitly NOT TESTED. MAC/unique flash IDs are hashed in public evidence;
the private mapping is required for actual operations. Backup image SHA-256s
are published without their contents.

The completed fixtures have labeled flash-resident RP recovery bridges and the
RF diagnostic on both boards, with A mapped to root port 3 and B to port 1.
Both original RP/radio backup pairs remain preserved privately. The boards
are development fixtures; production framing/core/provisioning and integrated
wireless USB HID remain later work. No additional meter work or repeated
pre-existing-application/recovery tests are required to proceed.

FNR-003/004 software was reconciled through main
`d1def14ea85dcf66220b40249b8e66bd52667e4a`. A physical USB subset passed;
full USB behavior remains unverified. The RF campaign passed 19/20 cases;
reverse software-channel isolation failed and reproduced. See the dedicated RF
records; this is not an overall FNR-004 physical acceptance claim.
FNR-005/006/007 software and FNR-008's integrated receiver/reference TiltMouse
gates must be assessed independently before their later physical cases run.

CI results belong to the exact draft PR head and are recorded in the PR. Local
cross-builds and safety guard tests do not satisfy hardware acceptance.
