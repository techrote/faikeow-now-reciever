#!/usr/bin/env python3
"""Check linked production paths, without loading/executing either firmware."""
import subprocess
import sys
from dependency_common import DEPS, ROOT

lanes = {
    "rp2040": (
        "arm/bin/arm-none-eabi-nm", "build/rp2040/fnr_rp2040_hid.elf",
        ["fnr_rp_transport_init", "fnr_rp_transport_task", "fnr_transport_rx_feed",
         "fnr_transport_rx_timeout", "fnr_uart_rx_push",
         "fnr_uart_rx_take", "fnr_transport_take_events_foreground",
         "fnr_transport_take_datagram_foreground", "fnr_transport_take_status_foreground",
         "fnr_transport_snapshot_foreground", "fnr_transport_reset_foreground",
         "fnr_receiver_adapter_init", "fnr_transport_foreground_consume",
         "fnr_receiver_core_accept_datagram", "fnr_receiver_core_transport_events",
         "fnr_envelope_decode", "fnr_relative_mouse_profile_make_ops",
         "fnr_rp_provisioning_init", "fnr_receiver_adapter_stage_provisioning",
         "fnr_provisioning_decode", "fnr_provisioning_image_public",
         "fnr_usb_adapter_task", "fnr_hid_device_service"]),
    "esp": (
        "xtensa/bin/xtensa-lx106-elf-nm", "build/esp8266/fnr_esp8266_ingress.elf",
        ["fnr_esp_transport_init", "fnr_esp_transport_task", "fnr_radio_take_foreground",
         "fnr_radio_generation_foreground", "esp_now_register_recv_cb",
         "fnr_transport_tx_offer_datagram", "fnr_transport_tx_restart",
         "fnr_transport_tx_peek", "fnr_transport_tx_advance", "fnr_transport_encode",
         "fnr_esp_provisioning_init", "fnr_radio_apply_provisioning",
         "fnr_provisioning_decode", "fnr_provisioning_image_public",
         "fnr_provisioning_image_radio_key", "esp_now_add_peer"]),
}
if len(sys.argv) != 2 or sys.argv[1] not in lanes:
    raise SystemExit("usage: check_transport_symbols.py rp2040|esp")
tool, image, required = lanes[sys.argv[1]]
elf = ROOT / image
output = subprocess.check_output(
    [str(DEPS / tool), "-g", "--defined-only", str(elf)], text=True, timeout=30)
symbols = {line.split()[-1] for line in output.splitlines() if line.split()}
missing = sorted(set(required) - symbols)
if missing:
    raise SystemExit("Missing production symbols: " + ", ".join(missing))
(elf.parent / "TRANSPORT_SYMBOLS").write_text("\n".join(sorted(required)) + "\n")
print(f"PASS: {sys.argv[1]} production transport/receiver and regression paths linked ({len(required)} symbols)")
