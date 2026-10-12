# Bounded FNR-007 radio sender

Qualification tooling, not TiltMouse or FNR-008 acceptance. Starts with Wi-Fi
off and sends only after an explicit UART command. Maximum 500 packets / 30 s
per command, one outstanding send callback, no queue. UART is 115200 8N1.

Stage `sender.ino` in a private/ignored sketch directory with the production
`radio_auth_crypto.cpp/.h`, `shared/src/radio_auth.c`, shared include directory,
and a private `private_pair.h` defining mutable `receiver_mac[6]`, mutable
`peer_key[16]`, `uint64_t config_id` and `uint32_t config_generation`. Values
come from the same local provisioning JSON plus the discovered receiver MAC.
Never commit that header, generated key source or provisioned firmware binaries.
Use the pinned ESP8285/core/SDK/flash configuration from the production build.

`INFO`/`STATS` report only public state, counters and key presence. `OFF`
deinitializes ESP-NOW and turns Wi-Fi off. It does not restore a changed MAC.

```text
TX channel identity_mode key_mode profile sequence count spacing_ms buttons dx dy
```

Identity 0 restores the native station MAC; identity 1 changes its local bit and
last byte. The synthetic payload remains platform v1 / relative_mouse schema 1
even when a different generic profile ID is selected for rejection testing.

| Key mode | SDK peer encryption | Application wrapper |
| ---: | --- | --- |
| 0 | Correct key | Correct wrapper |
| 1 | Mismatched key | Mismatched authentication key |
| 2 | Disabled | Bare platform packet (retained physical regression) |
| 3 | Correct key | Bare platform packet |
| 4 | Disabled | Incorrect authentication key |
| 5 | Correct key | One tag bit altered |
| 6 | Correct key | Wrong configuration generation |
| 7 | Disabled | Valid wrapper (measures SDK encryption-attestation limit) |

Session is a random nonzero restart discriminator generated at boot. Sequence
is explicit per command and increments per packet. Reusing old sequences in the
same session intentionally exercises duplicate/stale rejection. A send callback
does not prove receiver admission or HID output. Record receiver counters and
target-filtered USB-host observations separately.
