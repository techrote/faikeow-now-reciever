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

## Reproducibility evidence (post-manifest integration)

Both independent clean runs on source head
`f98dc23853e658d8e384dfd7f649a5050dd9c04b`
passed all four CI gates:

- [Push run 38099780518](https://github.com/techrote/faikeow-now-reciever/actions/runs/38099780518)
- [PR run 38099783197](https://github.com/techrote/faikeow-now-reciever/actions/runs/38099783197)

The two runs produced identical SHA-256 values for each pair of output images:

| Compile surrogate | Artifact | SHA-256 |
|---|---|---|
| RP2040 | `fnr_rp2040_inert.uf2` | `fcd83cdc2d1a7b20c89109fdbffcb5b7626dd944e6c2a1b48625d530113a2203` |
| RP2040 | `fnr_rp2040_inert.elf` | `2f0c22c53bf02418d54d958a0aa56fab47a7f90fcbe1f87f7f9992e675ca3278` |
| ESP8266 | `fnr_esp8266_inert.bin` | `f298fa1eb0f1fd7f3b5c03a3595782d13d7e343d165d29a329ca29019a388f8b` |
| ESP8266 | `fnr_esp8266_inert.elf` | `8f19de1f41601fe280acd442cffe8346ec7d65f3dd4d36d3bca36c1ad94692b6` |

These hashes apply only to the stated prior source head. The source ID is
embedded in ESP firmware metadata, so later commits are not expected to keep
the same output hash. Check the **exact final head** in Actions before merging.

## Acceptance and release record

The issue #2 acceptance comment and final PR/main workflow runs are the
authoritative acceptance record. This document records implementation
provenance and build-only evidence, not physical hardware acceptance.

FNR-002 remains responsible for board characterization. FNR-003/FNR-004
remain responsible for production USB and ESP-NOW functionality.
