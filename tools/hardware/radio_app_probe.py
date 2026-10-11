"""Observe a CRC-protected UART diagnostic identity on one mapped board.

Use after a separately recorded software or physical boot. This observation
does not reset the board, write flash, or establish factory-firmware behavior.
"""
import argparse
import json
import pathlib
import subprocess
import time
from datetime import datetime, timezone
from radio_backup import validate_cdc
from rp2040_backup import private_destination, sha256
from uart_fault_capture import Decoder, packet


def main():
    import serial
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=['A', 'B'], required=True)
    parser.add_argument('--identities', type=pathlib.Path, required=True)
    parser.add_argument('--port', required=True)
    parser.add_argument('--expected-radio-chip-id', type=lambda s: int(s, 0), required=True)
    parser.add_argument('--write-record', type=pathlib.Path, required=True)
    parser.add_argument('--bridge', type=pathlib.Path, required=True)
    parser.add_argument('--bridge-sha256', required=True)
    parser.add_argument('--boot-kind', choices=['software', 'user-confirmed-cold'], required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    if serial.__version__ != '3.5':
        raise ValueError('Require pinned pyserial 3.5')
    repo = pathlib.Path(__file__).resolve().parents[2]
    destination = private_destination(args.output, repo)
    write = json.loads(args.write_record.read_text())
    if (write['board'] != args.board or not write['exact_readback']
            or sha256(args.write_record.parent / 'readback.bin') != write['image_sha256']
            or sha256(args.bridge) != args.bridge_sha256):
        raise ValueError('Require unchanged verified deployment and explicit bridge image')
    identity = json.loads(args.identities.read_text(encoding='utf-8-sig'))[args.board]
    destination.mkdir(parents=True)
    inventory = destination / 'inventory.json'
    subprocess.run(['pwsh', '-NoProfile', '-File', str(repo / 'tools/hardware/discover_windows.ps1'),
                    '-OutputPath', str(inventory)], check=True)
    initial = json.loads(inventory.read_text(encoding='utf-8-sig'))
    validate_cdc(initial['devices'], args.port, identity)
    decoder = Decoder(); capture = bytearray(); frames = []
    with serial.Serial(args.port, 115200, timeout=.1) as port:
        port.dtr = True; port.rts = False; port.reset_input_buffer()
        port.write(packet(5))
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            data = port.read(1024); capture.extend(data); frames.extend(decoder.feed(data))
            if len(capture) > 16384:
                raise ValueError('Bounded observation capacity exceeded')
            if any(f[0] == 5 for f in frames):
                break
    valid = any(f == (5, args.expected_radio_chip_id, b'FNR-002 ESP link diagnostic v1') for f in frames)
    result = {'board': args.board, 'captured_at_utc': datetime.now(timezone.utc).isoformat(),
              'source_commit_at_start': initial['source_commit'], 'source_dirty_at_start': initial['source_dirty'],
              'boot_kind': args.boot_kind, 'command': 'CRC-protected DIAG_INFO via UART0 115200 8N1',
              'radio_image_sha256': write['image_sha256'], 'bridge_image_sha256': args.bridge_sha256,
              'captured_hex': capture.hex(), 'decoder_bad_frames': decoder.bad_frames,
              'boot_observed': valid, 'result': 'PASS' if valid else 'FAIL'}
    (destination / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    if not valid:
        raise SystemExit('FAIL: expected radio diagnostic identity not observed')
    print('PASS: exact CRC-protected diagnostic identity after ' + args.boot_kind + ' boot')


if __name__ == '__main__':
    main()
