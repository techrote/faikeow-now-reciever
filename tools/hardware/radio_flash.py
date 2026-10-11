"""Explicitly selected, backed-up ESP8285 write and exact readback.

Requires the FNR SRAM bridge (DTR/RTS never drive the radio) already running,
and the radio already in ROM/stub mode. Does not reset any unknown GPIO.
No erase-all operation exists. Writes only the image's occupied sectors.
"""
import argparse
import contextlib
import json
import pathlib
import subprocess
from datetime import datetime, timezone

from radio_backup import validate_cdc
from rp2040_backup import private_destination, sha256


def validate_backup(directory, board, mac):
    metadata = json.loads((directory / 'backup.json').read_text())
    if (metadata['board'] != board or not metadata['matching_independent_reads']
            or metadata['size_bytes'] != 1048576):
        raise ValueError('Require two matching complete backups for the selected board')
    for n in (1, 2):
        image = directory / f'original-{n}.bin'
        if image.stat().st_size != 1048576 or sha256(image) != metadata['sha256']:
            raise ValueError('Preserved backup is missing, incomplete or changed')
    if len(metadata['reads']) != 2 or any(r['mac'] != mac for r in metadata['reads']):
        raise ValueError('Backup radio identity differs from the selected target')
    return metadata


def validate_image(path, digest, restore, backup):
    if sha256(path) != digest or not 0 < path.stat().st_size <= 1048576:
        raise ValueError('Image hash/length mismatch')
    if restore:
        if digest != backup['sha256'] or path.stat().st_size != 1048576:
            raise ValueError('Restoration requires the exact complete preserved image')
    else:
        from esptool.bin_image import LoadFirmwareImage
        image = LoadFirmwareImage('esp8266', str(path))
        image.verify()
        if image.checksum != image.calculate_checksum():
            raise ValueError('ESP image checksum mismatch')
        header = path.read_bytes()[:4]
        if header[0] != 0xe9 or header[2] != 3 or header[3] != 0x20:
            raise ValueError('Require reviewed ESP8285 DOUT, 1 MiB, 40 MHz image')


def main():
    import serial
    import esptool
    from esptool.targets.esp8266 import ESP8266ROM
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=['A', 'B'], required=True)
    parser.add_argument('--identities', type=pathlib.Path, required=True)
    parser.add_argument('--port', required=True)
    parser.add_argument('--expected-mac', required=True)
    parser.add_argument('--backup', type=pathlib.Path, required=True)
    parser.add_argument('--image', type=pathlib.Path, required=True)
    parser.add_argument('--image-sha256', required=True)
    parser.add_argument('--mode', choices=['diagnostic', 'restore'], required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    if esptool.__version__ != '4.8.1' or serial.__version__ != '3.5':
        raise ValueError('Require esptool 4.8.1 and pyserial 3.5')
    repo = pathlib.Path(__file__).resolve().parents[2]
    destination = private_destination(args.output, repo)
    mac = args.expected_mac.lower()
    backup = validate_backup(args.backup.resolve(), args.board, mac)
    validate_image(args.image, args.image_sha256, args.mode == 'restore', backup)
    identity = json.loads(args.identities.read_text(encoding='utf-8-sig'))[args.board]
    destination.mkdir(parents=True)
    inventory = destination / 'inventory.json'
    subprocess.run(['pwsh', '-NoProfile', '-File', str(repo / 'tools/hardware/discover_windows.ps1'),
                    '-OutputPath', str(inventory)], check=True)
    validate_cdc(json.loads(inventory.read_text(encoding='utf-8-sig'))['devices'], args.port, identity)
    command = ['--chip', 'esp8266', '--baud', '115200', '--before', 'no_reset',
               '--after', 'no_reset_stub', 'write_flash', '--flash_mode', 'keep',
               '--flash_size', 'keep', '--flash_freq', 'keep', '0x0', str(args.image.resolve())]
    with (destination / 'write-readback.txt').open('w', encoding='utf-8') as log:
        with contextlib.redirect_stdout(log), contextlib.redirect_stderr(log):
            with serial.Serial(args.port, 115200, timeout=3) as port:
                port.dtr = True; port.rts = False
                rom = ESP8266ROM(port, 115200)
                rom.connect(mode='no_reset')
                if (':'.join(f'{b:02x}' for b in rom.read_mac()) != mac
                        or rom.get_chip_description() != 'ESP8285N08'):
                    raise ValueError('Radio identity mismatch; no write performed')
                esp = rom.run_stub()
                if esp.flash_id() != 0x14325e:
                    raise ValueError('Radio flash identity/capacity mismatch; no write performed')
                # ESP8266StubLoader does not inherit sync_stub_detected from the
                # ROM object. main() calls run_stub() again, so confirm the live
                # resident stub by SYNC before passing that object to main().
                # Otherwise 4.8.1 tries to upload over its own resident RAM stub.
                esp.sync()
                if not esp.sync_stub_detected:
                    raise ValueError('Resident RAM stub was not confirmed; no write performed')
                # A supplied connection keeps DTR asserted; every header field is
                # "keep" so the image and restored dump are compared byte-for-byte.
                esptool.main(command, esp=esp)
                readback = esp.read_flash(0, args.image.stat().st_size)
                readback_file = destination / 'readback.bin'
                readback_file.write_bytes(readback)
                if readback != args.image.read_bytes():
                    raise ValueError('Flash readback differs from the selected image; stay in ROM')
                print('PASS: exact full image readback; starting user code')
                esp.soft_reset(False)
    result = {'board': args.board, 'mode': args.mode, 'captured_at_utc': datetime.now(timezone.utc).isoformat(),
              'source_commit': subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip(),
              'image_sha256': args.image_sha256, 'size_bytes': args.image.stat().st_size,
              'backup_sha256': backup['sha256'], 'header_changes': False,
              'exact_readback': True, 'boot_observed': False, 'command': command}
    (destination / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('PASS: exact readback; separately observe application boot')


if __name__ == '__main__':
    main()
