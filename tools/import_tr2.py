"""Import the reference TR2 PC CD without modifying its media or an existing import.

Usage: python tools/import_tr2.py --cue disc.cue --exe Tomb2.exe --output Source/TR2/PC
Original RPL movies are retained; playback caches can be prepared separately.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import wave

from setup_import import Image, parse_cue_tracks

AUTHORITY_SHA256 = 'fd9c91d14d54540b3e00c9bedafeef9e4ddb1e9542adc9f29170c7f19b1a657b'
REQUIRED = {'DATA/TITLE.TR2', 'DATA/TITLE.PCX', 'DATA/TOMBPC.DAT', 'DATA/MAIN.SFX'}


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def executable_tables(exe):
    raw = Path(exe).read_bytes()
    if hashlib.sha256(raw).hexdigest() != AUTHORITY_SHA256:
        raise ValueError('Unsupported authority executable revision.')
    pe = struct.unpack_from('<I', raw, 60)[0]
    count = struct.unpack_from('<H', raw, pe + 6)[0]
    opt_size = struct.unpack_from('<H', raw, pe + 20)[0]
    base = struct.unpack_from('<I', raw, pe + 24 + 28)[0]
    result = {}
    for name, address, length in [('sine.bin', 0x468ddc, 2050), ('atan.bin', 0x467db8, 4130)]:
        for i in range(count):
            at = pe + 24 + opt_size + i * 40
            _, rva, size, offset = struct.unpack_from('<4I', raw, at + 8)
            delta = address - base - rva
            if 0 <= delta and delta + length <= size and offset + delta + length <= len(raw):
                result[name] = raw[offset + delta:offset + delta + length]
                break
        else:
            raise ValueError(f'Unmapped executable table: {name}')
    return result


def verify(root):
    root = Path(root)
    manifest = json.loads((root/'import-manifest.json').read_text())
    if manifest.get('game') != 'TR2' or manifest.get('format') != 1:
        raise ValueError('Not a supported TR2 import manifest.')
    for entry in manifest['files']:
        path = (root/entry['path']).resolve()
        if not path.is_relative_to(root.resolve()) or not path.is_file():
            raise ValueError(f"Missing or unsafe imported file: {entry['path']}")
        if path.stat().st_size != entry['size'] or digest(path) != entry['sha256']:
            raise ValueError(f"Imported file failed verification: {entry['path']}")
    return manifest


def import_assets(cue, exe, output):
    cue, exe, output = Path(cue).resolve(), Path(exe).resolve(), Path(output).resolve()
    # Never replace a user's existing asset tree, including on partial failure.
    if output.exists():
        raise ValueError('Destination already exists; verify it or choose a new destination.')
    tables = executable_tables(exe)
    tracks = parse_cue_tracks(cue, 61, ('MODE1/2352',))
    inputs = [cue, exe] + sorted({t['file'] for t in tracks})
    sources = [{'name': p.name, 'size': p.stat().st_size, 'sha256': digest(p)} for p in inputs]
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.tr2-import-', dir=output.parent) as temporary:
        stage = Path(temporary)/'PC'
        stage.mkdir()
        records = []

        def record(path, kind, **extra):
            records.append(dict(path=path.relative_to(stage).as_posix(), kind=kind,
                                size=path.stat().st_size, sha256=digest(path), **extra))

        first = tracks[0]
        disc = Image(first['file'], first['width'], first['begin'])
        # Bound ISO directory/file reads to the data track, not the entire BIN.
        disc.length = first['end']
        try:
            entries = list(disc.walk(*disc.root))
            names = set()
            for entry in entries:
                name = entry['path'].upper()
                if not (name.startswith(('DATA/', 'FMV/')) or name in ('MAIN.SFX', 'TOMBPC.DAT')):
                    continue
                if '/' not in name:
                    name = 'DATA/' + name
                if name in names:
                    raise ValueError(f'Duplicate asset: {name}')
                names.add(name)
                path = stage/name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(disc.read(entry['sector'], entry['size']))
                record(path, 'original', disc_path=entry['path'], sector=entry['sector'])
            if not REQUIRED <= names or not any(n.startswith('FMV/') and n.endswith('.RPL') for n in names):
                raise ValueError('Required TR2 menu data or movies missing from disc.')
        finally:
            disc.file.close()
        for track in tracks[1:]:
            path = stage/'AUDIO'/f"track{track['number']:02}.wav"
            path.parent.mkdir(exist_ok=True)
            with track['file'].open('rb') as source, wave.open(str(path), 'wb') as dest:
                dest.setparams((2, 2, 44100, 0, 'NONE', 'not compressed'))
                source.seek(track['begin'])
                remaining = track['end'] - track['begin']
                while remaining:
                    chunk = source.read(min(remaining, 1024*1024))
                    if not chunk:
                        raise ValueError('Truncated audio track.')
                    dest.writeframesraw(chunk)
                    remaining -= len(chunk)
            record(path, 'cd-audio', track=track['number'], begin=track['begin'], end=track['end'])
        for name, content in tables.items():
            path = stage/name
            path.write_bytes(content)
            record(path, 'executable-table', authority_sha256=AUTHORITY_SHA256)
        manifest = dict(game='TR2', edition='PC', format=1, converter=1,
                        sources=sources, files=records)
        (stage/'import-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
        verify(stage)
        # Catch changed input media before publishing the staged directory.
        for path, source in zip(inputs, sources):
            if digest(path) != source['sha256']:
                raise ValueError('Input media changed during import.')
        stage.rename(output)
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cue', type=Path)
    parser.add_argument('--exe', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    if not args.verify and (not args.cue or not args.exe):
        parser.error('--cue and --exe are required for importing')
    result = verify(args.output) if args.verify else import_assets(args.cue, args.exe, args.output)
    print(f"Verified {len(result['files'])} TR2 assets in {args.output}")
