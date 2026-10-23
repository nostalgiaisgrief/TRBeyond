"""DOS -> versioned Beyond levels. No executable extraction or asset downloads.

The v1 container holds independently addressed canonical sections, not a PHD
file. Integer records remain lossless for the DOS parity milestone. Future
texture/record encodings require an explicit supported schema, never quantizing
another source into the DOS palette. See analysis/BEYOND-ASSET-FORMAT.MD.
"""
import argparse
import hashlib
import struct
from pathlib import Path

MAGIC = b'TRBLVL\0\0'
VERSION = 1
TAGS = ('TEX8', 'ROOM', 'FLOR', 'MESH', 'ANIM', 'MODL', 'TEXI', 'WRLD', 'PALT', 'CINE', 'SFXB')
LIMIT = 256 * 1024 * 1024


def checksum(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & 0xffffffff
    return h


class Reader:
    def __init__(self, data):
        self.data, self.at = data, 0

    def take(self, count, width=1):
        if count < 0 or width <= 0 or count > (len(self.data)-self.at)//width:
            raise ValueError('Truncated or oversized level section')
        start = self.at
        self.at += count * width
        return self.data[start:self.at]

    def number(self, width=4):
        return int.from_bytes(self.take(width), 'little')

    def array(self, width, count_width=4):
        count = self.number(count_width)
        self.take(count, width)
        return count


def dos_sections(data):
    if not 8 <= len(data) <= LIMIT:
        raise ValueError('Invalid level length')
    r = Reader(data)
    if r.number() != 32:
        raise ValueError('Expected DOS PHD version 32')
    sections = {}

    def section(tag, start):
        sections[tag] = data[start:r.at]

    start = r.at; r.array(65536); section('TEX8', start)
    r.take(4)  # Original unused field, not part of the Beyond representation.
    start = r.at
    rooms = r.number(2)
    if not 0 < rooms <= 32767:
        raise ValueError('Invalid room count')
    for _ in range(rooms):
        r.take(16); r.array(2); r.array(32, 2)
        nz, nx = r.number(2), r.number(2)
        if nx < 3 or nz < 3:
            raise ValueError('Invalid room grid')
        r.take(nz*nx, 8); r.take(2)
        r.array(18, 2); r.array(18, 2); r.take(4)
    section('ROOM', start)
    start = r.at; r.array(2); section('FLOR', start)
    start = r.at; r.array(2); r.array(4); section('MESH', start)
    start = r.at
    for width in (32, 6, 8, 2): r.array(width)
    section('ANIM', start)
    start = r.at
    for width in (4, 2, 18, 32): r.array(width)
    section('MODL', start)
    start = r.at
    for width in (20, 16, 8): r.array(width)
    section('TEXI', start)
    start = r.at
    r.array(16); r.array(16)
    boxes = r.array(20)
    r.array(2); r.take(boxes, 12); r.array(2); r.array(22)
    section('WRLD', start)
    start = r.at; r.take(8192+768); section('PALT', start)
    start = r.at; r.array(16, 2); r.array(1, 2); section('CINE', start)
    start = r.at; r.take(256, 2); r.array(8); r.array(1); r.array(4); section('SFXB', start)
    if r.at != len(data):
        raise ValueError('Unexpected trailing level data')
    return sections


def encode_sections(data, sections, edition=1):
    offset = 64 + len(sections)*24
    directory, payload = bytearray(), bytearray()
    for tag, block in sections.items():
        directory += struct.pack('<4s5I', tag.encode('ascii'), 1, offset, len(block), checksum(block), 1)
        payload += block
        offset += len(block)
    return struct.pack('<8s6I32s', MAGIC, VERSION, 0, edition, len(sections), offset, 0,
                       hashlib.sha256(data).digest()) + directory + payload


def encode(data):
    return encode_sections(data, dos_sections(data))


def decode(data):
    if not 64 <= len(data) <= LIMIT:
        raise ValueError('Invalid Beyond level length')
    magic, major, minor, edition, count, length, flags, digest = struct.unpack_from('<8s6I32s', data)
    if (magic, major, minor, length, flags) != (MAGIC, 1, 0, len(data), 0) or edition not in (1, 2) or not 11 <= count <= 64:
        raise ValueError('Unsupported Beyond level header')
    end = 64 + count*24
    if end > len(data): raise ValueError('Truncated directory')
    spans, sections, total = [], {}, end
    for i in range(count):
        tag, schema, offset, size, crc, required = struct.unpack_from('<4s5I', data, 64+i*24)
        tag = tag.decode('ascii')
        if required not in (0, 1) or not size or offset < end or offset+size > len(data):
            raise ValueError('Invalid section extent')
        if any(offset < b and a < offset+size for a, b in spans): raise ValueError('Overlapping sections')
        block = data[offset:offset+size]
        if checksum(block) != crc: raise ValueError('Corrupted section')
        spans.append((offset, offset+size)); total += size
        if tag in TAGS or tag == 'TEXC':
            if schema != 1 or tag in sections: raise ValueError('Invalid section schema or duplicate')
            sections[tag] = block
        elif required: raise ValueError('Unsupported required section')
    expected=set(TAGS) | ({'TEXC'} if edition == 2 else set())
    if total != len(data) or set(sections) != expected: raise ValueError('Incomplete section directory')
    return sections


def convert_level(source, destination):
    source, destination = Path(source), Path(destination)
    data = source.read_bytes()
    encoded = encode(data)
    if decode(encoded) != dos_sections(data): raise ValueError('Conversion verification failed')
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix+'.tmp')
    try:
        temporary.write_bytes(encoded)
        if temporary.read_bytes() != encoded: raise OSError('Conversion write verification failed')
        temporary.replace(destination)
    finally:
        if temporary.exists(): temporary.unlink()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='Original PHD file or DATA directory')
    parser.add_argument('destination', type=Path, help='Output .TBL file or DATA directory')
    args = parser.parse_args()
    if args.source.is_dir():
        files = sorted(args.source.glob('*.PHD'))
        if not files: parser.error('No PHD levels found')
        for file in files:
            convert_level(file, args.destination/(file.stem+'.TBL'))
            print(file.name, '->', file.stem+'.TBL')
    else:
        convert_level(args.source, args.destination)
