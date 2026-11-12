"""Download TRCustoms releases into the portable Custom folder."""
import argparse
import contextlib
import os
from pathlib import Path, PurePosixPath
import shutil
import sys
import tempfile
import traceback
import subprocess
import zipfile

from prepare_movies import convert

MAX_ARCHIVE = 2 * 1024**3
MAX_EXTRACTED = 4 * 1024**3
PICTURES = {'.pcx', '.bmp', '.png', '.jpg', '.jpeg', '.tga', '.webp'}
MOVIES = {'.rpl', '.bmv', '.avi', '.mp4', '.mpg', '.str'}


def download(file_id, destination):
    url = f'https://trcustoms.org/api/level_files/{file_id}/download/'
    # Use the Windows HTTPS client and its system certificate store.
    curl = Path(os.environ['SystemRoot'])/'System32/curl.exe'
    result = subprocess.run([str(curl), '--disable', '--fail', '--silent', '--show-error',
                             '--location', '--proto', '=https', '--proto-redir', '=https',
                             '--connect-timeout', '20', '--speed-limit', '1', '--speed-time', '60',
                             '--max-filesize', str(MAX_ARCHIVE), '--output', str(destination), url],
                            capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
    if result.returncode:
        raise ValueError(f'Download failed ({result.returncode}): {result.stderr.strip()}')


def empty_rpl(path):
    with path.open('rb') as movie:
        if movie.readline(16).rstrip(b'\r\n') != b'ARMovie':
            return False
        # RPL stores its header fields in a fixed order. A single-chunk movie
        # can also have a zero chunk index, so check the frame and size fields.
        fields = [movie.readline(4096).split(maxsplit=1) for _ in range(17)]
        if any(not fields[i] or fields[i][0] != b'0' for i in (12, 13, 14, 15)):
            return False
        if not fields[16] or not fields[16][0].isdigit():
            return False
        catalogue = int(fields[16][0])
        size = path.stat().st_size
        if not movie.tell() <= catalogue < size:
            return False
        movie.seek(catalogue)
        # The empty clip's catalogue points to EOF, with no media payload.
        offset, separator, _ = movie.readline(256).partition(b',')
        return separator == b',' and offset.isdigit() and int(offset) == size


def import_archive(archive, stage, log):
    levels = []
    names = set()
    total = 0
    with zipfile.ZipFile(archive) as package:
        for info in package.infolist():
            name = PurePosixPath(info.filename.replace('\\', '/'))
            print(repr(info.filename), info.file_size, file=log)
            ext = name.suffix.lower()
            if info.is_dir() or ext not in PICTURES | MOVIES | {'.phd', '.tub'}:
                continue
            if name.is_absolute() or any(part == '..' or Path(part).is_reserved() or
                    part.rstrip(' .') != part or any(ch in '<>:"|?*' or ord(ch)<32 for ch in part)
                    for part in name.parts):
                raise ValueError(f'Unsafe archive path: {name}')
            key = str(name).casefold()
            if key in names:
                raise ValueError(f'Duplicate asset path: {name}')
            names.add(key)
            total += info.file_size
            if total > MAX_EXTRACTED:
                raise ValueError('Assets exceed the 4 GiB extraction limit')
            dest = stage/'Original'/name
            dest.parent.mkdir(exist_ok=True, parents=True)
            with package.open(info) as source, dest.open('xb') as out:
                shutil.copyfileobj(source, out)
            if ext in {'.phd', '.tub'} and name.stem.upper() not in {'TITLE','CUT1','CUT2','CUT3','CUT4'}:
                levels.append(dest)
    if len(levels) != 1:
        raise ValueError(f'Expected one playable DOS level; found {len(levels)}. Campaign order needs review.')
    level = levels[0]
    with level.open('rb') as f:
        if f.read(4) != b'\x20\x00\x00\x00':
            raise ValueError(f'Unsupported level header: {level.name}')
    # Runtime aliases are separate from the original named assets.
    shutil.copyfile(level, stage/'level.phd')
    title = level.parent/'TITLEH.PCX'
    if title.is_file():
        shutil.copyfile(title, stage/'title.pcx')
    movie_names = set()
    for movie in (stage/'Original').rglob('*'):
        if movie.is_file() and movie.suffix.lower() == '.rpl':
            target = stage/'FMV'/(movie.stem.upper()+'.bmv')
            if target.name in movie_names:
                raise ValueError(f'Ambiguous movie name: {movie.name}')
            movie_names.add(target.name)
            if empty_rpl(movie):
                print(f'Skipping empty movie {movie.relative_to(stage)}', file=log, flush=True)
                continue
            print(f'Converting {movie.relative_to(stage)}', file=log, flush=True)
            convert(movie, target)


def run(root, entry, file_id, uninstall=False):
    base = root/'Custom'/'TR1'/str(entry)
    if uninstall:
        # Windows cannot delete import.log while this process has it open.
        if not isinstance(entry, int) or entry <= 0 or base.resolve() != root.resolve()/'Custom'/'TR1'/str(entry):
            raise ValueError('Invalid custom entry directory')
        try:
            if base.exists():
                shutil.rmtree(base)
            return 0
        except OSError:
            with (base/'import.log').open('a', encoding='utf-8') as log:
                traceback.print_exc(file=log)
            return 1
    base.mkdir(parents=True, exist_ok=True)
    installed = base/'Game'
    with (base/'import.log').open('a', encoding='utf-8') as log, contextlib.redirect_stdout(log):
        try:
            print(f'Entry {entry}, release file {file_id}', file=log, flush=True)
            if installed.exists():
                return 0
            archive = base/f'release-{file_id}.archive'
            with tempfile.TemporaryDirectory(dir=base) as tmp:
                temp = Path(tmp)
                if not archive.exists():
                    download(file_id, temp/'download')
                    os.replace(temp/'download', archive)
                stage = temp/'Game'
                stage.mkdir()
                import_archive(archive, stage, log)
                stage.rename(installed)
            print('Installed', file=log)
            return 0
        except Exception:
            traceback.print_exc(file=log)
            return 1


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('entry', type=int)
    parser.add_argument('file', type=int)
    parser.add_argument('--uninstall', action='store_true')
    args = parser.parse_args()
    if min(args.entry, args.file) <= 0:
        parser.error('Expected positive catalogue IDs')
    sys.exit(run(Path.cwd(), args.entry, args.file, args.uninstall))
