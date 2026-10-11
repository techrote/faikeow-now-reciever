"""Read one mapped radio RF diagnostic identity without configuring networking."""
import argparse
import json
import pathlib
import subprocess
import time
from datetime import datetime, timezone
from radio_backup import validate_cdc
from rp2040_backup import private_destination, sha256


def main():
    import serial
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=['A', 'B'], required=True)
    parser.add_argument('--identities', type=pathlib.Path, required=True)
    parser.add_argument('--port', required=True)
    parser.add_argument('--expected-mac', required=True)
    parser.add_argument('--write-record', type=pathlib.Path, required=True)
    parser.add_argument('--boot-kind', choices=['software', 'user-confirmed-cold'], required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    repo = pathlib.Path(__file__).resolve().parents[2]
    destination = private_destination(args.output, repo)
    write = json.loads(args.write_record.read_text())
    if (write['board'] != args.board or write['mode'] != 'diagnostic' or not write['exact_readback']
            or sha256(args.write_record.parent / 'readback.bin') != write['image_sha256']):
        raise ValueError('Require unchanged selected RF image readback record')
    identity = json.loads(args.identities.read_text(encoding='utf-8-sig'))[args.board]
    destination.mkdir(parents=True)
    inventory = destination / 'inventory.json'
    subprocess.run(['pwsh', '-NoProfile', '-File', str(repo / 'tools/hardware/discover_windows.ps1'),
                    '-OutputPath', str(inventory)], check=True)
    initial = json.loads(inventory.read_text(encoding='utf-8-sig'))
    validate_cdc(initial['devices'], args.port, identity)
    lines = []; observation = None
    with serial.Serial(args.port, 115200, timeout=.2) as port:
        port.dtr = True; port.rts = False
        port.write(b'INFO\n')
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            raw = port.readline(2048)
            if not raw: continue
            if len(lines) >= 128: raise ValueError('RF identity capture exceeds bound')
            text = raw.decode('ascii', errors='replace').strip(); lines.append(text)
            try: sample = json.loads(text)
            except ValueError: continue
            if sample.get('event') != 'status': continue
            if (sample.get('identity') != 'FNR-002 RF diagnostic v1'
                    or sample.get('mac') != args.expected_mac.lower()):
                raise ValueError('Live RF application identity mismatch')
            observation = sample; break
    result = {'board': args.board, 'captured_at_utc': datetime.now(timezone.utc).isoformat(),
              'source_commit_at_start': initial['source_commit'], 'source_dirty_at_start': initial['source_dirty'],
              'image_sha256': write['image_sha256'], 'boot_kind': args.boot_kind,
              'command': 'INFO via UART0 115200 8N1; no RF configuration',
              'lines': lines, 'observation': observation, 'result': 'PASS' if observation else 'FAIL'}
    (destination / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(result['result'] + ': selected RF diagnostic application identity')
    if not observation: raise SystemExit(1)


if __name__ == '__main__': main()
