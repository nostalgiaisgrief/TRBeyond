"""Read-only DOS media import and staged portable installation."""
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import tempfile
import wave

from inspect_reference import Disc
from prepare_movies import convert
from convert_assets import convert_level, VERSION as ASSET_VERSION

LEVELS = ['GYM', 'TITLE', 'CUT1', 'CUT2', 'CUT3', 'CUT4', 'LEVEL1', 'LEVEL2',
          'LEVEL3A', 'LEVEL3B', 'LEVEL4', 'LEVEL5', 'LEVEL6', 'LEVEL7A', 'LEVEL7B',
          'LEVEL8A', 'LEVEL8B', 'LEVEL8C', 'LEVEL10A', 'LEVEL10B', 'LEVEL10C']
MOVIES = ['CAFE', 'CANYON', 'CORE', 'END', 'ESCAPE', 'LIFT', 'MANSION',
          'PRISON', 'PYRAMID', 'SNOW', 'VISION']
PICTURES = ['TITLEH', 'END', 'CRED1', 'CRED2', 'CRED3']
RUNTIME_FILES = ['TombRaiderBeyond.exe', 'openxr_loader.dll', 'OPENXR-LICENSE.txt']


class Cancelled(Exception):
    pass


def check_cancel(cancel):
    if cancel and cancel.is_set():
        raise Cancelled('Installation cancelled. No installation was committed.')


class Image(Disc):
    def __init__(self, path, sector_size=2352, offset=0, mode=1):
        self.file = Path(path).open('rb')
        self.sector_size, self.offset, self.mode = sector_size, offset, mode
        self.length = os.fstat(self.file.fileno()).st_size
        try:
            pvd = self.read(16, 2048)
            if pvd[:7] != b'\x01CD001\x01':
                raise ValueError('The selected image does not contain a supported PC ISO 9660 disc.')
            self.root = struct.unpack_from('<I', pvd, 158)[0], struct.unpack_from('<I', pvd, 166)[0]
        except Exception:
            self.file.close()
            raise

    def read(self, sector, size):
        if sector < 0 or size < 0 or size > self.length:
            raise ValueError('Invalid disc extent.')
        result = bytearray()
        while len(result) < size:
            at = self.offset + sector * self.sector_size
            if at + self.sector_size > self.length:
                raise ValueError('Truncated disc image.')
            self.file.seek(at)
            raw = self.file.read(self.sector_size)
            if self.sector_size == 2352:
                if len(raw) != 2352 or raw[:12] != b'\x00' + b'\xff' * 10 + b'\x00' or raw[15] != self.mode:
                    raise ValueError(f'Expected a Mode {self.mode} data sector.')
                if self.mode == 2 and (raw[16:20] != raw[20:24] or raw[18]&32):
                    raise ValueError('Expected a Mode 2 Form 1 file sector.')
                start=24 if self.mode==2 else 16
                raw = raw[start:start+2048]
            result.extend(raw)
            sector += 1
        return bytes(result[:size])

    def walk(self, sector, size, parent='', seen=None):
        seen = set() if seen is None else seen
        if sector in seen or len(seen) > 4096 or parent.count('/') > 32:
            raise ValueError('Invalid directory cycle or excessive nesting in image.')
        seen.add(sector)
        data = self.read(sector, size)
        pos = 0
        while pos < len(data):
            length = data[pos]
            if not length:
                pos = (pos // 2048 + 1) * 2048
                continue
            record = data[pos:pos + length]
            if len(record) < 34 or len(record) != length or 33 + record[32] > length:
                raise ValueError('Truncated directory record.')
            pos += length
            name = record[33:33 + record[32]]
            if name in (b'\0', b'\1'):
                continue
            name = name.decode('ascii').split(';')[0]
            if any(c in name for c in '/\\:') or name in ('.', '..'):
                raise ValueError('Unsafe image filename.')
            path = f'{parent}/{name}'.lstrip('/')
            extent, count = struct.unpack_from('<I', record, 2)[0], struct.unpack_from('<I', record, 10)[0]
            if record[25] & 2:
                yield from self.walk(extent, count, path, seen)
            else:
                yield dict(path=path, sector=extent, size=count)


def cue_tracks(cue, ps1=False):
    tracks, binary, current = [], None, None
    for line in cue.read_text(encoding='utf-8-sig').splitlines():
        line = line.strip()
        if not line or line.upper().startswith('REM '):
            continue
        match = re.fullmatch(r'FILE\s+"(.+)"\s+BINARY', line, re.I)
        if match:
            binary = (cue.parent / match[1]).resolve()
            if not binary.is_file():
                raise ValueError(f'Missing CUE companion: {match[1]}')
            current = None
        elif line.upper().startswith('FILE '):
            raise ValueError('Only BINARY CUE tracks are supported.')
        elif match := re.fullmatch(r'TRACK\s+(\d+)\s+(\S+)', line, re.I):
            if binary is None:
                raise ValueError('CUE track has no FILE.')
            current = dict(number=int(match[1]), kind=match[2].upper(), file=binary, indexes={})
            tracks.append(current)
        elif match := re.fullmatch(r'INDEX\s+(\d+)\s+(\d+):(\d+):(\d+)', line, re.I):
            if current is None or int(match[3]) >= 60 or int(match[4]) >= 75:
                raise ValueError('Invalid CUE index.')
            current['indexes'][int(match[1])] = (int(match[2])*60+int(match[3]))*75+int(match[4])
        elif line.upper().startswith(('PREGAP ', 'POSTGAP ', 'FLAGS ', 'TITLE ', 'PERFORMER ', 'CATALOG ')):
            continue
        else:
            raise ValueError(f'Unsupported CUE directive: {line}')
    if [t['number'] for t in tracks] != list(range(1, 58 if ps1 else 11)):
        raise ValueError('Unexpected Tomb Raider disc track layout.')
    if tracks[0]['kind'] not in (('MODE2/2352',) if ps1 else ('MODE1/2352', 'MODE1/2048')):
        raise ValueError('The disc data-track type does not match the selected source.')
    for i, track in enumerate(tracks):
        if 1 not in track['indexes'] or (i and track['kind'] != 'AUDIO'):
            raise ValueError('Missing track index or invalid audio track.')
        width = 2048 if track['kind'] == 'MODE1/2048' else 2352
        begin = track['indexes'][1] * width
        end = track['file'].stat().st_size
        if i + 1 < len(tracks) and tracks[i+1]['file'] == track['file']:
            nxt = tracks[i+1]['indexes']
            end = nxt.get(0, nxt.get(1, -1)) * width
        if not 0 <= begin < end <= track['file'].stat().st_size or (end-begin) % width:
            raise ValueError('Invalid or truncated CUE track extent.')
        track.update(begin=begin, end=end, width=width)
    return tracks


def inspect_source(kind, selected):
    if kind == 'ps1':
        from import_ps1 import inspect
        return inspect(selected)
    selected = Path(selected).resolve()
    tracks, music = [], []
    if kind == 'cue':
        tracks = cue_tracks(selected)
        t = tracks[0]
        image, width, offset = t['file'], t['width'], t['begin']
    elif kind == 'gog':
        folder = selected.parent if selected.is_file() else selected
        files = {p.name.lower(): p for p in folder.iterdir() if p.is_file()}
        image = files.get('game.gog')
        if not image:
            raise ValueError('Choose the installation folder containing GAME.GOG.')
        for n in range(2, 11):
            path = next((files.get(f'{n:02}{ext}') for ext in ('.mp3', '.ogg', '.wav') if f'{n:02}{ext}' in files), None)
            if not path:
                raise ValueError(f'Missing music track {n:02}.mp3 (or .ogg/.wav) beside GAME.GOG.')
            music.append(path)
        with image.open('rb') as f:
            f.seek(16*2352+16)
            width = 2352 if f.read(7) == b'\x01CD001\x01' else 2048
        offset = 0
    else:
        raise ValueError('Unsupported source selection.')
    disc = Image(image, width, offset)
    try:
        entries = {e['path'].upper(): e for e in disc.walk(*disc.root)}
        required = [f'DATA/{n}.PHD' for n in LEVELS] + [f'DATA/{n}.PCX' for n in PICTURES] + [f'FMV/{n}.RPL' for n in MOVIES]
        missing = [n for n in required if n not in entries]
        if missing:
            raise ValueError('Missing required PC assets: ' + ', '.join(missing))
        for name in LEVELS:
            e = entries[f'DATA/{name}.PHD']
            if disc.read(e['sector'], 4) != struct.pack('<I', 32):
                raise ValueError(f'{name} is not a supported PC level.')
        chosen = {n: entries[n] for n in required}
    finally:
        disc.file.close()
    return dict(kind=kind, image=image, width=width, offset=offset, entries=chosen, tracks=tracks, music=music)


def decode_audio(source, destination, cancel=None):
    import av
    with av.open(str(source)) as container, wave.open(str(destination), 'wb') as output:
        output.setparams((2, 2, 44100, 0, 'NONE', 'not compressed'))
        resampler = av.AudioResampler(format='s16', layout='stereo', rate=44100)
        frames = 0
        for frame in container.decode(audio=0):
            check_cancel(cancel)
            for pcm in resampler.resample(frame):
                output.writeframesraw(bytes(pcm.planes[0])[:pcm.samples*4]); frames += pcm.samples
        for pcm in resampler.resample(None):
            output.writeframesraw(bytes(pcm.planes[0])[:pcm.samples*4]); frames += pcm.samples
        if not frames:
            raise ValueError(f'No audio samples in {source.name}.')


def generated_sine():
    return struct.pack('<1025h', *(round(16384*math.sin(i*math.pi/2048)) for i in range(1025)))


def inspect_installation(selected):
    destination = Path(selected).expanduser().resolve()
    if not destination.is_dir():
        raise ValueError('Choose an existing Tomb Raider Beyond installation folder.')
    marker = destination / 'trb-install.json'
    recognized = any((destination / name).is_file() for name in ('TombRaiderBeyond.exe', 'Tomb Raider Beyond.exe'))
    if marker.is_file():
        try:
            info = json.loads(marker.read_text())
            recognized = recognized or (isinstance(info, dict) and info.get('format') in (1, 2) and info.get('portable') is True)
        except (ValueError, UnicodeError):
            pass
    if not recognized:
        raise ValueError('This folder is not recognized as a Tomb Raider Beyond installation.')
    return destination


def update_installation(selected, payload, progress=lambda value, text: None, cancel=None):
    """Replace only owned runtime files; roll back replacements on failure."""
    destination = inspect_installation(selected)
    marker = destination/'trb-install.json'
    try:
        info = json.loads(marker.read_text())
    except (OSError, ValueError) as error:
        raise ValueError('This asset-format milestone requires a fresh installation.') from error
    if info.get('asset_format') != ASSET_VERSION:
        raise ValueError('This asset-format milestone requires a fresh installation.')
    payload = Path(payload).resolve()
    check_cancel(cancel)
    for name in RUNTIME_FILES:
        if not (payload / name).is_file():
            raise ValueError(f'Missing installer runtime file: {name}')
    stage = Path(tempfile.mkdtemp(prefix='.trb-update-', dir=destination.parent)).resolve()
    if stage.parent != destination.parent or not stage.name.startswith('.trb-update-'):
        raise ValueError('Invalid update staging directory.')
    replacements, replaced, keep_backup = RUNTIME_FILES + ['sine.bin'], [], False
    # Keep existing shortcuts working without leaving an obsolete game binary.
    legacy_name = 'Tomb Raider Beyond.exe'
    if (destination / legacy_name).exists():
        replacements.append(legacy_name)
    try:
        (stage / 'new').mkdir()
        (stage / 'backup').mkdir()
        progress(10, 'Preparing runtime update')
        for name in RUNTIME_FILES:
            check_cancel(cancel)
            shutil.copy2(payload / name, stage / 'new' / name)
        (stage / 'new' / 'sine.bin').write_bytes(generated_sine())
        if legacy_name in replacements:
            shutil.copy2(payload / 'TombRaiderBeyond.exe', stage / 'new' / legacy_name)
        for name in replacements:
            current = destination / name
            if current.is_symlink() or (current.exists() and not current.is_file()):
                raise ValueError(f'Cannot replace runtime file: {name}')
            if current.exists():
                shutil.copy2(current, stage / 'backup' / name)
        try:
            for index, name in enumerate(replacements):
                check_cancel(cancel)
                progress(30 + index*15, f'Updating {name}')
                os.replace(stage / 'new' / name, destination / name)
                replaced.append(name)
            check_cancel(cancel)
        except Exception as error:
            rollback_errors = []
            for name in reversed(replaced):
                try:
                    backup = stage / 'backup' / name
                    if backup.exists():
                        os.replace(backup, destination / name)
                    else:
                        (destination / name).unlink()
                except OSError as rollback_error:
                    rollback_errors.append(str(rollback_error))
            if rollback_errors:
                keep_backup = True
                raise RuntimeError(f'Update could not be rolled back completely. Runtime backups are in {stage / "backup"}. Close the game before retrying.') from error
            if isinstance(error, PermissionError):
                raise ValueError('Cannot replace runtime files. Close Tomb Raider Beyond and try again; also check folder permissions. The previous runtime was restored.') from error
            raise
        progress(100, 'Update complete')
        return destination
    finally:
        if not keep_backup and stage.exists():
            if stage.parent == destination.parent and stage.name.startswith('.trb-update-'):
                shutil.rmtree(stage)


def install(kind, selected, destination, payload, progress=lambda value, text: None, cancel=None):
    if kind == 'ps1':
        from import_ps1 import install as install_ps1
        return install_ps1(selected, destination, payload, progress, cancel)
    if kind == 'update':
        return update_installation(selected, payload, progress, cancel)
    destination, payload = Path(destination).resolve(), Path(payload).resolve()
    if destination.exists() and (not destination.is_dir() or any(destination.iterdir())):
        raise ValueError('Choose a new or empty installation folder. Existing installations and saves will not be overwritten.')
    plan = inspect_source(kind, selected)
    check_cancel(cancel)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if shutil.disk_usage(destination.parent).free < 2*1024**3:
        raise ValueError('At least 2 GB of free space is required for extraction and movie conversion.')
    stage = Path(tempfile.mkdtemp(prefix='.trb-setup-', dir=destination.parent)).resolve()
    if stage.parent != destination.parent or not stage.name.startswith('.trb-setup-'):
        raise ValueError('Invalid staging directory.')
    committed = False
    try:
        source = stage/'Source'/'DOS'
        for edition in ['DOS', 'PS1', 'TRR']:
            for folder in ['DATA', 'FMV', 'AUDIO', 'SAVES', 'Original/DATA', 'Original/FMV', 'Original/AUDIO']:
                (stage/'Source'/edition/folder).mkdir(parents=True, exist_ok=True)
        (stage/'Custom').mkdir()
        for name in RUNTIME_FILES:
            shutil.copy2(payload/name, stage/name)
        (stage/'sine.bin').write_bytes(generated_sine())
        disc = Image(plan['image'], plan['width'], plan['offset'])
        try:
            for n, (name, entry) in enumerate(plan['entries'].items()):
                check_cancel(cancel)
                progress(5 + 20*n/len(plan['entries']), f'Extracting {name}')
                (source/'Original'/name).write_bytes(disc.read(entry['sector'], entry['size']))
        finally:
            disc.file.close()
        for index, name in enumerate(LEVELS):
            check_cancel(cancel)
            progress(25 + 5*index/len(LEVELS), f'Converting level {name}')
            convert_level(source/'Original'/'DATA'/f'{name}.PHD', source/'DATA'/f'{name}.TBL')
        for name in PICTURES:
            shutil.copy2(source/'Original'/'DATA'/f'{name}.PCX', source/'DATA'/f'{name}.PCX')
        for n in range(2, 11):
            check_cancel(cancel); progress(30 + (n-2)*2, f'Preparing music track {n:02}')
            out = source/'AUDIO'/f'cd{n:02}.wav'
            if kind == 'gog':
                original = source/'Original'/'AUDIO'/plan['music'][n-2].name
                shutil.copy2(plan['music'][n-2], original)
                decode_audio(original, out, cancel)
            else:
                track = plan['tracks'][n-1]
                with track['file'].open('rb') as f, wave.open(str(out), 'wb') as wav:
                    wav.setparams((2, 2, 44100, 0, 'NONE', 'not compressed')); f.seek(track['begin'])
                    left = track['end']-track['begin']
                    while left:
                        check_cancel(cancel); data = f.read(min(left, 1024*1024))
                        if not data: raise ValueError('Truncated audio track.')
                        wav.writeframesraw(data); left -= len(data)
            if kind == 'cue':
                shutil.copy2(out, source/'Original'/'AUDIO'/out.name)
        for n, name in enumerate(MOVIES):
            check_cancel(cancel); progress(48+n*4, f'Converting movie {name} ({n+1}/{len(MOVIES)})')
            convert(source/'Original'/'FMV'/f'{name}.RPL', source/'FMV'/f'{name}.bmv', cancel=cancel)
        check_cancel(cancel); progress(94, 'Verifying imported files')
        records = []
        for p in sorted(source.rglob('*')):
            if p.is_file():
                check_cancel(cancel)
                with p.open('rb') as f: digest = hashlib.file_digest(f, 'sha256').hexdigest()
                records.append(dict(path=p.relative_to(source).as_posix(), bytes=p.stat().st_size, sha256=digest))
        (source/'import-manifest.json').write_text(json.dumps(dict(edition='DOS', input_kind=kind, format=2, asset_format=ASSET_VERSION, converter=1, files=records), indent=2))
        (stage/'trb-install.json').write_text(json.dumps(dict(format=2, portable=True, asset_format=ASSET_VERSION, sources=['DOS'])))
        (stage/'settings.ini').write_text('[Game]\nsource=DOS\n')
        check_cancel(cancel)
        if destination.exists():
            destination.rmdir()  # Empty only; never remove an existing installation.
        os.rename(stage, destination)
        committed = True
        progress(100, 'Installation complete')
        return destination
    finally:
        if not committed and stage.exists():
            # Only this invocation's verified private staging tree is removed.
            if stage.parent == destination.parent and stage.name.startswith('.trb-setup-'):
                shutil.rmtree(stage)
