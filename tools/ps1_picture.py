"""Bounded RNC2 decoding for the PS1 disc's 384x256 RGB555 pictures."""
import struct
from pathlib import Path

def crc16(data):
    crc=0
    for value in data:
        crc^=value
        for _ in range(8):crc=(crc>>1)^(0xa001 if crc&1 else 0)
    return crc

def unpack(data):
    if len(data)<18 or data[:4]!=b'RNC\x02':raise ValueError('Expected RNC2 picture')
    size,packed,plain_crc,packed_crc=struct.unpack_from('>IIHH',data,4)
    payload=data[18:18+packed]
    if size!=196608 or len(payload)!=packed or crc16(payload)!=packed_crc:raise ValueError('Invalid RNC picture length/checksum')
    at=0;bits=0;remain=0;result=bytearray()
    def byte():
        nonlocal at
        if at>=len(payload):raise ValueError('Truncated RNC picture')
        value=payload[at];at+=1;return value
    def bit(count=1):
        nonlocal bits,remain
        value=0
        for _ in range(count):
            if not remain:bits=byte();remain=8
            value=(value<<1)|(bits>>7);bits=(bits<<1)&255;remain-=1
        return value
    def distance():
        high=0
        if bit():
            high=bit()
            if bit():
                high=(high<<1|bit())|4
                if not bit():high=high<<1|bit()
            elif not high:high=bit()+2
        return (high<<8|byte())+1
    if bit(2):raise ValueError('Locked/encrypted RNC pictures unsupported')
    while len(result)<size:
        if not bit():result.append(byte());continue
        if bit():
            if not bit():length=2;back=byte()+1
            else:
                length=byte()+8 if bit() else 3
                if length==8:
                    more=bit()
                    if not more:break
                    continue
                back=distance()
        else:
            length=bit()+4
            if bit():length=(length-1)*2+bit()
            if length==9:
                for _ in range(bit(4)*4+12):result.append(byte())
                if len(result)>size:raise ValueError('RNC output overflow')
                continue
            back=distance()
        if back>len(result) or len(result)+length>size:raise ValueError('Invalid RNC back reference')
        for _ in range(length):result.append(result[-back])
    if len(result)!=size or crc16(result)!=plain_crc:raise ValueError('RNC picture verification failed')
    return bytes(result)

def convert(source,destination):
    data=unpack(Path(source).read_bytes())
    # 384x256 little-endian RGB555 pixels, displayed in a 4:3 viewport.
    pixels=bytearray()
    for value, in struct.iter_unpack('<H',data):pixels+=bytes(((value&31)*255//31,((value>>5)&31)*255//31,((value>>10)&31)*255//31))
    Path(destination).write_bytes(b'TRBIMG1\0'+struct.pack('<II',384,256)+pixels)
