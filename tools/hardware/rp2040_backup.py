"""Read-only, topology-guarded RP2040 backup for native Windows.

Require one BOOTSEL target because these clones share a USB serial string.
Never infer A/B from COM number, volume letter or libusb address.
"""
import argparse
import hashlib
import json
import pathlib
import subprocess
from datetime import datetime, timezone


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def validate_target(devices, identity):
    targets = [d for d in devices if d['instance_id'].upper().startswith(
        'USB\\VID_2E8A&PID_0003\\')]
    if len(targets) != 1:
        raise ValueError('Connect exactly one RP2040 BOOTSEL target; shared serials are unsafe.')
    target = targets[0]
    paths = target['properties'].get('DEVPKEY_Device_LocationPaths', [])
    if identity['location_path'] not in paths:
        raise ValueError('Physical USB topology does not match the selected board.')
    if target['instance_id'].upper() != identity['instance_id'].upper():
        raise ValueError('USB identity does not match the selected board.')
    # picotool 2.2.0 intentionally interprets RP2040 --ser as flash unique ID,
    # not the nonunique USB BOOTSEL serial (picoboot_connection.c).
    flash_id = identity['flash_id']
    if len(flash_id) != 16 or any(c not in '0123456789abcdefABCDEF' for c in flash_id):
        raise ValueError('Expected an independently observed 64-bit flash ID.')
    return flash_id


def private_destination(destination, repo):
    destination = destination.resolve()
    if destination == repo or repo in destination.parents:
        raise ValueError('Factory dumps and raw logs must stay outside the repository.')
    if destination.exists():
        raise ValueError('Refusing to overwrite an existing backup directory.')
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=['A', 'B'], required=True)
    parser.add_argument('--identities', type=pathlib.Path, required=True,
                        help='Private JSON mapping A/B to instance_id, location_path, flash_id')
    parser.add_argument('--picotool', type=pathlib.Path, required=True)
    parser.add_argument('--picotool-sha256', required=True)
    parser.add_argument('--expected-size', type=int, required=True,
                        help='Previously observed full flash size in bytes')
    parser.add_argument('--explicit-range', action='store_true',
                        help='Use measured capacity as an address range when erased flash defeats save -a')
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    repo = pathlib.Path(__file__).resolve().parents[2]
    destination = private_destination(args.output, repo)
    tool = args.picotool.resolve()
    if sha256(tool) != args.picotool_sha256.lower():
        raise ValueError('picotool executable hash mismatch')
    if not 256 * 1024 <= args.expected_size <= 16 * 1024 * 1024:
        raise ValueError('Implausible RP2040 addressable flash capacity')
    identity = json.loads(args.identities.read_text(encoding='utf-8-sig'))[args.board]
    destination.mkdir(parents=True)
    inventory = destination / 'inventory.json'
    subprocess.run(['pwsh', '-NoProfile', '-File', str(repo / 'tools/hardware/discover_windows.ps1'),
                    '-OutputPath', str(inventory)], check=True)
    serial = validate_target(json.loads(inventory.read_text(encoding='utf-8-sig'))['devices'], identity)
    records = []

    def run(arguments, name):
        # Recheck topology immediately before every hardware command.
        fresh = destination / f'{name}-inventory.json'
        subprocess.run(['pwsh', '-NoProfile', '-File', str(repo / 'tools/hardware/discover_windows.ps1'),
                        '-OutputPath', str(fresh)], check=True)
        validate_target(json.loads(fresh.read_text(encoding='utf-8-sig'))['devices'], identity)
        command = [str(tool), *arguments]
        result = subprocess.run(command, capture_output=True, text=True, timeout=300)
        (destination / f'{name}.txt').write_text(result.stdout + result.stderr, encoding='utf-8')
        records.append({'command': command, 'returncode': result.returncode})
        if result.returncode:
            raise RuntimeError(f'{name} failed; private log retained; no flash was written')

    run(['info', '-a', '--ser', serial], 'info')
    images = [destination / f'original-{n}.bin' for n in (1, 2)]
    for index, image in enumerate(images, 1):
        selection = (['-r', '0x10000000', hex(0x10000000 + args.expected_size)]
                     if args.explicit_range else ['-a'])
        run(['save', *selection, '-v', str(image), '--ser', serial], f'read-{index}')
        if image.stat().st_size != args.expected_size:
            raise ValueError('Backup size differs from observed flash capacity')
    hashes = [sha256(image) for image in images]
    if hashes[0] != hashes[1]:
        raise ValueError('Independent flash reads disagree; do not flash this board')
    manifest = {'schema_version': 1, 'board': args.board,
                'captured_at_utc': datetime.now(timezone.utc).isoformat(),
                'identity': identity, 'picotool_sha256': sha256(tool),
                'source_commit': subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip(),
                'commands': records, 'size_bytes': args.expected_size,
                'explicit_range': args.explicit_range,
                'sha256': hashes[0], 'matching_independent_reads': True,
                'restoration_tested': False}
    (destination / 'backup.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'PASS: two matching full reads for board {args.board}; restoration remains untested')


if __name__ == '__main__':
    main()
