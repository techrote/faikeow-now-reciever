#!/usr/bin/env python3
"""Host regression for the secret-separated FNR-007 source generator."""
from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
GEN = ROOT / "tools" / "generate_provisioning.py"
DUMMY_KEY = "00112233445566778899aabbccddeeff"

doc = {
    "schema_version": 1,
    "config_id": "0x0123456789abcdef",
    "generation": 7,
    "peer_mac": "02:03:04:05:06:07",
    "channel": 6,
    "platform_version": 1,
    "profile_id": 1,
    "profile_schema_version": 1,
    "freshness_timeout_ms": 250,
    "radio_key_hex": DUMMY_KEY,
}

with tempfile.TemporaryDirectory() as tmp:
    root = pathlib.Path(tmp)
    cfg = root / "provisioning.json"
    out = root / "generated"
    cfg.write_text(json.dumps(doc), encoding="utf-8")
    subprocess.run(
        [sys.executable, str(GEN), "--config", str(cfg), "--out-dir", str(out)],
        check=True, capture_output=True, text=True)

    rp = (out / "rp_provisioning_image.c").read_text(encoding="utf-8")
    esp = (out / "esp_provisioning_image.cpp").read_text(encoding="utf-8")
    redacted_text = (out / "provisioning-redacted.json").read_text(encoding="utf-8")
    redacted = json.loads(redacted_text)

    key_bytes = bytes.fromhex(DUMMY_KEY)
    key_tokens = [f"0x{b:02x}" for b in key_bytes]
    assert not all(token in rp for token in key_tokens), "RP source contains radio key bytes"
    assert all(token in esp for token in key_tokens), "ESP source omitted radio key"
    assert DUMMY_KEY not in redacted_text
    assert "radio_key_hex" not in redacted
    assert redacted["radio_key_present"] is True
    assert redacted["config_id"] == "0x0123456789abcdef"
    assert redacted["peer_mac"] == "02:03:04:05:06:07"
    assert len(redacted["public_record_sha256"]) == 64

    subprocess.run([
        "cc", "-std=c17", "-Wall", "-Wextra", "-Werror", "-Wconversion", "-Wshadow",
        "-I", str(ROOT / "shared" / "include"),
        "-c", str(out / "rp_provisioning_image.c"),
        "-o", str(root / "rp_provisioning_image.o"),
    ], check=True)
    subprocess.run([
        "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Wconversion", "-Wshadow",
        "-I", str(ROOT / "shared" / "include"),
        "-I", str(ROOT / "firmware" / "esp8266"),
        "-c", str(out / "esp_provisioning_image.cpp"),
        "-o", str(root / "esp_provisioning_image.o"),
    ], check=True)

    bad = dict(doc)
    bad["radio_key_hex"] = "00" * 16
    cfg.write_text(json.dumps(bad), encoding="utf-8")
    failed = subprocess.run(
        [sys.executable, str(GEN), "--config", str(cfg), "--out-dir", str(out)],
        capture_output=True, text=True)
    assert failed.returncode != 0

print("PASS: FNR-007 paired generator keeps key out of RP/redacted outputs")
