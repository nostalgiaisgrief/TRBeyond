"""Prepare movie caches named by the supplied TR2 gameflow."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from prepare_movies import convert


def startup_movies(root):
    data=(root/'DATA/TOMBPC.DAT').read_bytes()
    if len(data)<390 or struct.unpack_from('<I',data)[0]!=3:
        raise ValueError('Invalid TR2 gameflow')
    header=data[262:390]
    levels,pictures,titles,movies,cuts=struct.unpack_from('<5H',header,64)
    pos=390
    names=[]
    for kind,count in enumerate([levels,pictures,titles,movies,levels,cuts]):
        offsets=struct.unpack_from('<'+'H'*count,data,pos);pos+=count*2
        size=struct.unpack_from('<H',data,pos)[0];pos+=2
        if size>len(data)-pos:raise ValueError('Truncated string table')
        strings=bytes(c^header[120] for c in data[pos:pos+size]);pos+=size
        if kind==3:
            for offset in offsets:
                name=strings[offset:].split(b'\0')[0].decode('ascii').replace('\\','/').upper()
                if not name.startswith('FMV/') or '..' in name or ':' in name:raise ValueError('Invalid movie path')
                names.append(name)
    offsets=struct.unpack_from('<'+'H'*(levels+1),data,pos);pos+=(levels+1)*2
    size=struct.unpack_from('<H',data,pos)[0];pos+=2
    if levels<2 or size>len(data)-pos:raise ValueError('Invalid gameflow script')
    result=[]
    demos=struct.unpack_from('<H',header,74)[0]
    for index in range(1+levels-demos):
        start=offsets[index]
        end=offsets[index+1] if index+1<len(offsets) else size
        if not 0<=start<=end<=size or (end|start)&1:raise ValueError('Invalid script bounds')
        words=struct.unpack_from('<'+'H'*((end-start)//2),data,pos+start)
        at=0
        while at<len(words):
            op=words[at];at+=1
            if op==9:break
            if index and op in (6,11,13,14,15,21,22):continue
            if at==len(words):raise ValueError('Truncated command')
            value=words[at];at+=1
            if op==3 and value<len(names):
                if names[value] not in result:result.append(names[value])
            elif index and op==4 and value==index-1:continue
            elif not index or op not in (5,10,16,17,18,19,20):raise ValueError('Unsupported gameflow command')
        else:raise ValueError('Unterminated script')
    return result


def digest(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()


def prepare(root):
    manifest=root/'movie-cache-manifest.json'
    previous=json.loads(manifest.read_text()) if manifest.exists() else {}
    cached={item['source']:item for item in previous.get('files',[]) if previous.get('converter')==1}
    records=[]
    for name in startup_movies(root):
        source=root/name;target=source.with_suffix('.bmv')
        existing=cached.get(name,{})
        if target.is_file() and existing.get('source_sha256')==digest(source) and existing.get('sha256')==digest(target):
            records.append(existing)
            continue
        convert(source,target)
        records.append(dict(source=name,source_sha256=digest(source),path=target.relative_to(root).as_posix(),sha256=digest(target),bytes=target.stat().st_size))
    (root/'movie-cache-manifest.json').write_text(json.dumps(dict(converter=1,format='TRBMOV1',files=records),indent=2)+'\n')
    print(f'Prepared {len(records)} gameflow movies')


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('root',type=Path);args=parser.parse_args();prepare(args.root)
