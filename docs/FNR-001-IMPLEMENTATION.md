# FNR-001 implementation evidence and boundaries

## Provenance

- Owner: issue #2; implementation branch `fnr-001-dual-firmware-ci`; PR #13.
- Assessed main: `8cc7a4c109a099466d8389a4e3b4004bfdf44c01`.
- Source recovered (CRC-verified entries) from
  `checkpoint/prepass-FNR-001-ad4f7d5eef81/checkpoints/prepass/FNR-001/ad4f7d5eef81/FNR-001-dependency-prepass.zip`.
- The ZIP checkpoint itself is preserved, not copied to production paths.
- Initial integration commit: `fa70c800fa3c1a6124f756408f0248a630a4a2ae`.

## Observed initial CI (before final manifest/doc refinements)

Both run IDs
[38099605766](https://github.com/techrote/faikeow-now-reciever/actions/runs/38099605766) and
[38099614591](https://github.com/techrote/faikeow-now-reciever/actions/runs/38099614591)
passed all four jobs: text sanity, native UBSan, inert RP2040 cross-build,
inert ESP8266 cross-build. Actual RP2040 UF2/ELF and ESP BIN/ELF
were produced as downloadable Actions artifacts with SHA256SUMS.

These runs are **baseline evidence**, not acceptance for later commits.
Exact final PR head and post-merge main require their own green checks.

## Deliberate non-claims

- No physical clone-board identification, pin mapping, flash/recovery test.
- No functioning ESP-NOW radio ingress or USB HID profile.
- No generic envelope, inter-MCU framed link or provisioning.
- No TiltMouse reference interoperability.
- Inert output images must not be described as working receiver firmware.
- Toolchain acquisition and compilation do not establish physical operation.

## Pending FNR-001 acceptance

- Final exact-head CI success after manifest/doc changes.
- Final artifact manifests and hashes verified.
- Reconcile PR, squash merge if accepted and confirm main CI.
- Update the programme tracker and close #2 only after verification.
