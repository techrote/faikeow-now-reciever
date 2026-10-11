"""Bounded UART bursts, deliberate frame damage and independent MCU recovery.

Uses the already-qualified SRAM bridge and radio diagnostic. No flash write or
unknown GPIO. Radio reset is an application command; RP reset is the bridge's
documented 1200-baud ROM transition, followed by a guarded SRAM reload.
"""
import argparse
import json
import pathlib
import struct
import subprocess
import sys
import time
import zlib
from datetime import datetime, timezone
from radio_backup import validate_cdc
from rp2040_backup import private_destination, sha256


def payload(sequence, stream):
    n = (1, 32, 128, 250)[sequence & 3]
    value = 0x002f4a91 ^ sequence ^ (0xa583174d if stream else 0x7139e2b5)
    result = bytearray()
    for _ in range(n):
        value ^= (value << 13) & 0xffffffff
        value ^= value >> 17
        value ^= (value << 5) & 0xffffffff
        result.append(value & 255)
    return bytes(result)


def packet(kind, sequence=0, data=b''):
    raw = bytes([kind]) + struct.pack('<IH', sequence, len(data)) + data
    raw += struct.pack('<I', zlib.crc32(raw))
    return b'\xc0' + raw.replace(b'\xdb', b'\xdb\xdd').replace(b'\xc0', b'\xdb\xdc') + b'\xc0'


class Decoder:
    def __init__(self):
        self.active = self.escaped = self.discard = False
        self.raw = bytearray()
        self.bad_frames = 0

    def feed(self, data):
        frames = []
        for byte in data:
            if byte == 0xc0:
                raw = self.raw
                if self.active and raw and not self.discard:
                    n = struct.unpack_from('<H', raw, 5)[0] if len(raw) >= 7 else 251
                    if (not self.escaped and n <= 250 and len(raw) == n + 11
                            and zlib.crc32(raw[:-4]) == struct.unpack_from('<I', raw, len(raw) - 4)[0]):
                        frames.append((raw[0], struct.unpack_from('<I', raw, 1)[0], bytes(raw[7:7 + n])))
                    else:
                        self.bad_frames += 1
                self.raw = bytearray(); self.active = True; self.escaped = self.discard = False
            elif self.active and not self.discard:
                if self.escaped:
                    self.escaped = False
                    if byte == 0xdc: byte = 0xc0
                    elif byte == 0xdd: byte = 0xdb
                    else: self.discard = True; self.bad_frames += 1; continue
                elif byte == 0xdb:
                    self.escaped = True; continue
                if len(self.raw) >= 261:
                    self.discard = True; self.bad_frames += 1
                else:
                    self.raw.append(byte)
        return frames


def main():
    import serial
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=['A', 'B'], required=True)
    parser.add_argument('--identities', type=pathlib.Path, required=True)
    parser.add_argument('--port', required=True)
    parser.add_argument('--expected-radio-chip-id', type=lambda s: int(s, 0), required=True)
    parser.add_argument('--radio-write-result', type=pathlib.Path, required=True)
    parser.add_argument('--bridge', type=pathlib.Path, required=True)
    parser.add_argument('--bridge-sha256', required=True)
    parser.add_argument('--picotool', type=pathlib.Path, required=True)
    parser.add_argument('--picotool-sha256', required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    if serial.__version__ != '3.5':
        raise ValueError('Require pinned pyserial 3.5')
    repo = pathlib.Path(__file__).resolve().parents[2]
    destination = private_destination(args.output, repo)
    write = json.loads(args.radio_write_result.read_text())
    if (write['board'] != args.board or write['mode'] != 'diagnostic' or not write['exact_readback']
            or sha256(args.radio_write_result.parent / 'readback.bin') != write['image_sha256']):
        raise ValueError('Require exact radio diagnostic deployment on this board')
    identity = json.loads(args.identities.read_text(encoding='utf-8-sig'))[args.board]
    destination.mkdir(parents=True)
    cases = []; serial_port = None; frames = []; decoder = Decoder(); capture = []
    log = (destination / 'capture.jsonl').open('w', encoding='utf-8')
    beginning = time.monotonic()

    def record(**value):
        log.write(json.dumps({'elapsed': time.monotonic() - beginning, **value}) + '\n'); log.flush()

    def open_port(name):
        nonlocal serial_port, decoder, frames
        inventory = destination / (name + '-inventory.json')
        subprocess.run(['pwsh', '-NoProfile', '-File', str(repo / 'tools/hardware/discover_windows.ps1'),
                        '-OutputPath', str(inventory)], check=True)
        validate_cdc(json.loads(inventory.read_text(encoding='utf-8-sig'))['devices'], args.port, identity)
        serial_port = serial.Serial(args.port, 115200, timeout=.1)
        serial_port.dtr = True; serial_port.rts = False
        decoder = Decoder(); frames = []

    def send(data, description):
        record(command=description, tx_hex=data.hex())
        serial_port.write(data)

    def read_until(kind, sequence=None, timeout=5):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            for i, frame in enumerate(frames):
                if frame[0] == kind and (sequence is None or frame[1] == sequence):
                    return frames.pop(i)
            data = serial_port.read(1024)
            if data:
                record(rx_hex=data.hex())
                new = decoder.feed(data); capture.extend(new); frames.extend(new)
                if len(capture) > 20000 or len(frames) > 10000:
                    raise ValueError('Bounded capture capacity exceeded')
        raise TimeoutError('Expected diagnostic frame missing')

    def info():
        send(packet(5), 'DIAG_INFO')
        frame = read_until(5)
        if frame[1] != args.expected_radio_chip_id or frame[2] != b'FNR-002 ESP link diagnostic v1':
            raise ValueError('Live radio identity mismatch')

    def start():
        nonlocal frames, capture, decoder
        frames = []; capture = []; decoder = Decoder()
        send(packet(3), 'DIAG_START'); frame = read_until(5)
        if frame[1] != args.expected_radio_chip_id or frame[2] != b'FNR-002 ESP link diagnostic v1':
            raise ValueError('Radio START identity mismatch')

    def finish(count, errors=(0, 0, 0)):
        send(packet(4), 'DIAG_STOP')
        values = struct.unpack('<9I', read_until(6)[2])
        if values[:3] != (count, 0, 0) or values[4:6] != (0, 0) or values[6:] != errors:
            raise ValueError('Radio statistics disagree: ' + str(values))
        echoes = [f for f in capture if f[0] == 129]
        streams = [f for f in capture if f[0] == 2]
        if len(echoes) != count or [f[1] for f in echoes] != list(range(count)):
            raise ValueError('Missing/duplicate/out-of-order echoes')
        if decoder.bad_frames or any(f[2] != payload(f[1], f[0] == 2) for f in echoes + streams):
            raise ValueError('Host CRC/payload integrity error')
        return {'radio_stats': list(values), 'echoes': len(echoes), 'streams_observed': len(streams),
                'host_bad_frames': decoder.bad_frames}

    def burst(name):
        start(); send(b''.join(packet(1, seq, payload(seq, False)) for seq in range(20)), '20-frame contiguous burst')
        read_until(129, 19)
        cases.append({'case': name, 'result': 'PASS', **finish(20)})

    try:
        open_port('initial'); info(); burst('bursty_bidirectional')
        start()
        bad = bytearray(packet(1, 0, payload(0, False))); bad[-2] ^= 1
        # bad[-2] is an ordinary CRC byte for this fixed seed/frame, not escape.
        send(bytes(bad) + b'\xc0\x01\x00\xc0' + b'\xc0' + b'\x01' * 262 + b'\xc0'
             + packet(1, 0, payload(0, False)), 'CRC damage + interrupted frame + bounded overflow + valid frame')
        read_until(129, 0)
        cases.append({'case': 'deliberate_damage_and_resynchronization', 'result': 'PASS', **finish(1, (1, 1, 1))})
        start(); read_until(2)
        send(packet(7), 'radio application reset while stream/sender active')
        for seq in range(10):
            send(packet(1, seq, payload(seq, False)), 'intentional request during radio reset')
            time.sleep(.05)
        frames[:] = [f for f in frames if f[0] != 5]
        rebooted = read_until(5)
        if rebooted[1] != args.expected_radio_chip_id:
            raise ValueError('Radio reset identity mismatch')
        cases.append({'case': 'radio_application_reset', 'result': 'PASS', 'requests_attempted_during_reset': 10,
                      'loss_during_reset': 'intentional; not part of integrity qualification'})
        # Retire all UART bytes from the reset before starting a clean phase.
        time.sleep(.1); serial_port.reset_input_buffer()
        burst('recovery_after_radio_reset')
        start(); first = read_until(2)
        record(command='RP2040 1200-baud ROM transition while radio streams')
        try:
            serial_port.baudrate = 1200
        except serial.SerialException:
            pass
        serial_port.close(); serial_port = None
        time.sleep(1)
        subprocess.run([sys.executable, str(repo / 'tools/hardware/load_ram.py'), '--board', args.board,
                        '--identities', str(args.identities), '--picotool', str(args.picotool),
                        '--picotool-sha256', args.picotool_sha256, '--image', str(args.bridge),
                        '--image-sha256', args.bridge_sha256, '--output', str(destination / 'rp-reload')], check=True)
        time.sleep(2); open_port('after-rp-reset'); info(); resumed = read_until(2)
        send(packet(4), 'stop stream after RP restart'); stats = struct.unpack('<9I', read_until(6)[2])
        if resumed[1] <= first[1] or resumed[2] != payload(resumed[1], True):
            raise ValueError('Radio stream did not survive RP restart')
        cases.append({'case': 'rp_rom_restart_while_radio_streams', 'result': 'PASS',
                      'stream_sequence_before': first[1], 'stream_sequence_after': resumed[1],
                      'radio_stats': list(stats), 'frames_lost_during_rp_rom': 'intentional/unobserved'})
        time.sleep(.1); serial_port.reset_input_buffer(); burst('recovery_after_rp_restart')
        print('PASS: UART burst, deliberate damage/resync and independent reset/recovery cases', flush=True)
    finally:
        if serial_port:
            serial_port.close()
        log.close()
        result = {'board': args.board, 'captured_at_utc': datetime.now(timezone.utc).isoformat(),
                  'source_commit': subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip(),
                  'bridge_sha256': args.bridge_sha256, 'radio_image_sha256': write['image_sha256'],
                  'baud': 115200, 'cases': cases, 'result': 'PASS' if len(cases) == 6 else 'FAIL'}
        (destination / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
