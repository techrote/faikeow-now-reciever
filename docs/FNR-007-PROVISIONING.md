# FNR-007 — Generic provisioning and recovery

FNR-007 makes the generic receiver deployable without a universal secret and
without defining TiltMouse as the only possible sender.

## Provisioning model

v0.1 uses **paired build-time provisioning**.

One local JSON configuration generates:

- an RP2040 source containing only a public, CRC-protected configuration record;
- an ESP8285 source containing the same public record plus one 16-byte
  per-device ESP-NOW peer key;
- a redacted manifest containing no key material.

The generated sources live under `build/` and are ignored. They are not
repository artifacts and must not be committed.

Factory/default firmware contains no provisioning record and no radio key. It is
therefore fail-closed.

This design deliberately avoids a writable runtime configuration filesystem in
v0.1. Persistence is the normal MCU flash persistence of the generated firmware
images. Reconfiguration is performed by generating and flashing a new paired
image set through the already accepted no-solder RP2040 BOOTSEL and ESP recovery
paths.

## Local input schema

Keep the input outside the repository. A typical local file is:

```json
{
  "schema_version": 1,
  "config_id": "0x0123456789abcdef",
  "generation": 1,
  "peer_mac": "02:03:04:05:06:07",
  "channel": 6,
  "platform_version": 1,
  "profile_id": 1,
  "profile_schema_version": 1,
  "freshness_timeout_ms": 250,
  "radio_key_hex": "<32 hexadecimal characters>"
}
```

Rules:

- `config_id` is a nonzero public deployment identifier, not a key;
- `generation` is a nonzero public configuration revision;
- `peer_mac` is the allowed sender station MAC and must be unicast/nonzero;
- channel is 1..14;
- platform/profile versions must match the frozen FNR-006 v1 contract;
- the ESP-NOW key is exactly 16 nonzero bytes;
- v0.1 provisioned production operation requires encryption;
- there is no production plaintext fallback.

The key must be unique to the deployment/device pair in normal use. No
repository-wide production key is defined.

## Public record v1

The public record is exactly 40 bytes. Multibyte fields are little endian.

| Offset | Width | Field |
| ---: | ---: | --- |
| 0 | 4 | ASCII `FNR7` |
| 4 | 1 | record version = 1 |
| 5 | 1 | flags: provisioned + encryption-required |
| 6 | 1 | RF channel |
| 7 | 1 | FNR platform version |
| 8 | 1 | profile ID |
| 9 | 1 | profile schema version |
| 10 | 6 | allowed sender MAC |
| 16 | 4 | application freshness timeout ms |
| 20 | 8 | public `config_id` |
| 28 | 4 | public configuration generation |
| 32 | 4 | reserved, zero |
| 36 | 4 | CRC-32/IEEE over bytes 0..35 |

Any bad magic, version, flags, reserved value, CRC or semantic field causes
fail-closed unprovisioned behavior.

The RP2040 image never contains the radio key.

## Build

Factory/unprovisioned builds remain unchanged:

```sh
bash tools/build_rp2040.sh
FNR_SOURCE_ID="$(git rev-parse HEAD)" bash tools/build_esp.sh
```

Provisioned paired builds use the **same** local file for both lanes:

```sh
export FNR_PROVISIONING_CONFIG=/absolute/path/outside/repo/provisioning.json

bash tools/build_rp2040.sh
FNR_SOURCE_ID="$(git rev-parse HEAD)" bash tools/build_esp.sh
```

`tools/generate_provisioning.py` writes:

- `build/provisioning/rp_provisioning_image.c`
- `build/provisioning/esp_provisioning_image.cpp`
- `build/provisioning/provisioning-redacted.json`

The redacted manifest exposes config identity, channel, peer, versions,
freshness and public-record SHA-256 but never the radio key.

Compare the redacted manifest/config identity when pairing separately built
artifacts.

## Radio application and acknowledgement

The ESP production path:

1. validates the public record;
2. requires the ESP-only 16-byte key;
3. selects the configured channel;
4. initializes ESP-NOW;
5. registers the configured sender as an encrypted peer;
6. verifies peer existence/channel using the pinned SDK;
7. registers the receive callback;
8. enters `FNR_RADIO_READY` only after those steps succeed.

A missing key, corrupt record, peer-registration failure or channel/setup error
never falls back to production plaintext receive.

The old explicit plaintext `fnr_radio_apply_config()` seam remains solely for
the already accepted isolated FNR-002/FNR-004 diagnostic firmware. Production
`main.cpp` does not call it.

The RP2040 stages the decoded peer/profile configuration with both admission
gates disabled. It enables them only after an FNR-005 `RADIO_STATUS` reports
`READY` on the expected channel.

Link/radio reset, timeout, unavailable/error status or wrong channel revokes
that acknowledgement and returns the FNR-006 core to peer/profile-disabled
state. A later matching `READY` may re-enable the staged config. Stale movement
is never replayed.

This is the FNR-007 radio-configuration acknowledgement and does not change the
FNR-005 wire format.

## Safe diagnostics

No diagnostic surface returns key bytes.

Available redacted state includes:

- receiver ESP station MAC;
- configured sender peer MAC;
- channel;
- platform/profile versions;
- freshness timeout;
- public config ID/generation;
- key-present/encryption-enabled boolean;
- public record validation result;
- RP staged/radio-acknowledged state;
- current radio state.

The generator's redacted manifest provides the offline artifact-side view. The
adapter snapshot APIs provide firmware-side state for bounded diagnostics and
the FNR-007/FNR-008 physical campaigns.

## Failure behavior

- factory/missing config -> radio off, RP peer/profile disabled;
- corrupt public record -> fail closed;
- missing/all-zero/wrong-size key -> fail closed;
- wrong peer -> ESP ingress and RP authorization reject it;
- wrong channel -> no matching READY acknowledgement / no accepted RF path;
- wrong profile/version -> RP rejects before profile output;
- radio/link reset -> neutral output and admission revoked pending fresh READY;
- USB remains an ordinary profile-defined HID interface; provisioning requires
  no permanent host driver or service.

## Reset and reconfiguration

No soldering is required.

To return to factory-safe state, flash the normal unprovisioned RP2040 and ESP
images using the documented recovery procedures.

To reconfigure, edit/create a new local provisioning JSON, increment
`generation`, normally choose a new per-device radio key/config ID when the
trust relationship changes, rebuild both images and flash the pair.

Original recovery paths from FNR-002 remain authoritative and are not replaced
by this mechanism.

## Security boundary

ESP-NOW encryption protects supported over-air peer traffic, but FNR-007 does
not claim secure boot, flash encryption, tamper resistance or protection against
firmware extraction. A person who extracts the provisioned ESP image can
recover its embedded peer key.

The session discriminator from FNR-006 remains ordering/restart metadata, not
authentication.

## Automated evidence

Software qualification covers:

- exact record/CRC golden vector;
- corruption and semantic rejection;
- 20,000 deterministic codec/hostile cases;
- secret separation between RP and ESP generated sources;
- generated source compilation;
- RP public-record loading and fail-closed behavior;
- ESP key loader and redacted diagnostics;
- actual production ESP radio adapter encrypted-peer setup/failure behavior;
- actual FNR-005 -> RP staged/READY/revoke/re-enable gate;
- UBSan/ASan and GCC `-fanalyzer`;
- both locked target cross-builds and linked-symbol checks.

## Remaining physical acceptance

FNR-007 is not physically accepted merely because these software checks pass.

The issue-owned representative-board smoke still needs to establish:

- receiver ESP MAC discovery;
- one provisioned peer/channel/profile;
- intentional wrong peer/channel/profile safe inactivity;
- correct-config restore;
- power-cycle persistence;
- no-solder reconfiguration/recovery.

Full TiltMouse/reference-sender interoperability remains FNR-008.
