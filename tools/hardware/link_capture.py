"""Capture a bounded SRAM link test with private provenance and strict results."""
import argparse
import json
import pathlib
import subprocess
import time
from datetime import datetime, timezone
from rp2040_backup import private_destination
from radio_backup import validate_cdc


def assess(end, seconds, chip_id):
    errors = ['gaps', 'pattern_errors', 'crc_errors', 'length_errors', 'parser_overflows',
              'uart_overruns', 'uart_framing', 'uart_parity', 'uart_breaks', 'ring_drops']
    reasons = [name for name in errors if end[name] != 0]
    if end['active_duration_us'] < seconds * 1000000:
        reasons.append('duration too short')
    if end['echoes'] != end['sent']:
        reasons.append('missing echoes/timeouts')
    if not end['radio_seen'] or end['radio_id'] != chip_id:
        reasons.append('radio diagnostic identity mismatch')
    radio = end['radio_stats']
    if (not end['radio_stats_received'] or len(radio) != 9
            or radio[0] != end['sent'] or radio[3] != end['streams']):
        reasons.append('radio statistics missing or disagree')
    elif any(radio[i] for i in [1, 2, 4, 5, 6, 7, 8]):
        reasons.append('radio integrity/UART/parser errors')
    threshold = 10000 if seconds == 600 else 100
    if min(end['sent'], end['echoes'], end['streams']) < threshold:
        reasons.append('insufficient frames')
    return reasons


def main():
    import serial
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=['A', 'B'], required=True)
    parser.add_argument('--identities', type=pathlib.Path, required=True)
    parser.add_argument('--port', required=True)
    parser.add_argument('--seconds', type=int, choices=[10, 600], required=True)
    parser.add_argument('--expected-radio-chip-id', type=lambda s: int(s, 0), required=True)
    parser.add_argument('--rp-image-sha256', required=True)
    parser.add_argument('--radio-write-result', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    repo = pathlib.Path(__file__).resolve().parents[2]
    destination = private_destination(args.output, repo)
    write = json.loads(args.radio_write_result.read_text())
    if write['board'] != args.board or write['mode'] != 'diagnostic' or not write['exact_readback']:
        raise ValueError('Require this board\'s successful diagnostic write/readback record')
    if len(args.rp_image_sha256) != 64 or any(c not in '0123456789abcdef' for c in args.rp_image_sha256):
        raise ValueError('Record the validated loaded RP image SHA256')
    identity = json.loads(args.identities.read_text(encoding='utf-8-sig'))[args.board]
    destination.mkdir(parents=True)
    inventory = destination / 'inventory.json'
    subprocess.run(['pwsh', '-NoProfile', '-File', str(repo / 'tools/hardware/discover_windows.ps1'),
                    '-OutputPath', str(inventory)], check=True)
    validate_cdc(json.loads(inventory.read_text(encoding='utf-8-sig'))['devices'], args.port, identity)
    ended = None
    started = time.monotonic()
    with serial.Serial(args.port, 115200, timeout=1) as port:
        port.dtr = True
        port.reset_input_buffer()
        port.write(f'RUN{args.seconds}\n'.encode())
        with (destination / 'capture.jsonl').open('w', encoding='utf-8') as log:
            while time.monotonic() - started < args.seconds + 15:
                line = port.readline(2048)
                if not line:
                    continue
                text = line.decode('ascii', errors='replace').strip()
                log.write(json.dumps({'host_monotonic_elapsed': time.monotonic() - started,
                                      'raw_line': text}) + '\n')
                log.flush()
                try:
                    sample = json.loads(text)
                except ValueError:
                    continue
                print(text, flush=True)
                if sample.get('event') == 'end':
                    ended = sample
                    break
    reasons = assess(ended, args.seconds, args.expected_radio_chip_id) if ended else ['no final marker/timeout']
    result = {'board': args.board, 'captured_at_utc': datetime.now(timezone.utc).isoformat(),
              'source_commit': subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip(),
              'rp_image_sha256': args.rp_image_sha256, 'radio_image_sha256': write['image_sha256'],
              'baud': 115200, 'seed': '0x002f4a91', 'requested_seconds': args.seconds,
              'test_kind': 'sustained integrity' if args.seconds == 600 else 'smoke only',
              'end': ended, 'result': 'FAIL' if reasons else 'PASS', 'failures': reasons}
    (destination / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    if reasons:
        raise SystemExit('FAIL: ' + ', '.join(reasons))
    print('PASS: measured run; see private result; separate fault cases remain')


if __name__ == '__main__':
    main()
