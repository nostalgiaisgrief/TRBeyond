"""Inventory an original raw Mode 1 CD and executable; no third-party code.

Run with Python 3. Reads source files only. Extracted assets remain private.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


class Disc:
    def __init__(self, path):
        self.file = path.open('rb')
        pvd = self.read(16, 2048)
        if pvd[:7] != b'\x01CD001\x01':
            raise ValueError('Expected ISO 9660 primary volume in raw Mode 1 track')
        self.volume = pvd[40:72].decode('ascii').strip()
        self.root = (u32(pvd, 158), u32(pvd, 166))

    def read(self, sector, size):
        result = bytearray()
        while len(result) < size:
            self.file.seek(sector * 2352)
            raw = self.file.read(2352)
            if len(raw) != 2352 or raw[15] != 1:
                raise ValueError(f'Invalid Mode 1 sector: {sector}')
            result.extend(raw[16:2064])
            sector += 1
        return bytes(result[:size])

    def walk(self, sector, size, parent=''):
        data = self.read(sector, size)
        pos = 0
        while pos < len(data):
            length = data[pos]
            if not length:
                pos = (pos // 2048 + 1) * 2048
                continue
            record = data[pos:pos + length]
            if len(record) < 34:
                raise ValueError('Truncated directory entry')
            name = record[33:33 + record[32]]
            pos += length
            if name in (b'\x00', b'\x01'):
                continue
            name = name.decode('ascii').split(';')[0]
            if '/' in name or '\\' in name or name in ('.', '..'):
                raise ValueError('Unsafe directory name')
            path = f'{parent}/{name}'.lstrip('/')
            extent, count = u32(record, 2), u32(record, 10)
            if record[25] & 2:
                yield from self.walk(extent, count, path)
            else:
                yield {'path': path, 'sector': extent, 'size': count}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--install', type=Path, required=True)
    parser.add_argument('--bin', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--assets', type=Path, required=True)
    args = parser.parse_args()
    exe = (args.install / 'TOMB.EXE').read_bytes()
    le = u32(exe, 60)
    if exe[:2] != b'MZ' or exe[le:le + 2] != b'LE':
        raise ValueError('Expected MZ/LE executable')
    objects = []
    table = le + u32(exe, le + 0x40)
    for i in range(u32(exe, le + 0x44)):
        size, base, flags, first_page, pages, reserved = struct.unpack_from('<6I', exe, table + i * 24)
        objects.append(dict(number=i + 1, virtual_size=size, base=base,
                            flags=flags, first_page=first_page, pages=pages))
    disc = Disc(args.bin)
    files = list(disc.walk(*disc.root))
    extracted = []
    args.assets.mkdir(parents=True, exist_ok=True)
    for entry in files:
        if entry['path'].upper() in ('DATA/GYM.PHD', 'DATA/LEVEL1.PHD', 'DATA/TITLE.PHD'):
            content = disc.read(entry['sector'], entry['size'])
            dest = args.assets / entry['path']
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(content)
            extracted.append(dict(**entry, sha256=hashlib.sha256(content).hexdigest(),
                                  first_u32=u32(content, 0)))
    report = dict(install=str(args.install), cd_image=str(args.bin),
                  exe_sha256=hashlib.sha256(exe).hexdigest(), exe_size=len(exe),
                  le_offset=le, entry_object=u32(exe, le + 0x18),
                  entry_offset=u32(exe, le + 0x1c), objects=objects,
                  cd_volume=disc.volume, files=files, extracted=extracted)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'files'}, indent=2))
    print(f'CD contains {len(files)} files; inventory saved to {args.out}')


if __name__ == '__main__':
    main()
