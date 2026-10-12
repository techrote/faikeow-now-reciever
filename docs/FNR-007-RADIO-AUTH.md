# FNR-007 authenticated radio wrapper v1

This wrapper repairs the plaintext admission failure retained in
[physical evidence](../evidence/FNR-007/README.md). The operator authorized this
sender-contract change within FNR-007. It applies to provisioned production
radio traffic. The enclosed [FNR platform v1 packet](FNR-006-RECEIVER.md) and
FNR-005 internal framing remain byte-for-byte unchanged.

## Wire contract

| Offset | Width | Field |
| ---: | ---: | --- |
| 0 | 4 | ASCII `FNA1`, wrapper version 1 |
| 4 | 2 | enclosed packet length, unsigned little endian |
| 6 | N | complete FNR platform packet, 1..228 bytes |
| 6+N | 16 | first 16 bytes of HMAC-SHA-256 digest |

Total length must be exactly N+22 and at most 250 bytes. There is no legacy
unwrapped production path. A platform v1 packet must also satisfy its own
18-byte header/profile rules; this generic radio layer does not interpret them.
The deployed profile-payload capacity is now **210 bytes** (228 minus 18).
The existing relative-mouse packet is 22 bytes enclosed, 44 bytes over radio.

## Authentication

`K` is the deployment's existing secret 16-byte ESP-NOW peer key. Derive:

```text
KA = HMAC-SHA-256(K, ASCII "FNR-radio-auth-key-v1")
M  = ASCII "FNR-radio-auth-v1"
     || config_id LE64 || generation LE32
     || sender station MAC (6 bytes) || receiver station MAC (6 bytes)
     || wire header (6 bytes) || enclosed packet (N bytes)
tag = HMAC-SHA-256(KA, M)[0:16]
```

ASCII strings contain no NUL terminator. The MAC addresses are raw bytes in
display order, not ASCII. The tag is raw digest bytes with no integer encoding.
The receiver compares all 16 tag bytes before accepting. The configuration ID
and generation must be nonzero and must change when reconfiguring the public
deployment. A trust-relationship change also requires a fresh key. The receiver
station MAC comes from the redacted provisioning diagnostic, not an assumed
clone identity.

The receiver uses BearSSL supplied by the pinned ESP8266 Arduino core. It checks
RFC 4231 test case 1 and the independent wrapper vector below before READY.
The maximum authentication message is 275 bytes; buffers and callback work are
bounded and allocate no heap. Derived keys and crypto contexts are wiped after
use. The RP image contains neither K nor KA and receives only verified enclosed
packets through the unchanged FNR-005 datagram path.

## Public interoperability vector

These are public test values, never a deployment key:

```text
K:          0102030405060708090a0b0c0d0e0f10
config_id:  0x123456789abcdef0
generation: 1
sender:     02:03:04:05:06:07
receiver:   0a:0b:0c:0d:0e:0f
inner:      010203
wire:       464e413103000102030ee8091a2e3299194cee5459c9c8b018
```

The three-byte inner value deliberately tests wrapper independence; it is not
a valid platform v1 application packet. Python `hmac.digest` / `hashlib.sha256`
independently produced this vector; the actual ESP crypto self-test checks it.

## Security and measurement boundary

Production still requires successful encrypted ESP-NOW peer registration and
exact sender-MAC admission. However, the pinned SDK callback cannot attest an
individual packet's RF channel or encryption status. Physical qualification
showed that an unwrapped plaintext packet could reach the old production path
despite encrypted peer registration. The pinned Arduino core
[documents that limitation](https://github.com/esp8266/Arduino/blob/3.1.2/libraries/ESP8266WiFiMesh/README.md).

The wrapper provides key possession and packet/configuration/address integrity
independently of that callback limitation. `encryption_enabled` diagnoses peer
registration; it is not a per-packet encryption assertion. A sender that knows
the key can construct a valid tag even if it deliberately disables its link
encryption. The receiver cannot distinguish that from an SDK-decrypted packet.
There is no receiver plaintext fallback, and unwrapped or incorrectly tagged
traffic is always rejected on the provisioned path.

Channel selection is not authentication or RF isolation. Wrong-channel behavior
must be measured. HMAC does not add secrecy to plaintext link traffic, secure
boot, tamper resistance or protection against ESP firmware extraction. FNR-006
retains authority over profile/session/order/freshness handling. The wrapper
does not establish persistent replay protection across receiver restarts or
cryptographic wall-clock freshness; a recorded authenticated packet remains
cryptographically authentic. Session IDs are restart discriminators, not keys.

## Diagnostics

The ESP snapshot exposes `authentication_enabled` (READY with the production
authentication gate) and saturating `authentication_rejected`. That counter
counts malformed/bad-tag packets from the configured MAC; wrong MACs remain
`rejected_source`. A failed authentication increments received but never
accepted and never publishes a datagram. No key bytes or derived keys are
reported. FNR-005 status fields remain unchanged, so the additional counter is
available only through the redacted ESP diagnostic snapshot, not the UART
production wire format.
