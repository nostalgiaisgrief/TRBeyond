"""PS1 CUE/BIN import. Raw XA movie sectors retain interleaved audio/video."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import tempfile
import wave
from convert_ps1 import convert_level
from ps1_picture import convert as convert_picture
from prepare_movies import convert as convert_movie

MOVIES=('CAFE','CANYON','CORELOGO','END','LIFT','MANSION','PRISON','PYRAMID','SNOW','VISION')
PICTURES={'AMERTIT':'TITLEH','END':'END','CRED':'CRED1'}

def inspect(selected):
    from setup_import import cue_tracks,Image,LEVELS
    tracks=cue_tracks(Path(selected).resolve(),True);track=tracks[0]
    disc=Image(track['file'],2352,track['begin'],2)
    try:
        entries={e['path'].upper():e for e in disc.walk(*disc.root)}
        required=[f'PSXDATA/{name}.PSX' for name in LEVELS]+[f'FMV/{name}.FMV' for name in MOVIES]+[f'DELDATA/{name}.RAW' for name in PICTURES]
        if any(name not in entries for name in required):raise ValueError('The disc is missing required PS1 game data.')
        for name in required:
            e=entries[name]
            if track['begin']+(e['sector']+(e['size']+2047)//2048)*2352>track['end']:raise ValueError('File extends beyond PS1 data track')
        return dict(kind='ps1',tracks=tracks,entries={name:entries[name] for name in required})
    finally:disc.file.close()

def install(selected,destination,payload,progress,cancel):
    from setup_import import Image,LEVELS,RUNTIME_FILES,check_cancel,generated_sine,install_bundled_campaign
    destination,payload=Path(destination).resolve(),Path(payload).resolve()
    if destination.exists() and (not destination.is_dir() or any(destination.iterdir())):raise ValueError('Choose a new or empty installation folder.')
    plan=inspect(selected);destination.parent.mkdir(parents=True,exist_ok=True)
    if shutil.disk_usage(destination.parent).free<10*1024**3:raise ValueError('At least 10 GB of free space is required for PS1 data and movie conversion.')
    stage=Path(tempfile.mkdtemp(prefix='.trb-setup-',dir=destination.parent)).resolve()
    if stage.parent!=destination.parent or not stage.name.startswith('.trb-setup-'):raise ValueError('Invalid staging directory')
    committed=False
    try:
        source=stage/'Source/PS1'
        for edition in ('DOS','PS1','TRR'):
            for folder in ('DATA','FMV','AUDIO','SAVES'):(stage/'Source'/edition/folder).mkdir(parents=True)
        for folder in ('PSXDATA','DELDATA','FMV','AUDIO'):(source/'Original'/folder).mkdir(parents=True)
        (stage/'Custom').mkdir()
        for name in RUNTIME_FILES:shutil.copy2(payload/name,stage/name)
        install_bundled_campaign(stage,payload)
        (stage/'sine.bin').write_bytes(generated_sine())
        track=plan['tracks'][0];disc=Image(track['file'],2352,track['begin'],2)
        try:
            for index,(name,entry) in enumerate(plan['entries'].items()):
                check_cancel(cancel);progress(5+15*index/len(plan['entries']),f'Extracting {name}')
                target=source/'Original'/name
                if name.startswith('FMV/'):
                    disc.file.seek(track['begin']+entry['sector']*2352);left=((entry['size']+2047)//2048)*2352
                    with target.open('wb') as out:
                        while left:
                            check_cancel(cancel);block=disc.file.read(min(left,2352*256))
                            if not block:raise ValueError('Truncated PS1 movie')
                            out.write(block);left-=len(block)
                else:target.write_bytes(disc.read(entry['sector'],entry['size']))
        finally:disc.file.close()
        for index,name in enumerate(LEVELS):
            check_cancel(cancel);progress(20+20*index/len(LEVELS),f'Converting PS1 level {name}')
            convert_level(source/'Original/PSXDATA'/f'{name}.PSX',source/'DATA'/f'{name}.TBL')
        for name,target in PICTURES.items():convert_picture(source/'Original/DELDATA'/f'{name}.RAW',source/'DATA'/f'{target}.tbi')
        for index,track in enumerate(plan['tracks'][1:]):
            check_cancel(cancel);progress(40+10*index/56,f'Preparing PS1 audio {track["number"]:02}')
            target=source/'AUDIO'/f'cd{track["number"]:02}.wav'
            with track['file'].open('rb') as f,wave.open(str(target),'wb') as out:
                out.setparams((2,2,44100,0,'NONE','not compressed'));f.seek(track['begin']);left=track['end']-track['begin']
                while left:
                    check_cancel(cancel);block=f.read(min(left,1024*1024))
                    if not block:raise ValueError('Truncated PS1 audio')
                    out.writeframesraw(block);left-=len(block)
            shutil.copy2(target,source/'Original/AUDIO'/target.name)
        for index,name in enumerate(MOVIES):
            check_cancel(cancel);progress(50+40*index/len(MOVIES),f'Converting PS1 movie {name}')
            convert_movie(source/'Original/FMV'/f'{name}.FMV',source/'FMV'/f'{name}.bmv',cancel=cancel)
        records=[];progress(92,'Verifying PS1 assets')
        for file in sorted(source.rglob('*')):
            if file.is_file():
                check_cancel(cancel)
                with file.open('rb') as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
                records.append(dict(path=file.relative_to(source).as_posix(),bytes=file.stat().st_size,sha256=digest))
        (source/'import-manifest.json').write_text(json.dumps(dict(format=2,asset_format=1,converter=1,edition='PS1',input_kind='cue',files=records),indent=2))
        (stage/'trb-install.json').write_text(json.dumps(dict(format=2,asset_format=1,portable=True,sources=['PS1'])))
        (stage/'settings.ini').write_text('[Game]\nsource=PS1\n')
        check_cancel(cancel)
        if destination.exists():destination.rmdir()
        os.rename(stage,destination);committed=True;progress(100,'Installation complete');return destination
    finally:
        if not committed and stage.exists() and stage.parent==destination.parent and stage.name.startswith('.trb-setup-'):shutil.rmtree(stage)
