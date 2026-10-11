"""Read-only ESP8285 backup through the FNR SRAM bridge on native Windows.

DTR remains asserted to disable TinyUSB's overwritable CDC TX FIFO. The bridge
does not route DTR/RTS to radio GPIOs. Do not use this helper with other bridges.
Raw images, MAC addresses and logs remain in a new private directory.
"""
import argparse
import contextlib
import json
import pathlib
import subprocess
from datetime import datetime, timezone

from rp2040_backup import private_destination, sha256


def validate_cdc(devices, port, identity):
    candidates = [d for d in devices if d['instance_id'].upper().startswith(
        'USB\\VID_2E8A&PID_000A&MI_00\\')]
    if len(candidates) != 1:
        raise ValueError('Require exactly one FNR diagnostic CDC interface')
    device = candidates[0]
    if not device['description'].endswith(f'({port})'):
        raise ValueError('Explicit COM port does not match the diagnostic interface')
    expected = identity['location_path'] + '#USBMI(0)'
    if expected not in device['properties'].get('DEVPKEY_Device_LocationPaths', []):
        raise ValueError('CDC topology does not match the selected board')


def main():
    import esptool
    import serial
    from esptool.targets.esp8266 import ESP8266ROM

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=['A', 'B'], required=True)
    parser.add_argument('--identities', type=pathlib.Path, required=True)
    parser.add_argument('--port', required=True)
    parser.add_argument('--expected-mac', required=True)
    parser.add_argument('--expected-flash-id', type=lambda s: int(s, 0), required=True)
    parser.add_argument('--expected-size', type=int, choices=[1048576], required=True)
    parser.add_argument('--bridge-sha256', required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    if esptool.__version__ != '4.8.1' or serial.__version__ != '3.5':
        raise ValueError('Require pinned esptool 4.8.1 and pyserial 3.5')
    if len(args.bridge_sha256) != 64 or any(c not in '0123456789abcdef' for c in args.bridge_sha256):
        raise ValueError('Record the validated loaded bridge SHA-256')
    repo = pathlib.Path(__file__).resolve().parents[2]
    destination = private_destination(args.output, repo)
    identity = json.loads(args.identities.read_text(encoding='utf-8-sig'))[args.board]
    destination.mkdir(parents=True)
    hashes = []
    metadata = []
    for n in (1, 2):
        inventory = destination / f'inventory-{n}.json'
        subprocess.run(['pwsh', '-NoProfile', '-File', str(repo / 'tools/hardware/discover_windows.ps1'),
                        '-OutputPath', str(inventory)], check=True)
        validate_cdc(json.loads(inventory.read_text(encoding='utf-8-sig'))['devices'], args.port, identity)
        with (destination / f'read-{n}.txt').open('w', encoding='utf-8') as log:
            with contextlib.redirect_stdout(log), contextlib.redirect_stderr(log):
                # Passing an already-open Serial object avoids esptool's Windows
                # constructor deasserting DTR. no_reset never toggles these lines.
                with serial.Serial(args.port, 115200, timeout=3) as port:
                    port.dtr = True
                    port.rts = False
                    rom = ESP8266ROM(port, 115200)
                    rom.connect(mode='no_reset')
                    mac = ':'.join(f'{b:02x}' for b in rom.read_mac())
                    chip = rom.get_chip_description()
                    if mac != args.expected_mac.lower() or chip != 'ESP8285N08':
                        raise ValueError('Radio silicon/MAC does not match selected board')
                    esp = rom.run_stub()  # Official esptool RAM stub, no flash write.
                    flash_id = esp.flash_id()
                    if flash_id != args.expected_flash_id:
                        raise ValueError('Radio flash ID mismatch')
                    if (flash_id >> 16) != 0x14:
                        raise ValueError('JEDEC capacity does not match the 1 MiB read')
                    print(f'chip={chip} mac={mac} flash_id={flash_id:#x} DTR=True')
                    data = esp.read_flash(0, args.expected_size)
                    # read_flash checks the stub's MD5 of the complete read.
                    if len(data) != args.expected_size:
                        raise ValueError('Incomplete flash read')
                    image = destination / f'original-{n}.bin'
                    image.write_bytes(data)
                    hashes.append(sha256(image))
                    port.send_break(0.1)
                    counters = port.read_until(b'\n', 512).decode('ascii', errors='replace')
                    print(counters)
                    metadata.append({'chip': chip, 'mac': mac, 'flash_id': flash_id,
                                     'bridge_counters': counters.strip()})
        print(f'Completed full independent read {n}; sha256={hashes[-1]}', flush=True)
    if hashes[0] != hashes[1]:
        raise ValueError('Independent radio reads disagree; do not write flash')
    result = {'schema_version': 1, 'board': args.board, 'captured_at_utc': datetime.now(timezone.utc).isoformat(),
              'source_commit': subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip(),
              'bridge_sha256': args.bridge_sha256, 'esptool': esptool.__version__, 'pyserial': serial.__version__,
              'size_bytes': args.expected_size, 'sha256': hashes[0], 'reads': metadata,
              'matching_independent_reads': True, 'restoration_tested': False}
    (destination / 'backup.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(f'PASS: two matching full radio reads for board {args.board}; restoration untested')


if __name__ == '__main__':
    main()
