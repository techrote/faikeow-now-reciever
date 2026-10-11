# Receiver hardware integration

## Authoritative devboard reference

[techrote/picoWcloneESP8285N08](https://github.com/techrote/picoWcloneESP8285N08)
is the authoritative hardware reference for the characterized RP2040 +
ESP8285N08 clone Pico W devboards. General chip capabilities, verified wiring,
resource use, button/ROM behavior, flashing, recovery and programming guidance
are maintained there.

| Reference | Current documentation | Qualified baseline |
|---|---|---|
| Overview | [README](https://github.com/techrote/picoWcloneESP8285N08#readme) | [8d48cdd](https://github.com/techrote/picoWcloneESP8285N08/tree/8d48cdde71ccba824436635858265cb31c4ed88f) |
| Measured board contract | [Current](https://github.com/techrote/picoWcloneESP8285N08/blob/main/docs/BOARD-CONTRACT.md) | [Pinned](https://github.com/techrote/picoWcloneESP8285N08/blob/8d48cdde71ccba824436635858265cb31c4ed88f/docs/BOARD-CONTRACT.md) |
| Programming reference | [Current](https://github.com/techrote/picoWcloneESP8285N08/blob/main/docs/DEVBOARD-PROGRAMMING-REFERENCE.md) | [Pinned](https://github.com/techrote/picoWcloneESP8285N08/blob/8d48cdde71ccba824436635858265cb31c4ed88f/docs/DEVBOARD-PROGRAMMING-REFERENCE.md) |
| Operating runbook | [Current](https://github.com/techrote/picoWcloneESP8285N08/blob/main/docs/DEVBOARD-OPERATING-RUNBOOK.md) | [Pinned](https://github.com/techrote/picoWcloneESP8285N08/blob/8d48cdde71ccba824436635858265cb31c4ed88f/docs/DEVBOARD-OPERATING-RUNBOOK.md) |
| Tool source/checkout | [Upstream tools](https://github.com/techrote/picoWcloneESP8285N08/blob/main/docs/UPSTREAM-TOOLS.md) | [Pinned](https://github.com/techrote/picoWcloneESP8285N08/blob/8d48cdde71ccba824436635858265cb31c4ed88f/docs/UPSTREAM-TOOLS.md) |

The qualified documentation baseline is
`8d48cdde71ccba824436635858265cb31c4ed88f`. The original accepted receiver-side
hardware implementation/evidence baseline remains
`2777c90bfb13194feaeb6f7410c8029417af8da4` (PR #17, FNR-002).
Use current hardware documentation for ongoing development and record the exact
reference commit used when making reproducible firmware/acceptance claims.
A later documentation commit does not automatically qualify a new firmware
artifact, board revision or changed electrical assumption.

## Receiver integration baseline

Both measured units independently identify as RP2040 B2 with 2 MiB external
flash plus ESP8285N08 with 1 MiB embedded flash. ESP8285 is ESP8266-family Wi-Fi
hardware without Bluetooth; these are non-CYW43 clones.

The established onboard link is RP2040 UART0 GP0 TX -> radio RX and GP1 RX <-
radio TX, qualified at 115200 baud, 8N1 without flow control. Reserve GP0/GP1
while using the radio. No external wiring is needed for this link. RP-controlled
radio reset/download pins, complete header routing and electrical measurements
remain unknown; use the reference's tested physical recovery sequences.

FNR-002 passed independent complete backup/restoration, diagnostic cold boot,
600-second UART integrity and software fault/recovery cases on both boards.
The [FNR-004 follow-up](../evidence/FNR-004/README.md) established distance- and
requested-power-sensitive mismatch reception. SDK channel selection is not a
per-packet physical-channel admission guarantee; the exact RF mechanism remains
unmeasured. Preserve those failed negative tests when planning later acceptance.
Full USB/profile/integrated receiver acceptance is not implied by this baseline.

## Authority and maintenance boundaries

- **Dedicated devboard repository:** general hardware facts, resource constraints,
  programming/recovery procedures and later board characterization.
- **This receiver repository:** firmware sources, diagnostic/host helper sources,
  dependency lock/builds, production framing, receiver envelope/core, profiles,
  provisioning and application-specific acceptance.
- **Local historical records:** [BOARD-CONTRACT.md](BOARD-CONTRACT.md) and
  [evidence/FNR-002/](../evidence/FNR-002/README.md) preserve the original campaign.
  The original public evidence/checksums remain unchanged.

Add new general hardware facts/procedures to the dedicated repository, then
update the receiver's integration summary or pinned reference when warranted.
Keep receiver-specific implementation and acceptance evidence here. Reuse
accepted evidence for unchanged hardware/artifacts; repeat physical campaigns
only when the task's changes or an observed failure warrants it. Unknown nets
and unverified clone revisions remain unsupported until separately established.
