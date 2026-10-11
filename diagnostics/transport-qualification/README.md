# FNR-005 production transport qualification fixture

Issue #6 owns this separately named injector/observer. It is not production
firmware or the FNR-002 SLIP/CRC32 fixture. Application bytes are opaque and no
HID output or FNR-006 envelope/profile semantics are implemented.

Use one representative characterized board (A for this campaign). The two
available units are identical hardware designs; Board B is not a mandatory
second qualification DUT. Use it only for a short optional smoke check or a
plausible unit-specific anomaly. Prior independent FNR-002 evidence is reused.

## Implementation boundaries

The ESP diagnostic includes the actual production radio and transport adapter
sources, and links the actual shared codec/TX/ingress modules. Its deterministic
producer invokes the production `rx` callback body from foreground; production
`fnr_radio_take_foreground` and `fnr_esp_transport_task` own subsequent transfer.
The normal loop retains the production 1 ms cooperative delay. The UART RX
direction is enabled solely for bounded diagnostic commands. Normal TX uses the
unmodified production FIFO calculation, serializer and bounded slots.

The RP SRAM diagnostic includes the actual production adapter and links the
shared parser/ring. Its observer uses the public event/take APIs. It additionally
reads finite internal ownership state for drained snapshots. It uses CDC rather
than the production HID descriptor and does not call the HID backend. No raw
UART bytes bypass the production receive path. USB output occurs only for
commands/end-of-phase summaries; no unbounded per-packet print queue exists.

Fault stimuli use the production codec to construct a frame, then make bounded
deliberate damage. The injector has a separate 600-byte raw-stimulus buffer and
bounded FIFO writer used only while normal production TX is idle. This writer
does not qualify clean TX ownership; normal traffic exercises that machinery.
Corruption always traverses the physical UART and actual production RP parser.
`I` pauses the real UART IRQ and can cause an actual hardware overrun. `F`
leaves IRQ running but delays foreground service; `C` delays only consumption.
No guessed GPIO, baud change or electrical reset/download operation exists.

## Commands and bounds

RP commands: `INFO`, `S` (snapshot), `R` (production parser reset), `H` (reset
diagnostic payload digest only), `E first-id`, `C ms` (consumer pause, <=10000),
`F ms` (foreground pause, <=10000), `I ms` (IRQ pause, <=1000), `B` (RP ROM),
and `X command` (bounded reverse diagnostic UART control).

ESP commands: `RUN count length rate first-id` (count <=30000, length 1..250,
rate 1..200); `STOP`/`SNAP` (freeze generation, drain, emit diagnostic telemetry);
`HASH` (diagnostic digest reset); `MUTE`/`RESUME` (pause/resume production service);
`RESTART` (actual radio lifecycle restart), `REBOOT` (ESP application reboot),
`ZERO` (resynchronization delimiter), `FAULT 0..13`.

Faults 0..13: repeated delimiters, terminated truncation, insertion, removal,
changed byte, CRC damage, malformed COBS, CRC-valid unsupported version,
CRC-valid unknown type, CRC-valid impossible length, exact encoded-capacity
segment, oversized segment, bounded garbage, and unterminated partial frame.
Each fault must be followed by an explicit boundary and clean recovery burst.

Telemetry is a distinct opaque diagnostic datagram, encoded/queued by the real
TX APIs and validated/taken by the actual RP consumer API. Its synthetic source
distinguishes it from pattern datagrams. It records generated/admitted/taken,
TX replacements/aborts/completions/bytes, phase timing, cumulative payload hash,
radio identity and exact source SHA. Production status frames remain present
at their normal one-second cadence during active traffic. Telemetry is never
counted as an injected test datagram; assessments account for its wire bytes.

`transport_capture.py` pins a manifest digest, exact source/lock/artifacts,
deployment/readback records, fresh topology, RP UID and live ESP MAC/source.
Plans contain only reviewed bounded commands/waits/markers. Raw JSONL, identity,
inventory, original backups and local manifest paths stay outside Git.
Successful capture is **NOT ASSESSED**, never an automatic PASS.
`transport_assessment.py` separately checks clean, TX overload and consumer-delay
accounting; ten synthetic regression tests cover false-PASS paths and bounds.

## Build and deployment

Linux CI builds both production lanes and both qualification lanes from the
exact head. `FNR_SOURCE_ID=<full commit> bash tools/build_transport_qualification.sh
rp2040|esp` uses the same dependency lock. The RP observer is SRAM-only and must
pass `check_ram_uf2.py`. Windows builds use the supplemental qualified native
toolchain; record its distinct binaries, compiler hashes and configuration.

Before hardware: commit sources, require clean tree, native/sanitizer/adapter
regressions, production and qualification builds, linked-path checks and exact
artifact hashes. Follow the dedicated hardware runbook and guarded helpers for
radio ROM entry, original-backup validation, exact write/readback and RP SRAM
load. Do not repurpose an old deployment record or infer a cold boot.

Use >=18002 datagrams at 30/s for the sustained phase: 18001 intervals of 33333
us span approximately 600.027333 seconds. Confirm actual target `active_us`
>=600000000; scheduling arithmetic alone is insufficient. The 115200 8N1 budget
is 11520 wire bytes/s, <=8378 bytes/s at 30 maximum frames/s plus one status/s.
Run this long campaign once on the representative DUT.

Every claim still needs physical evidence. Build success and synthetic
assessment tests do not establish traversal, overload, restart or recovery.
