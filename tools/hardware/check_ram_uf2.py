"""Reject non-RP2040 or non-SRAM UF2 images before volatile diagnostics."""
import hashlib
import pathlib
import struct
import sys


def validate(data):
    if not data or len(data) % 512:
        raise ValueError('UF2 must contain complete 512-byte blocks')
    blocks = len(data) // 512
    seen = set()
    for offset in range(0, len(data), 512):
        magic0, magic1, flags, address, size, number, total, family = struct.unpack_from('<8I', data, offset)
        if (magic0, magic1, struct.unpack_from('<I', data, offset + 508)[0]) != (
                0x0a324655, 0x9e5d5157, 0x0ab16f30):
            raise ValueError('UF2 magic mismatch')
        if flags != 0x2000 or family != 0xe48bff56:
            raise ValueError('Expected ordinary RP2040 family-tagged UF2 blocks')
        if size != 256 or address % 256 or not 0x20000000 <= address < address + size <= 0x20042000:
            raise ValueError('UF2 includes a non-SRAM or malformed destination')
        if total != blocks or number >= blocks or number in seen:
            raise ValueError('UF2 block numbering mismatch')
        seen.add(number)
    return blocks


if __name__ == '__main__':
    content = pathlib.Path(sys.argv[1]).read_bytes()
    print(f'PASS: {validate(content)} RP2040 SRAM blocks; sha256={hashlib.sha256(content).hexdigest()}')
