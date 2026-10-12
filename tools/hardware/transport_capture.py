"""Guarded bounded FNR-005 control/capture. No flashing or implicit device selection.

The reviewed JSON plan contains only bounded command/wait/expect operations.
Every command and response is recorded privately. Assessment is a separate
explicit operation; successful capture alone never means physical PASS.
"""
import argparse
import contextlib
import hashlib
import json
import pathlib
import re
import subprocess
import time
from datetime import datetime, timezone
from rp2040_backup import private_destination, sha256

COMMAND = re.compile(r'(INFO|S|R|H|B|E [0-9]+|[CFI] [0-9]+|X (STOP|SNAP|MUTE|RESUME|RESTART|REBOOT|HASH|ZERO|FAULT [0-9]+|RUN [0-9]+ [0-9]+ [0-9]+ [0-9]+))\Z')

FIRMWARE_INPUTS = ('dependencies.lock.json', '.gitattributes', 'CMakeLists.txt',
                   'shared', 'firmware', 'diagnostics/transport-qualification/esp',
                   'diagnostics/transport-qualification/rp2040',
                   'diagnostics/transport-qualification/pattern.h')


def validate_firmware_source(repo, source):
    """Permit later evidence/tool commits only when compiled inputs are identical.

    Images remain pinned to their actual committed source, never relabeled as a
    later HEAD. This avoids reflashing merely because evidence was committed.
    """
    if not re.fullmatch(r'[0-9a-f]{40}', source):
        raise ValueError('Exact firmware source commit required')
    if subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain'], text=True).strip():
        raise ValueError('Require a clean committed tool/source tree')
    for command in (['merge-base', '--is-ancestor', source, 'HEAD'],
                    ['diff', '--quiet', source, 'HEAD', '--', *FIRMWARE_INPUTS]):
        result = subprocess.run(['git', '-C', str(repo), *command], capture_output=True)
        if result.returncode:
            raise ValueError('Firmware source is not an ancestor with identical compiled inputs')
    return subprocess.check_output(['git', '-C', str(repo), 'rev-parse', source + '^{tree}'], text=True).strip()


def validate_plan(plan):
    if not isinstance(plan, list) or not 1 <= len(plan) <= 256:
        raise ValueError('Plan requires 1..256 bounded steps')
    duration = 0
    for step in plan:
        if not isinstance(step, dict) or len(step) != 1: raise ValueError('One action per step required')
        key, value = next(iter(step.items()))
        if key == 'command':
            if not isinstance(value, str) or not COMMAND.fullmatch(value): raise ValueError('Unsupported command')
            numbers = [int(v) for v in re.findall(r'\b[0-9]+\b', value)]
            if any(v > 0xffffffff for v in numbers): raise ValueError('Numeric command overflow')
            if value.startswith(('C ', 'F ')) and numbers[0] > 10000: raise ValueError('Pause limit')
            if value.startswith('I ') and numbers[0] > 1000: raise ValueError('IRQ pause limit')
            if value.startswith('X FAULT ') and numbers[0] > 13: raise ValueError('Fault limit')
            if value.startswith('X RUN '):
                count, length, rate, first = numbers
                if not 1 <= count <= 30000 or not 1 <= length <= 250 or not 1 <= rate <= 200:
                    raise ValueError('RUN bounds')
                if first + count > 0xffffffff: raise ValueError('Pattern ID wrap outside campaign')
        elif key == 'wait':
            if type(value) not in (int, float) or not 0 <= value <= 610: raise ValueError('Wait limit')
            duration += value
        elif key == 'expect':
            if value not in ('esp', 'snapshot', 'identity', 'after_esp', 'reset', 'expected', 'command',
                             'hash_reset', 'consumer_pause', 'foreground_pause', 'irq_pause'):
                raise ValueError('Unknown response marker')
            duration += 10
        else: raise ValueError('Unsupported action')
    if duration > 1800: raise ValueError('Total capture duration limit')
    return plan


def validate_selected_cdc(devices, port, identity):
    matches = [d for d in devices if d['instance_id'].upper().startswith('USB\\VID_2E8A&PID_000A&MI_00\\') and
               identity['location_path'] + '#USBMI(0)' in d['properties'].get('DEVPKEY_Device_LocationPaths', [])]
    if len(matches) != 1 or not matches[0]['description'].endswith('(' + port + ')'):
        raise ValueError('Selected board CDC identity/topology is ambiguous or moved')


def main():
    import serial
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=['A', 'B'], required=True)
    parser.add_argument('--identities', type=pathlib.Path, required=True)
    parser.add_argument('--port', required=True)
    parser.add_argument('--manifest', type=pathlib.Path, required=True)
    parser.add_argument('--manifest-sha256', required=True)
    parser.add_argument('--rp-load', type=pathlib.Path, required=True)
    parser.add_argument('--esp-write', type=pathlib.Path, required=True)
    parser.add_argument('--plan', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    if serial.__version__ != '3.5': raise ValueError('Require pyserial 3.5')
    repo = pathlib.Path(__file__).resolve().parents[2]
    destination = private_destination(args.output, repo)
    if sha256(args.manifest) != args.manifest_sha256: raise ValueError('Manifest hash mismatch')
    manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
    head = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip()
    if subprocess.check_output(['git', '-C', str(repo), 'status', '--porcelain'], text=True).strip():
        raise ValueError('Commit qualification sources before hardware control')
    source = manifest['source_commit']
    if validate_firmware_source(repo, source) != manifest['source_tree']:
        raise ValueError('Firmware source tree mismatch')
    lock_digest = hashlib.sha256(subprocess.check_output(
        ['git', '-C', str(repo), 'show', 'HEAD:dependencies.lock.json'])).hexdigest()
    if (lock_digest != manifest['lock_sha256'] or
            sha256(repo / 'dependencies.lock.json') != manifest['working_lock_sha256']):
        raise ValueError('Source/dependency identity mismatch')
    for item in manifest['artifacts']:
        if sha256(pathlib.Path(item['path'])) != item['sha256']: raise ValueError('Artifact changed')
    rp = json.loads(args.rp_load.read_text(encoding='utf-8'))
    esp = json.loads(args.esp_write.read_text(encoding='utf-8'))
    if (rp['board'] != args.board or esp['board'] != args.board or not rp['verified_load'] or
            rp['mode'] != 'sram' or not esp['exact_readback'] or rp['source_commit'] != source or
            esp['source_commit'] != source or rp['image_sha256'] != manifest['rp_sha256'] or
            esp['image_sha256'] != manifest['esp_sha256'] or
            sha256(args.esp_write.parent / 'readback.bin') != manifest['esp_sha256']):
        raise ValueError('Exact qualification deployment records required')
    identity = json.loads(args.identities.read_text(encoding='utf-8-sig'))[args.board]
    plan = validate_plan(json.loads(args.plan.read_text(encoding='utf-8')))
    destination.mkdir(parents=True)
    inventory = destination / 'inventory.json'
    subprocess.run(['pwsh', '-NoProfile', '-File', str(repo / 'tools/hardware/discover_windows.ps1'),
                    '-OutputPath', str(inventory)], check=True)
    validate_selected_cdc(json.loads(inventory.read_text(encoding='utf-8-sig'))['devices'], args.port, identity)
    total_bytes = 0
    records = []
    partial = bytearray()
    start = time.monotonic()
    result = {'board': args.board, 'source_commit': source, 'source_tree': manifest['source_tree'],
        'tool_commit': head, 'rp_load_sha256': sha256(args.rp_load), 'esp_write_sha256': sha256(args.esp_write),
        'manifest_sha256': args.manifest_sha256, 'plan_sha256': sha256(args.plan),
        'tool_sha256': sha256(pathlib.Path(__file__)), 'captured_at_utc': datetime.now(timezone.utc).isoformat(),
        'qualification_result': 'NOT ASSESSED', 'plan_completed': False}
    (destination / 'plan.json').write_text(json.dumps(plan, indent=2) + '\n', encoding='utf-8')
    try:
        with (destination / 'capture.jsonl').open('w', encoding='utf-8') as log:
            def record(value):
                value = {'host_elapsed': time.monotonic() - start, **value}
                log.write(json.dumps(value) + '\n'); log.flush(); records.append(value)
                if len(records) > 10000: raise ValueError('Capture record bound exceeded')
            identity_verified = False
            rom_requested = False
            @contextlib.contextmanager
            def shutdown_guard():
                try:
                    yield
                finally:
                    if identity_verified and not rom_requested:
                        try:
                            send('X STOP'); receive(.5)
                            result['stop_submitted'] = True
                        except Exception as error:
                            result['cleanup_error'] = str(error)
            with serial.Serial(args.port, 115200, timeout=.05, write_timeout=1) as port, shutdown_guard():
                port.dtr = True; port.rts = False
                def receive(duration, event=None):
                    nonlocal total_bytes
                    deadline = time.monotonic() + duration
                    while time.monotonic() < deadline:
                        data = port.read(4096); total_bytes += len(data); partial.extend(data)
                        if total_bytes > 4 * 1024 * 1024 or len(partial) > 8192:
                            raise ValueError('Bounded capture capacity exceeded')
                        while b'\n' in partial:
                            line, _, rest = partial.partition(b'\n'); partial[:] = rest
                            if not line.strip(): continue
                            response = json.loads(line)
                            if not isinstance(response, dict): raise ValueError('Invalid observer response')
                            record({'response': response})
                            if event and response.get('event') == event: return response
                    if event: raise ValueError('Expected response absent: ' + event)
                def send(command):
                    nonlocal rom_requested
                    if command == 'B': rom_requested = True
                    record({'command': command}); payload = (command + '\n').encode('ascii')
                    if port.write(payload) != len(payload): raise ValueError('Short control write')
                send('INFO')
                observed = receive(5, 'identity')
                if (observed.get('identity') != 'FNR-005 observer v1' or observed.get('source') != source or
                        observed.get('uid', '').lower() != identity['flash_id'].lower()):
                    raise ValueError('Observer/source/RP UID mismatch; no stimulus sent')
                # Qualification telemetry uses production CRC framing. Compare
                # its actual radio identity with the preserved deployment target.
                send('X ZERO'); receive(.1)  # explicit post-load/watchdog boundary
                send('X SNAP')
                radio = receive(5, 'esp')
                expected_mac = manifest['radio_mac']
                if radio.get('source') != source or radio.get('mac') != expected_mac:
                    raise ValueError('ESP source/MAC mismatch; no campaign stimulus sent')
                identity_verified = True
                for step in plan:
                    key, value = next(iter(step.items()))
                    record({'step': step})
                    if key == 'command': send(value)
                    elif key == 'wait': receive(value)
                    else: receive(10, value)
                result['plan_completed'] = True
    except Exception as error:
        result['capture_error'] = str(error)
        raise
    finally:
        result['bytes_captured'] = total_bytes
        result['capture_sha256'] = sha256(destination / 'capture.jsonl') if (destination / 'capture.jsonl').exists() else None
        (destination / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('Captured bounded FNR-005 plan; explicit assessment remains required')


if __name__ == '__main__':
    main()
