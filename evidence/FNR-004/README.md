# FNR-004 channel-behavior follow-up — 11 October 2026

## Engineering disposition

The reproducible anomaly is a **distance- and requested-power-sensitive mismatch
reception behavior**, not evidence that the portable source filter or payload
checker failed. SDK channel selection is verified configuration; it does not
guarantee zero delivery from a transmitter reporting another channel. The
pinned receive callback has no per-packet channel metadata, so this adapter
cannot implement a physical-channel admission check from that ABI.

The investigation resolves the receiver contract and negative-test interpretation.
It does **not** identify the exact RF mechanism or claim a firmware cure. All
zero-reception failures remain FAIL. Production ingress is unchanged; source
admission remains exact MAC matching, which is not cryptographic authentication.
Future key/provisioning policy belongs to FNR-007. Lowering power or increasing
distance is not accepted as an isolation or security mechanism.

## Method and provenance

Two existing privately identified boards A/B, native Windows 11 Pro, onboard
UART0 at 115200 8N1 and unchanged labeled RP recovery bridges. Reused original
backups, identity/recovery and ten-minute UART evidence. Fresh USB label/topology,
live distinct radio MACs and exact diagnostic readback guards precede each run.
Initial B CDC error 31 blocked the first attempt; user reconnection recovered it.
No RF case ran during that failed connection attempt.

[provenance.json](provenance.json) records exact sources, image/tool/driver
digests, guarded deployment and software-boot observations. The distance matrix
uses the unchanged original diagnostic, SHA-256
`83e67685b82f4f2a5b90cb6356ff26377ab4ca67656893c4853ce976b2f1c3c3`.
Power testing uses source `5f9f5e7c312c746df4ed138ddf7a9514131432c9`, clean
at build, image SHA-256
`aa68778ae10238417c92739f506f09c93a796ee1e61ad0f2fd60e7d6d6bd084d`
(285264 bytes). Both radios passed guarded exact readback and live software boot.
This new image was not independently cold-boot qualified; unchanged recovery and
UART qualification are reused, not assigned to a new long soak.

The matrix driver is a private investigation script, with digest recorded here;
it preserved all channel-pair failures instead of stopping at the first one.
Its helper imports are identified by each run's clean host source commit.
Other captures use the committed guarded `tools/hardware/rf_pair_capture.py`.
Capture start/end commits are retained where the helper recorded them; the
private matrix recorded its start commit only. Neither on-device image changed
during the distance comparison.

Distances are user reported. SDK power values are requested ceilings in quarter
dBm, not calibrated RF measurements. No physical frequency, RSSI, spectrum or
receiver selectivity was measured. SDK status sampled at command boundaries is
not a continuous RF trace. Raw captures, full identifiers, host paths and original
dumps remain private; their digests are public.

## Results

[channel-comparisons.json](channel-comparisons.json) retains all 58 cases,
including counters, channels, configurations and negative failures. Every case
was reassessed using the current full queue/callback/integrity checks before
curation; whitelist fields exclude private identifiers.

The original focused B-to-A reproduction accepted 100/100 mismatch packets
(A channel 1, B channel 6), with 17 successful and 83 failed sender callbacks.
The preceding same-channel unexpected-source test rejected all 100 packets.

The two distance matrices used 300 packets of 250 bytes per case, configured
10 ms spacing, both directions and receiver/transmitter pairs:
1/1, 1/6, 6/6, 6/1, 11/11, 1/11, 11/1, 6/11, 11/6, 1/1.

| Comparison | Same-channel controls | Mismatch observation |
|---|---|---|
| 20 cm matrix | All eight checked 300/300, zero corruption/drops/gaps | Ten of twelve mismatch cases received packets; two received zero |
| 180 cm matrix | All eight checked 300/300, zero corruption/drops/gaps | B-to-A RX6/TX11 received 5/300 and RX11/TX6 received 6/300; other ten received zero |
| 180 cm focused RX1/TX6 | Both source-reject controls rejected 100/100 | Zero of 1,000 per direction; all 2,000 sender callbacks failed |

The entire 180 cm matrix is **FAIL**, despite the passing original-channel
focused repeat. Failed send callbacks do not erase received valid payloads.

Power testing then held the user-reported separation at 20 cm, same image,
ports and geometry, changing the requested SDK ceiling on both radios. A ceiling
of 82 represents 20.5 dBm; zero represents 0 dBm. These are API requests.

| Order / direction, RX1/TX6 | Requested ceiling | Mismatch accepted / sent | TX successful / failed callbacks | Zero-reception result |
|---|---|---|---|---|
| High 1, B-to-A | 82 | 36/300 | 3 / 297 | FAIL |
| Low, A-to-B | 0 | 0/1,000 | 0 / 1,000 | PASS |
| Low, B-to-A | 0 | 2/1,000 | 0 / 1,000 | FAIL |
| High 2, B-to-A | 82 | 12/300 | 0 / 300 | FAIL |

All four corresponding 500-packet, 250-byte same-channel burst controls checked
every packet without unexplained loss, corruption or duplicates. All four
100-packet unexpected-source controls received/rejected every packet and
accepted none. Final SDK channels and configured ceilings matched requests.
Both radios reported station mode, disconnected station status, auto-connect
and auto-reconnect disabled in these active-case samples. No infrastructure
association was observed in those samples; no deliberate infrastructure
configuration was introduced.

The high/low/high result supports power sensitivity at fixed geometry, and the
distance comparison supports distance sensitivity with the original image.
Finite RF selectivity/strong-signal effects are a plausible interpretation;
physical channel, mechanism and calibrated power remain unknown. These small,
sequential bench samples do not establish a probability, range specification or
universal threshold. Residual low-power and 180 cm receptions are preserved.

## Receiver acceptance and final state

FNR-004 selects and verifies an SDK channel, enforces exact-source/length
admission, and exposes bounded newest-datagram handoff. It promises neither
cryptographic sender identity nor per-packet physical-channel exclusion.
Native, RP build, ESP build and physical RF evidence remain distinct. The
production framing/receiver/HID/reference-sender acceptance gates remain in
later programme issues. No TiltMouse contract was used for this opaque payload
investigation.

[final-state.json](final-state.json) records both connected boards with unchanged
persistent RP bridges and the new RF diagnostic. A guarded final OFF check
confirmed both radio identities, Wi-Fi mode OFF, no RX/TX activity or outstanding
packet, and closed serial handles. Last configured ceiling is 82 on both.
No further board handling is required for documentation or CI work.

The immutable [FNR-002 snapshot](../FNR-002/README.md) and its failures are unchanged.
[SHA256SUMS](SHA256SUMS) covers this separate follow-up's public evidence.
