# FNR-007 physical provisioning qualification

Qualification is in progress. **The reviewed implementation is not accepted.**
PR [#22](https://github.com/techrote/faikeow-now-reciever/pull/22) remains draft;
issue [#8](https://github.com/techrote/faikeow-now-reciever/issues/8) remains open.

## Starting state and representative fixture

- Reconciled main: `136ef3b612f83fd9cad74a5dd50c2e5f629b44de`.
- Reviewed candidate: `73998749cc0a334661db20547ec76925128c6fa9`.
- Exact-head push `38294375216` and PR `38294382856`: 4/4 successful checks.
- Board A is the sole representative receiver DUT: characterized RP2040 B2,
  2 MiB external flash, ESP8285N08 with 1 MiB embedded flash.
- Receiver station MAC: `ec:94:cb:b3:c4:65` (redacted provisioning diagnostic).
- Board B supplies bounded synthetic sender traffic only, with operator
  authorization. Its station MAC is `ec:94:cb:b3:c4:91`.
- Operator-reported separation: approximately 180 cm. This is a bench smoke,
  not a range/performance qualification or RF isolation claim.

## Initial public provisioning identity

Schema 1; config ID `0x6529df3c591471ca`; generation 1; sender peer
`ec:94:cb:b3:c4:91`; requested receiver channel 1; platform 1;
`relative_mouse` profile ID/schema 1; freshness 250 ms; key present true.
Public 40-byte record SHA-256:
`6020de6b184ade32f047ecf2eeabfb3dc50f0687fe1fa91ce79d16e32e65ae22`.
The fresh per-device key and provisioning JSON remain outside the repository.

## Initial artifacts (reviewed source)

These are supplemental native Windows builds of the exact reviewed source,
using the pinned SDK/core and installed Windows toolchains. They are distinct
from the Linux CI artifacts; the Linux build scripts were not run on this host.

| Artifact | SHA-256 |
| --- | --- |
| Provisioned RP2040 UF2 | `532012e9e9e566666630660aca859c2be5bbc501253f9e1ed9d08ad07a839f44` |
| Provisioned ESP image | `4d8f73fdcd93e1fa20849b61a5e12e968c90e3630e47078e6043d2c0d9d77c78` |
| Redacted ESP provisioning diagnostic | `3b47d95cda9711218cfcf491b4b0cbd14bee5ae69979bd7c83bbc1efdbfc2cee` |

RP UF2/ELF/BIN were checked for absence of the exact radio-key byte sequence.
Both flashed production images passed guarded exact readback. The diagnostic
reported record valid, key present, encryption enabled, radio READY, and the
expected public metadata. A subsequent operator-confirmed cold boot ran the
production pair. Production USB is mouse HID `cafe:f003` with no CDC channel.

## Retained security failure — 2026-10-12

Each bounded transmission used 20 structurally valid relative-mouse packets,
50 ms spacing, zero buttons, X +1, Y -1. Host observation used target-filtered
Windows Raw Input for five seconds; sender UART recorded 20/20 enqueue and
20/20 completion callbacks. A sender completion is not receiver admission.

| Production receiver case | Moving HID events | X/Y totals | Assessment |
| --- | ---: | --- | --- |
| Idle | 0 | 0 / 0 | Safe inactivity |
| Correct encrypted peer/profile | 20 | +20 / -20 | Positive smoke |
| Same peer, mismatched sender key | 0 | 0 / 0 | Safe inactivity |
| Same peer, sender encryption disabled | 20 | +20 / -20 | **FAIL** |
| Unauthorized MAC `ee:94:cb:b3:c4:11`, encrypted | 0 | 0 / 0 | Safe inactivity |
| Plaintext after operator cold-restarted sender B | 19 | +19 / -19 | **FAIL reproduced** |

The cold-restart case eliminates a previous encrypted sender peer setup as a
necessary condition. Missing movement in that repeat is not an acceptance
claim; any movement from unauthenticated traffic is a failure. No button events
were observed in these cases. Raw captures, timestamps and full target discovery
are retained privately; no key or unrelated host inventory is public.

Encrypted SDK peer registration alone did not enforce authenticated input.
The pinned receive callback provides source MAC and bytes, with no trustworthy
per-packet encryption indicator. The pinned Arduino documentation also
[describes this limitation](https://github.com/esp8266/Arduino/blob/3.1.2/libraries/ESP8266WiFiMesh/README.md).

The operator explicitly authorized a versioned authenticated radio wrapper
within FNR-007 after this failure, including its sender-contract/payload-budget
change. A deterministic production-adapter regression is added before the fix.
The original failure remains part of acceptance evidence. No merge is authorized
by the initial CI success or by the positive movement smoke.

Regression-first commit `ad10a9a7d22092ba5938eb5cbef25eafd31082ed` reproduced
the defect in push CI `38309757230`: the actual production radio-adapter test
failed `!fnr_radio_take_foreground(&d)` after a bare packet from the configured
MAC. The other 14 native tests passed; this expected failure precedes the repair.

## Repaired production smoke — 2026-10-12

Artifact source: `7b08dd1dcbec5f2484af846bcaee3acd4f11422b` (clean source).
Push `38310778521` and PR `38310782531` both passed all four CI checks.
The public config is unchanged from the initial identity above. The RP artifact
is byte-for-byte unchanged. Rebuilt repaired ESP image SHA-256:
`67f242d7a8909f93b58a696598d5b0e344c28b9c67b83cfc0f8a66244ddaa80a`.
Repaired provisioning diagnostic SHA-256:
`7f44fb804b9707a646ca60c2f4b6ef4e8d8519da6f850bf7c70d4055390b9ce5`.

The repaired diagnostic ran on A and reported expected public metadata, valid
record, key present, encryption enabled, authentication enabled and READY. The
production ESP/RP pair was then flashed with exact readback and cold-booted by
the operator. One ROM synchronisation attempt failed before any write; repeating
the documented physical recovery sequence succeeded. No reset GPIO was guessed.

[Curated production results](production-smoke.json) retain source/tree/lock,
artifact hashes, timestamps, durations, requested channel and sender counters
for all 15 cases. Each active case attempted/queued 20 packets at 50 ms spacing;
each target-filtered Windows Raw Input capture lasted approximately five seconds.

| Repaired production case | Moving HID events | Assessment |
| --- | ---: | --- |
| Idle after cold boot | 0 | Safe inactivity |
| Correct encrypted/authenticated peer/profile | 20 | Positive smoke |
| Wrong sender MAC, otherwise correct encrypted wrapper | 0 | Safe inactivity |
| Unsupported packet profile 99 | 0 | Safe inactivity |
| Mismatched sender key | 0 | Safe inactivity |
| Bare plaintext packet, configured MAC | 0 | Original defect blocked |
| Encrypted but unwrapped packet | 0 | Authentication remains mandatory |
| Plaintext with incorrect authentication key | 0 | Keyless spoof blocked |
| Altered authentication tag | 0 | Safe inactivity |
| Wrong authenticated configuration generation | 0 | Safe inactivity |
| Sender requested channel 6, receiver configured channel 1 | 0 | Bounded measurement; reception counters pending |
| Same channel mismatch plus wrong sender MAC | 0 | Safe inactivity |
| Valid wrapper with sender link encryption deliberately disabled | 20 | SDK encryption-attestation limitation measured |
| Replay of earlier sequences in the same session | 0 | No replayed movement |
| Correct encrypted/authenticated restore without receiver reflash | 20 | Positive recovery smoke |

All movement totals matched the moving-event counts (+X / -Y); no button events
were observed. The mismatched-channel authorized case had 20 sender completion
failures and no HID movement. This does not prove absence of physical reception;
the observer's radio counters must establish that separately. It does not
contradict the retained FNR-004 mismatch-reception measurements.

The valid-wrapper/plaintext case is deliberately **a measurement**, not a claim
of encrypted-only callback delivery. It proves that a key-holding sender can
authenticate traffic while disabling its own link encryption. This SDK limit is
explicit in [the wrapper security contract](../../docs/FNR-007-RADIO-AUTH.md).
Bare or invalidly authenticated traffic produced no HID output. Production
still requires encrypted peer registration and has no unwrapped fallback.

These are exact production RF-to-USB observations. RP admission/counter state
is not exposed by the production HID-only interface. The separate observer
phase remains necessary for boot staging, matching READY, downstream rejection,
revocation, neutral barriers and recovery-order preservation.

## Remaining acceptance

The [initial observer results](observer-initial.json), from the same repaired
source, retain two further physical failures before repair:

- A CRC-correct provisioning record selecting profile ID 1/schema 2 was staged
  and acknowledged despite the target only supporting schema 1. No RF traffic
  was sent in this test. Boot admission was disabled until READY, but the
  unsupported schema subsequently enabled both gates: **FAIL**.
- A bounded 5000 ms UART RX IRQ interruption revoked acknowledgement, disabled
  both gates and queued/submitted neutral HID reports. Recovery exceeded the
  first 6.5-second observation window; a later snapshot confirmed automatic
  matching READY recovery. Both transitions cleared sender/session ordering.
  Replaying the already-applied sequence range 1100..1119 then produced 20/20
  movements: **FAIL**. Deterministic actual-adapter regressions precede repair.

The observer also distinguished 20 RP wrong-profile rejections from 20 ESP
wrong-source rejections (zero downstream RP packets in the latter case).
Bare/tag-invalid traffic physically reached ESP but was never admitted. The
bounded requested-channel 6 versus receiver-channel 1 cases recorded no radio
reception or HID movement; correct channel operation resumed. Periodic radio
status counters can lag downstream datagram counts in individual snapshots.

RP admission/counter observation, full power-cycle metadata reconciliation,
link recovery, factory reset and reconfiguration still require qualification.
Unsupported provisioning-schema handling and replay after acknowledgement
revocation are explicitly included in the observer phase. FNR-008 is not
dependency ready; TiltMouse interoperability/performance is not claimed.
