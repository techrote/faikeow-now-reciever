"""Load a validated SRAM-only RP2040 image into one explicitly mapped ROM target."""
import argparse
import json
import pathlib
import subprocess
from datetime import datetime, timezone
from check_ram_uf2 import validate
from rp2040_backup import private_destination, sha256, validate_target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=['A', 'B'], required=True)
    parser.add_argument('--identities', type=pathlib.Path, required=True)
    parser.add_argument('--picotool', type=pathlib.Path, required=True)
    parser.add_argument('--picotool-sha256', required=True)
    parser.add_argument('--image', type=pathlib.Path, required=True)
    parser.add_argument('--image-sha256', required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    repo = pathlib.Path(__file__).resolve().parents[2]
    destination = private_destination(args.output, repo)
    if sha256(args.image) != args.image_sha256 or sha256(args.picotool) != args.picotool_sha256:
        raise ValueError('Image/tool SHA-256 mismatch')
    blocks = validate(args.image.read_bytes())
    identity = json.loads(args.identities.read_text(encoding='utf-8-sig'))[args.board]
    destination.mkdir(parents=True)
    inventory = destination / 'inventory.json'
    subprocess.run(['pwsh', '-NoProfile', '-File', str(repo / 'tools/hardware/discover_windows.ps1'),
                    '-OutputPath', str(inventory)], check=True)
    uid = validate_target(json.loads(inventory.read_text(encoding='utf-8-sig'))['devices'], identity)
    command = [str(args.picotool.resolve()), 'load', '-v', '-x', str(args.image.resolve()), '--ser', uid]
    completed = subprocess.run(command, capture_output=True, text=True, timeout=120)
    (destination / 'load.txt').write_text(completed.stdout + completed.stderr, encoding='utf-8')
    if completed.returncode:
        raise ValueError('SRAM load failed; see private log')
    result = {'board': args.board, 'captured_at_utc': datetime.now(timezone.utc).isoformat(),
              'mode': 'sram', 'verified_load': True,
              'source_commit': subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip(),
              'image_sha256': args.image_sha256, 'blocks': blocks, 'command': command, 'result': 'PASS'}
    (destination / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('PASS: verified SRAM image loaded into the mapped target')


if __name__ == '__main__':
    main()
