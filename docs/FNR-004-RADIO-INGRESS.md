# FNR-004 — Generic radio ingress contract and qualification

## Status and scope

The radio adapter is an ESP8266 Arduino core 3.1.2/NONOS implementation. The
shared C module has no Arduino dependencies. FNR-004 does not interpret datagram
contents and does not implement a physical RP2040 transport.

## Provisioning

**Fail closed** is the shipping default: `FNR_RADIO_ENABLE` is undefined and
radio startup leaves Wi-Fi OFF. To build a *locally provisioned* receiver, supply
all eight compile-time definitions:
`FNR_RADIO_ENABLE=1`, `FNR_CHANNEL` (1–14) and
`FNR_PEER_0` through `FNR_PEER_5` (six decimal MAC octets).
They are build parameters, not hard-coded source secrets. Alternatively,
`fnr_radio_apply_config(const fnr_radio_config *)` accepts a copied foreground
configuration and restarts reception; null revokes admission. This seam makes
the real registration path link-reachable in an otherwise unprovisioned image.
FNR-002 independently established ESP8285N08 with 1 MiB embedded flash on both
units; the authoritative [devboard reference](03-HARDWARE.md) owns the hardware
and programming contract. FNR-007 owns persisted configuration.
The MAC admission check excludes zero, broadcast and multicast addresses.
The exact MAC match is **not cryptographic sender authentication**.
No infrastructure association, SoftAP, DHCP application or socket is used.
There is no RSSI field in this NONOS callback ABI.

## Interfaces for FNR-005

`fnr_radio_take_foreground(fnr_datagram *out)` returns false for no pending
record, invalid receiver state or null output; success copies the entire owned
record into caller storage (250-byte bounded payload, exact six-byte source,
size). FNR-005's production foreground task now owns this explicit take seam;
the former optional weak consumer in ESP `main.cpp` has been removed so there
are no competing consumers. At most one record is extracted per foreground
iteration, then serialized through the accepted UART configuration. The record
remains owned by the caller independently of further receive
callbacks. One pending record is held: **newest wins**, replacing an older
unconsumed record and saturating the dropped counter. No event merging or
hidden retransmission is performed. Order is the order of accepted callback
invocations, subject to the explicit newest-wins drop rule.
`fnr_radio_status_foreground(fnr_radio_ingress *out)` copies a status/ingress
snapshot; `fnr_radio_station_mac(uint8_t out[6])` retrieves the SDK station MAC.
Counters saturate at UINT32_MAX; they persist across `fnr_radio_restart()`.
Reset invalidates the pending record and counts its discard. The status includes configuration
(channel + allowed MAC), state and accepted/rejected/loss counters, but no keys.
The current implementation provisions one exact source MAC. Extending to a
bounded peer list belongs to FNR-007.

`fnr_radio_generation_foreground()` is an additional foreground lifecycle token.
It advances modulo 2^32 on every start attempt, including unconfigured/error
attempts, independently of saturating counters. FNR-005 observes it between
lifecycle operations, retires old transport-owned records, advances its separate
wire generation and emits a RESET status. It is not an RF sender session or a
physical restart proof. See [the FNR-005 contract](FNR-005-TRANSPORT.md).

## Concurrency and callback behavior

The SDK callback type is statically asserted as
`void (*)(uint8_t*,uint8_t*,uint8_t)`; this is **not** the ESP32 callback.
Shared-slot, diagnostics, transition, and pop operations are protected in the
firmware adapter by short interrupt-masked sections and never yield inside
them. The portable module exposes a serially-called contract, **not an
internally lock-free MPMC queue**. This depends on the NONOS cooperative
SDK callback and foreground not concurrently executing on another core or
reentering the adapter within a masked section. The public header did not
supply a stronger formal scheduling guarantee, so target stress/reentrancy
verification remains a physical acceptance question. The callback only validates
length/source, increments bounded counters and copies at most 250 bytes.
It does not allocate, log, block, parse a profile or write to a guessed UART.

## Initialization and recovery

STARTING -> READY requires station mode, explicit channel verification,
`esp_now_init`, receiver role `ESP_NOW_ROLE_SLAVE` and successful callback
registration. Failures enter ERROR with pending state invalidated. Restart
unregisters the callback/deinitializes ESP-NOW when active, invalidates pending
data and reapplies explicit configuration. UNCONFIGURED turns Wi-Fi off.
The channel is not a claimed regulatory-domain or physical link validation.

## Channel selection and admission

The configured channel selects the SDK radio channel and startup checks the
SDK's reported value. It is not a per-datagram channel admission field. The
pinned NONOS receive callback supplies source MAC, data and length, with no
physical receive-channel metadata. Exact-source admission is performed in the
portable ingress module; channel selection cannot substitute for that check or
for future cryptographic policy.

The [FNR-004 follow-up evidence](../evidence/FNR-004/README.md) resolves the
receiver contract: mismatch reception varies with distance and requested power,
and zero delivery across software-reported channels is not guaranteed. The
original FNR-002 failure is preserved unchanged. At fixed operator-reported
20 cm, B-to-A high/low/high power tests accepted 36/300, 2/1,000 and 12/300
mismatch packets; all same-channel burst and source-rejection controls passed.
At 180 cm, original-channel focused tests received zero in both directions,
while two other matrix channel pairs still received packets. Lower power and
greater separation reduce observed mismatch reception but do not establish
isolation. Every observed mismatch reception remains a failed zero-reception
test, including residual low-power reception.

SDK channel values alone do not establish actual over-the-air frequency, and
a failed transmit callback does not prove that no packet reached the receiver.
The precise RF mechanism was not measured; strong-signal/selectivity effects
are a hypothesis, not a diagnosed circuit fault. No production software cure
is asserted or required to uphold the documented source/length/handoff contract.
Channel choice is not authentication or per-packet physical-channel admission.
FNR-007 owns future key/provisioning policy. FNR-008 still owns integrated
receiver/profile acceptance.

## Evidence limits

Native tests exercise ownership, length bounds (including 251–255),
configuration, filtering, overflow, ordering, restart and saturation.
Target compilation/CI must independently establish the pinned SDK symbol/API
compatibility, ELF/BIN provenance and RP2040 regression status. **Neither a
cross-build nor native simulation proves physical ESP-NOW reception.**
FNR-002 established actual chip identity, flashing/recovery, the onboard UART0
GP0/GP1 interconnect and independent UART integrity. Reuse those unchanged
results through the authoritative devboard reference. FNR-005's software
consumer is attached, while its [production physical framing campaign](FNR-005-HARDWARE-HANDOFF.md)
remains separate. Diagnostic UART/RF exchanges do not establish that production
transport or integrated HID/profile acceptance.
