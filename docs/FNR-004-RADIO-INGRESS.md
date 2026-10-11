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
the real registration path link-reachable in an otherwise unprovisioned image. The build still uses
an unverified ESP8285 flash surrogate. FNR-007 owns persisted configuration.
The MAC admission check excludes zero, broadcast and multicast addresses.
The exact MAC match is **not cryptographic sender authentication**.
No infrastructure association, SoftAP, DHCP application or socket is used.
There is no RSSI field in this NONOS callback ABI.

## Interfaces for FNR-005

`fnr_radio_take_foreground(fnr_datagram *out)` returns false for no pending
record, invalid receiver state or null output; success copies the entire owned
record into caller storage (250-byte bounded payload, exact six-byte source,
size). The record remains owned by the caller independently of further receive
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

## Evidence limits

Native tests exercise ownership, length bounds (including 251–255),
configuration, filtering, overflow, ordering, restart and saturation.
Target compilation/CI must independently establish the pinned SDK symbol/API
compatibility, ELF/BIN provenance and RP2040 regression status. **Neither a
cross-build nor native simulation proves physical ESP-NOW reception.**
FNR-002 still needs to establish the actual chip, flashing/recovery, internal
MCU interconnect and board resource contract. FNR-005 must attach its own
framed generic consumer once that contract exists.
