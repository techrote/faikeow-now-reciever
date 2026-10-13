# FNR-008 joint physical acceptance

Owning receiver issue: #9. Reference sender issue: TiltMouse #8.
Session: `joint-TM008-FNR008-2026-10-13` (Europe/London).
Status: **IN PROGRESS; NOT ACCEPTED**. Neither issue is closed by this record.

## Reconciled authority

Receiver default/main `a49d95e61dc910c7db139db0280f80a31d10bfc8` and
TiltMouse main `2be851985fefb55bc0df3063b66896c4ec900092` were reconciled
against both owning issues, all comments, prerequisite PRs and current CI.
No open competing campaign PR existed at intake. FNR-005 #6, FNR-006 #7,
and FNR-007 #8 are accepted; receiver main CI 38322804818 passes all four gates.
TiltMouse TM-005A/B, TM-006/007 are merged; its main CI 38442701195 passes
host tests, wired build, authenticated public-fixture wireless build and style.

Board hardware authority is unchanged:
`techrote/picoWcloneESP8285N08@e8a80ae21e6432d30f6ec6146ff6754605a5d469`.
Board A only is the representative receiver DUT. Operator confirmed that A
remains on its previous USB port; fresh USB topology matches the private map.
Board B is not a receiver DUT in this campaign. Accepted identity, UART0
GP0/GP1 at 115200 8N1, original backup pairs and no-solder recovery evidence
are reused. No guessed radio control pins are driven.

## Contract reviewed before hardware access

- FNR-005 internal v1: COBS/CRC16, 16-byte LE header, distinct transport
  sequence/generation, bounded storage, events before data, 3000 ms watchdog.
- FNR-006 platform v1: 18-byte LE header, PROFILE_DATA 1, flags 0, nonzero
  u64 session, modulo-u32 sequence, half-range rejected, four retired sessions.
- relative_mouse ID 1 / schema 1: exact four bytes (schema, full buttons, X/Y),
  -127..127 axes. Movement at most once; gaps never trigger retransmission.
- FNR-007 FNA1/HMAC wrapper v1: 22-byte overhead, 228 enclosed platform bytes,
  210 profile bytes; a mouse packet is 44 RF bytes. MACs and public config
  ID/generation bind authentication. RP remains key-free.
- Default 250 ms application freshness is distinct from internal watchdog.
  Transport/ack transitions retain platform ordering while neutralizing output.
  USB lifecycle drops pending movement and requires the neutral barrier.
- Encrypted peer registration and wrapper authentication are required. SDK
  callbacks cannot attest individual packet encryption or channel. Channel
  selection is not isolation. Recorded authenticated packets are not protected
  by persistent anti-replay state across a complete receiver reboot.

## Starting receiver observations

Host: native Windows 11 Pro 64-bit, version 10.0.26300, 13 October 2026.
A starts with the FNR-007 config-2 production pair. Prior deployment records:
RP UF2 `b9b6f1d6152227e681734f3eb2e777dea29015eade1c85915a1386fd9f5d770a`;
ESP BIN `e1071cfcb04167b5b52287eaadf1305089b9bd4aeee30d7e04a2f32c1983b187`.
These are historical deployment identities, not a fresh flash readback. Source
`9d813d7143124a8dcadc3b2a339db00ac792392b` has the same firmware/build/lock
inputs as accepted current main. Config 2 authorizes the earlier synthetic
sender, so it is not a TiltMouse pairing.

Fresh physical observations in this session:

| Case | Result | Evidence limit |
| --- | --- | --- |
| USB enumeration | PASS: cafe:f003 mouse and USB input child healthy at A's mapped topology | Current identity; does not prove current binary bytes |
| Raw device/configuration descriptors | PASS: one boot mouse HID interface, IN endpoint 0x81, 8-byte endpoint, 1 ms polling | Report descriptor not returned by hub API |
| Parsed HID capabilities | PASS: usage page 1 / usage 2, two buttons, two axes, 4-byte Windows input buffer including report-ID prefix; no output/feature reports | Parsed subset |
| 20-second target-only idle | PASS: zero motion, no removal/error | No active-button, timeout, or explicit neutral-report claim |
| GET_REPORT | Windows error 1; no report obtained | Retained limitation, not a neutral-state pass |

The same session established normal TiltMouse USB enumeration and bounded idle,
then observed a zero-motion tilt trial. Sender-side diagnosis reproduced a
false freshness failure in the actual target loop. TiltMouse source
`5f07be236680d6232081923615c0433d643c4fdf` corrects that clock boundary; its
diagnostic USB capture established ready inputs, quiet idle and movement on
both host axes. Operator feedback requires Y/response tuning and a repeat
trial. This establishes no receiver radio, transport or profile result.
TiltMouse diagnosis is tracked in
[its draft PR #25](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/pull/25)
and its own evidence directory. Receiver acceptance is not inferred from a
sender's MAC callbacks or a working USB descriptor.

## Paired candidate preparation

A fresh private pair was generated for the live TiltMouse, using public config
ID `0x28df6312548b3111`, generation 3, channel 1, platform/profile/schema 1,
250 ms freshness and a fresh 16-byte deployment key. The input JSON is outside
Git; the sender header and generated receiver sources are ignored. The sender
adapter must independently match its live STA MAC before transmission.

Receiver production RP, admission-observer RP, production ESP and redacted
diagnostic ESP were built from clean receiver main using the supplemental
Windows lane: Pico SDK 2.2.0, SDK TinyUSB 0.18.0, Arm GNU 13.3.Rel1,
ESP8266 Arduino 3.1.2 / NONOSDK22x_190703, Xtensa GCC 10.3.0, Arduino CLI 1.5.1.
Each RP UF2/ELF/BIN was checked for absence of the deployment key. Private
manifests retain complete build arguments, artifact hashes and generated public
record identity. Candidate build success is not deployment or acceptance.

## Shared campaign and acceptance split

The [shared matrix](matrix.csv) records one result per physical case. Tests are
not duplicated for the two issues. TM-owned sensor/touch/direct-USB conclusions
belong to TiltMouse; radio/transport/core/profile/RP USB conclusions belong here.
Both issues require observed interoperability with exact paired binaries.

Keep all untouched cases NOT TESTED. Stop and diagnose unexplained power,
thermal, reset/reconnect, recovery or protocol failures. Preserve failed runs;
do not alter workloads/thresholds to produce passes. Do not infer zero loss,
precise latency, all-buttons-up, or at-most-once host output from compilation.
Raw inventories, original flash, full private identities and keys remain
outside Git. Concise derived captures and stable hashes will identify completed
cases. No physical RF acceptance or final v0.1 defaults are claimed yet.
