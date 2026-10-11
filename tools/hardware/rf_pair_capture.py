"""Explicit two-board, topology/MAC-guarded RF diagnostic campaign.

Requires independently preserved/recovered radios and labeled RP bridges already
deployed. Performs runtime-only peer/channel tests using the isolated RF image.
Raw MACs and captures stay private. No firmware write or infrastructure join.
"""
import argparse
import json
import pathlib
import subprocess
import time
from datetime import datetime, timezone

from rp2040_backup import private_destination, sha256
from radio_flash import validate_backup


def bounded_integer(low, high):
    def parse(text):
        value = int(text)
        if not low <= value <= high:
            raise argparse.ArgumentTypeError(f'Require {low}..{high}')
        return value
    return parse


def validate_pair(devices, identities, ports):
    candidates = [d for d in devices if d['instance_id'].upper().startswith('USB\\VID_2E8A&PID_000A&MI_00\\')]
    if len(candidates) != 2 or ports['A'] == ports['B']:
        raise ValueError('Require exactly two distinct labeled diagnostic CDC ports')
    for board in ('A', 'B'):
        path = identities[board]['location_path']
        matches = [d for d in candidates if path + '#USBMI(0)' in d['properties'].get('DEVPKEY_Device_LocationPaths', [])]
        parents = [d for d in devices if d['instance_id'].upper() == 'USB\\VID_2E8A&PID_000A\\FNR002' + board]
        if (len(matches) != 1 or not matches[0]['description'].endswith('(' + ports[board] + ')')
                or len(parents) != 1 or path not in parents[0]['properties'].get('DEVPKEY_Device_LocationPaths', [])):
            raise ValueError('Explicit port/fixture label/topology mismatch')


def assess(tx, rx, kind, count, sender_mac, receive_channel=None, transmit_channel=None,
           power_limit_qdbm=None):
    reasons = []
    if ((receive_channel is not None and rx['channel'] != receive_channel)
            or (transmit_channel is not None and tx['channel'] != transmit_channel)):
        reasons.append('final SDK channel differs from requested configuration')
    if power_limit_qdbm is not None and any(
            sample.get('power_limit_qdbm') != power_limit_qdbm for sample in (tx, rx)):
        reasons.append('final diagnostic power ceiling differs from request')
    if tx['attempted'] != count or tx['queued'] != count or tx['enqueue_fail'] or tx['outstanding']:
        reasons.append('TX queue/callback completion')
    if tx['tx_ok'] + tx['tx_callback_fail'] != count:
        reasons.append('missing TX callbacks')
    if any(rx[k] for k in ('integrity_errors', 'duplicates', 'invalid_length', 'uart_overrun', 'uart_rx_error')):
        reasons.append('RX integrity/UART errors')
    if tx['uart_overrun'] or tx['uart_rx_error']:
        reasons.append('TX UART errors')
    if kind == 'wrong_channel':
        if rx['received'] or rx['accepted'] or rx['checked']:
            reasons.append('unexpected wrong-channel reception')
        if tx['tx_ok']:
            reasons.append('unexpected wrong-channel acknowledgement')
    elif kind == 'reject_source':
        if rx['received'] != count or rx['rejected_source'] != count or rx['accepted'] or rx['checked']:
            reasons.append('source admission failed')
    else:
        if tx['tx_ok'] != count or tx['tx_callback_fail']:
            reasons.append('RF TX delivery failures')
        if rx['accepted'] != count or rx['rejected_source'] or rx['last_source'] != sender_mac:
            reasons.append('RF reception/source mismatch')
        if kind == 'backpressure':
            if (not rx['dropped'] or rx['checked'] + rx['dropped'] != count
                    or rx['consumed'] != rx['checked'] or rx['gaps'] != rx['dropped']):
                reasons.append('bounded backpressure accounting')
        elif rx['checked'] != count or rx['consumed'] != count or rx['dropped'] or rx['gaps']:
            reasons.append('missing/extra RF payloads or drops')
    return reasons


def main():
    import serial
    if serial.__version__ != '3.5':
        raise ValueError('Require pinned pyserial 3.5')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--identities', type=pathlib.Path, required=True)
    parser.add_argument('--port-A', required=True)
    parser.add_argument('--port-B', required=True)
    parser.add_argument('--backup-A', type=pathlib.Path, required=True)
    parser.add_argument('--backup-B', type=pathlib.Path, required=True)
    parser.add_argument('--radio-write-A', type=pathlib.Path, required=True)
    parser.add_argument('--radio-write-B', type=pathlib.Path, required=True)
    parser.add_argument('--radio-image-sha256', required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--direction', choices=['both', 'A-to-B', 'B-to-A'], default='both')
    parser.add_argument('--receive-channel', type=bounded_integer(1, 14), default=1)
    parser.add_argument('--mismatch-channel', type=bounded_integer(1, 14), default=6,
                        help='Sender channel for wrong_channel only; must differ from receive channel')
    parser.add_argument('--mismatch-count', type=bounded_integer(1, 20000), default=100)
    parser.add_argument('--separation-cm', type=bounded_integer(1, 100000),
                        help='Operator-reported antenna separation; recorded, not measured by this tool')
    parser.add_argument('--power-limit-qdbm', type=bounded_integer(0, 82),
                        help='Optional diagnostic SDK ceiling, quarter dBm; requires POWER-capable image')
    parser.add_argument('--case', action='append', choices=['one_byte', 'representative_32',
                        'representative_128', 'full_250', 'burst', 'backpressure',
                        'reject_source', 'wrong_channel', 'receiver_restart', 'sustained'],
                        help='Run only selected cases, preserving prior captures separately')
    args = parser.parse_args()
    if args.receive_channel == args.mismatch_channel:
        parser.error('Mismatch channel must differ from receive channel')
    repo = pathlib.Path(__file__).resolve().parents[2]
    destination = private_destination(args.output, repo)
    identities = json.loads(args.identities.read_text(encoding='utf-8-sig'))
    ports = {'A': args.port_A, 'B': args.port_B}
    macs, writes = {}, {}
    for board in ('A', 'B'):
        backup_path = getattr(args, 'backup_' + board)
        record = json.loads((backup_path / 'backup.json').read_text())
        macs[board] = record['reads'][0]['mac']
        validate_backup(backup_path, board, macs[board])
        path = getattr(args, 'radio_write_' + board)
        write = json.loads(path.read_text())
        if (write['board'] != board or write['mode'] != 'diagnostic' or not write['exact_readback']
                or write['image_sha256'] != args.radio_image_sha256
                or sha256(path.parent / 'readback.bin') != args.radio_image_sha256):
            raise ValueError('Require exact RF diagnostic deployment/readback on both boards')
        writes[board] = write
    if macs['A'] == macs['B']:
        raise ValueError('Radio identities are not distinct')
    destination.mkdir(parents=True)
    inventory = destination / 'inventory.json'
    subprocess.run(['pwsh', '-NoProfile', '-File', str(repo / 'tools/hardware/discover_windows.ps1'),
                    '-OutputPath', str(inventory)], check=True)
    initial_inventory = json.loads(inventory.read_text(encoding='utf-8-sig'))
    validate_pair(initial_inventory['devices'], identities, ports)
    streams = {b: serial.Serial(ports[b], 115200, timeout=.2) for b in ('A', 'B')}
    cases = []
    expected_cases = None
    started = time.monotonic()
    log = (destination / 'capture.jsonl').open('w', encoding='utf-8')

    def wait_event(board, event, timeout):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            line = streams[board].readline(2048)
            if not line:
                continue
            raw = line.decode('ascii', errors='replace').strip()
            log.write(json.dumps({'elapsed': time.monotonic() - started, 'board': board, 'raw': raw}) + '\n'); log.flush()
            try:
                value = json.loads(raw)
            except ValueError:
                continue
            if value.get('event') == 'error':
                raise ValueError('Radio diagnostic command error: ' + raw)
            if value.get('event') == event:
                if value.get('identity') != 'FNR-002 RF diagnostic v1' or value.get('mac') != macs[board]:
                    raise ValueError('Live radio diagnostic identity mismatch')
                return value
        raise TimeoutError(board + ' ' + event)

    def command(board, text, event='status'):
        log.write(json.dumps({'elapsed': time.monotonic() - started, 'board': board, 'command': text}) + '\n'); log.flush()
        streams[board].write((text + '\n').encode('ascii'))
        return wait_event(board, event, 5)

    try:
        for board in ('A', 'B'):
            streams[board].dtr = True; streams[board].rts = False
            command(board, 'INFO')  # Verify both live MACs before configuring RF.
            if args.power_limit_qdbm is not None:
                power = command(board, f'POWER {args.power_limit_qdbm}', 'power_set')
                if power.get('power_limit_qdbm') != args.power_limit_qdbm:
                    raise ValueError('Diagnostic did not acknowledge requested power ceiling')
        recipes = [('one_byte', 1, 100, 50), ('representative_32', 32, 200, 10),
                   ('representative_128', 128, 400, 10), ('full_250', 250, 2000, 10),
                   ('burst', 250, 500, 0), ('backpressure', 250, 500, 2),
                   ('reject_source', 250, 100, 10), ('wrong_channel', 250, args.mismatch_count, 10),
                   ('receiver_restart', 250, 200, 10), ('sustained', 250, 6000, 20)]
        if args.case:
            recipes = [recipe for recipe in recipes if recipe[0] in args.case]
        directions = [('A', 'B'), ('B', 'A')]
        if args.direction != 'both':
            directions = [pair for pair in directions if '-to-'.join(pair) == args.direction]
        expected_cases = len(recipes) * len(directions)
        for sender, receiver in directions:
            for kind, length, count, spacing in recipes:
                command(sender, 'OFF', 'off')
                allowed = macs[sender] if kind != 'reject_source' else '02:00:00:00:00:01'
                if allowed == macs[sender] and kind == 'reject_source':
                    raise ValueError('Unexpected peer-test identity collision')
                ready = command(receiver, f'RX {args.receive_channel} {allowed} {length}', 'rx_ready')
                if ready['state'] != 2 or ready['channel'] != args.receive_channel or ready['peer'] != allowed:
                    raise ValueError('Receiver configuration did not take effect')
                if kind == 'receiver_restart':
                    ready = command(receiver, 'RESTART', 'restarted')
                    if ready['state'] != 2:
                        raise ValueError('Receiver restart failed')
                if kind == 'backpressure':
                    command(receiver, 'PAUSE 800', 'paused')
                channel = args.mismatch_channel if kind == 'wrong_channel' else args.receive_channel
                tx_start = command(sender, f'TX {channel} {macs[receiver]} {length} {count} {spacing}', 'tx_started')
                if tx_start['channel'] != channel or tx_start['peer'] != macs[receiver]:
                    raise ValueError('Sender configuration mismatch')
                tx = wait_event(sender, 'tx_end', count * spacing / 1000 + 30)
                time.sleep(.1)
                rx = command(receiver, 'STATS')
                failures = assess(tx, rx, kind, count, macs[sender], args.receive_channel, channel,
                                  args.power_limit_qdbm)
                result = {'sender': sender, 'receiver': receiver, 'case': kind, 'length': length,
                          'count': count, 'interval_ms': spacing, 'tx_channel': channel,
                          'rx_channel': args.receive_channel,
                          'tx': tx, 'rx': rx, 'result': 'FAIL' if failures else 'PASS', 'failures': failures}
                cases.append(result)
                (destination / 'cases.json').write_text(json.dumps(cases, indent=2) + '\n')
                print(f'{sender}->{receiver} {kind}: {result["result"]}', flush=True)
                if failures:
                    raise ValueError(', '.join(failures))
    finally:
        for board in streams:
            try:
                command(board, 'OFF', 'off')
            except (OSError, ValueError, TimeoutError):
                pass
            streams[board].close()
        log.close()
        result = {'captured_at_utc': datetime.now(timezone.utc).isoformat(),
                  'source_commit_at_start': initial_inventory['source_commit'],
                  'source_dirty_at_start': initial_inventory['source_dirty'],
                  'source_commit': subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip(),
                  'radio_image_sha256': args.radio_image_sha256, 'radio_deployments': writes,
                  'direction_selection': args.direction, 'case_selection': args.case,
                  'receive_channel': args.receive_channel, 'mismatch_channel': args.mismatch_channel,
                  'mismatch_count': args.mismatch_count, 'operator_reported_separation_cm': args.separation_cm,
                  'requested_power_limit_qdbm': args.power_limit_qdbm,
                  'expected_cases': expected_cases,
                  'elapsed_seconds': time.monotonic() - started, 'cases_completed': len(cases),
                  'result': 'PASS' if expected_cases is not None and len(cases) == expected_cases
                            and all(c['result'] == 'PASS' for c in cases) else 'FAIL'}
        (destination / 'result.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
