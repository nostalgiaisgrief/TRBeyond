"""Prepare the supplied Unfinished Business campaign for the setup payload."""
import argparse
import hashlib
import json
import shutil
import stat
from pathlib import Path
from convert_assets import convert_level

LEVELS = ('EGYPT', 'CAT', 'END', 'END2')
PICTURES = ('TITLEH', 'CRED0')


def copy_asset(source, destination):
    if destination.exists(): destination.chmod(stat.S_IREAD | stat.S_IWRITE)
    shutil.copyfile(source, destination)


def prepare(source, destination):
    source, destination = Path(source), Path(destination)
    originals = [f'{name}.TUB' for name in LEVELS] + [f'{name}.PCX' for name in PICTURES]
    for name in originals:
        if not (source/name).is_file():
            raise ValueError(f'Missing Unfinished Business file: {name}')
    for folder in ('DATA', 'Original/DATA', 'SAVES', 'AUDIO', 'FMV'):
        (destination/folder).mkdir(parents=True, exist_ok=True)
    for name in originals:
        copy_asset(source/name, destination/'Original/DATA'/name)
    for name in LEVELS:
        convert_level(source/f'{name}.TUB', destination/'DATA'/f'{name}.TBL')
    for name in PICTURES:
        copy_asset(source/f'{name}.PCX', destination/'DATA'/f'{name}.PCX')
    files = []
    paths = [f'Original/DATA/{name}' for name in originals]
    paths += [f'DATA/{name}.TBL' for name in LEVELS] + [f'DATA/{name}.PCX' for name in PICTURES]
    for relative in paths:
        file = destination/relative
        files.append(dict(path=relative, bytes=file.stat().st_size,
                          sha256=hashlib.sha256(file.read_bytes()).hexdigest()))
    (destination/'import-manifest.json').write_text(json.dumps(dict(
        format=2, asset_format=1, edition='UB', files=files), indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    prepare(args.source, args.destination)
