"""Explicit RP2040 flash deployment/restoration with topology and backup guards.

Requires the selected target already in BOOTSEL and a privately recorded JEDEC
capacity. Never erases the whole chip independently of a selected image. The
restore path writes the exact preserved full image, including originally erased
bytes, and verifies the complete result before rebooting.
"""
import argparse
import json
import pathlib
import struct
import subprocess
from datetime import datetime, timezone

from rp2040_backup import private_destination, sha256, validate_target


def validate_backup(directory, board, identity, size):
    record = json.loads((directory / 'backup.json').read_text(encoding='utf-8-sig'))
    if (record['board'] != board or record['identity'] != identity
            or record['size_bytes'] != size or not record['matching_independent_reads']):
        raise ValueError('Backup identity/capacity mismatch')
    for n in (1, 2):
        path = directory / f'original-{n}.bin'
        if path.stat().st_size != size or sha256(path) != record['sha256']:
            raise ValueError('Require two unchanged complete original backups')
    return record


def flash_blocks(data, capacity):
    if not data or len(data) % 512:
        raise ValueError('Incomplete UF2')
    count = len(data) // 512
    numbers, destinations, segments = set(), set(), []
    for offset in range(0, len(data), 512):
        m0, m1, flags, address, size, number, total, family = struct.unpack_from('<8I', data, offset)
        if (m0, m1, struct.unpack_from('<I', data, offset + 508)[0]) != (
                0x0a324655, 0x9e5d5157, 0x0ab16f30):
            raise ValueError('UF2 magic mismatch')
        if flags != 0x2000 or family != 0xe48bff56:
            raise ValueError('Require ordinary RP2040 UF2 blocks')
        if size != 256 or address % 256 or not 0x10000000 <= address < address + size <= 0x10000000 + capacity:
            raise ValueError('UF2 destination exceeds measured flash')
        if total != count or number >= count or number in numbers or address in destinations:
            raise ValueError('Duplicate or inconsistent UF2 blocks')
        numbers.add(number); destinations.add(address)
        segments.append((address, data[offset + 32:offset + 32 + size]))
    if min(destinations) != 0x10000000:
        raise ValueError('Require reviewed bootable image starting at flash base')
    return segments


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=['A', 'B'], required=True)
    parser.add_argument('--identities', type=pathlib.Path, required=True)
    parser.add_argument('--backup', type=pathlib.Path, required=True)
    parser.add_argument('--geometry', type=pathlib.Path, required=True,
                        help='Private observation JSON: board, flash_id, jedec_hex, size_bytes')
    parser.add_argument('--picotool', type=pathlib.Path, required=True)
    parser.add_argument('--picotool-sha256', required=True)
    parser.add_argument('--image', type=pathlib.Path, required=True)
    parser.add_argument('--image-sha256', required=True)
    parser.add_argument('--mode', choices=['diagnostic', 'restore'], required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    repo = pathlib.Path(__file__).resolve().parents[2]
    destination = private_destination(args.output, repo)
    identity = json.loads(args.identities.read_text(encoding='utf-8-sig'))[args.board]
    geometry = json.loads(args.geometry.read_text(encoding='utf-8-sig'))
    size = geometry['size_bytes']
    jedec = bytes.fromhex(geometry['jedec_hex'])
    if (geometry['board'] != args.board or geometry['flash_id'].lower() != identity['flash_id'].lower()
            or len(jedec) != 3 or not 18 <= jedec[2] <= 24 or size != 1 << jedec[2]):
        raise ValueError('Require independently observed target flash geometry')
    backup = validate_backup(args.backup, args.board, identity, size)
    if sha256(args.picotool) != args.picotool_sha256 or sha256(args.image) != args.image_sha256:
        raise ValueError('Tool/image SHA-256 mismatch')
    image = args.image.read_bytes()
    restore = args.mode == 'restore'
    if restore:
        if len(image) != size or args.image_sha256 != backup['sha256']:
            raise ValueError('Restore requires exact full preserved image')
        segments = [(0x10000000, image)]
    else:
        segments = flash_blocks(image, size)
    destination.mkdir(parents=True)
    commands = []

    def run(arguments, name):
        inventory = destination / f'{name}-inventory.json'
        subprocess.run(['pwsh', '-NoProfile', '-File', str(repo / 'tools/hardware/discover_windows.ps1'),
                        '-OutputPath', str(inventory)], check=True)
        uid = validate_target(json.loads(inventory.read_text(encoding='utf-8-sig'))['devices'], identity)
        command = [str(args.picotool.resolve()), *arguments, '--ser', uid]
        result = subprocess.run(command, capture_output=True, text=True, timeout=300)
        (destination / f'{name}.txt').write_text(result.stdout + result.stderr, encoding='utf-8')
        commands.append({'command': command, 'returncode': result.returncode})
        if result.returncode:
            raise RuntimeError(f'{name} failed; inspect private log before continuing')

    options = ['-t', 'bin', '-o', '0x10000000'] if restore else ['-t', 'uf2']
    run(['load', '-v', str(args.image.resolve()), *options], 'write')
    readback = destination / 'readback.bin'
    end = max(address + len(data) for address, data in segments)
    run(['save', '-r', '0x10000000', hex(end), str(readback)], 'readback')
    contents = readback.read_bytes()
    if len(contents) != end - 0x10000000 or any(
            contents[address - 0x10000000:address - 0x10000000 + len(data)] != data
            for address, data in segments):
        raise ValueError('Selected image readback mismatch; target remains in BOOTSEL')
    run(['reboot', '-a'], 'boot')
    result = {'board': args.board, 'mode': args.mode,
              'captured_at_utc': datetime.now(timezone.utc).isoformat(),
              'source_commit': subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip(),
              'image_sha256': args.image_sha256, 'backup_sha256': backup['sha256'],
              'image_size_bytes': len(image), 'readback_size_bytes': len(contents),
              'readback_sha256': sha256(readback), 'exact_readback': True,
              'boot_observed': False, 'commands': commands}
    (destination / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('PASS: exact selected image readback; application boot needs separate observation')


if __name__ == '__main__':
    main()
