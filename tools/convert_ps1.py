"""PS1 authored content conversion to Beyond sections (no DOS inputs)."""
import io
import struct
import wave
from pathlib import Path
from convert_assets import Reader, encode_sections, decode

def pack(fmt,*values):return struct.pack('<'+fmt,*values)
def counted(data,width=1,short=False):return pack('H' if short else 'I',len(data)//width)+data

class PSReader(Reader):
    def read(self,fmt):return struct.unpack('<'+fmt,self.take(struct.calcsize('<'+fmt)))
    def block(self,width,short=False):return self.take(self.number(2 if short else 4),width)

def rgb555(value):
    return bytes(((value&31)*255//31,((value>>5)&31)*255//31,((value>>10)&31)*255//31,255 if value else 0))

class Atlas:
    def __init__(self,pages,cluts):
        self.pages,self.cluts=pages,cluts;self.tiles=[];self.cache={};self.x=self.y=self.height=0
    def put(self,tile,clut,uv):
        if not 0<=tile<13 or not 0<=clut<1024:raise ValueError('Invalid PS1 texture page/palette')
        x0=min(p[0] for p in uv);y0=min(p[1] for p in uv);x1=max(p[0] for p in uv);y1=max(p[1] for p in uv)
        key=(tile,clut,x0,y0,x1,y1)
        if key not in self.cache:
            w,h=x1-x0+1,y1-y0+1
            if self.x+w>256:self.x=0;self.y+=self.height;self.height=0
            if not self.tiles or self.y+h>256:self.tiles.append(bytearray(256*256*4));self.x=self.y=self.height=0
            dest=self.tiles[-1];palette=[rgb555(v[0]) for v in struct.iter_unpack('<H',self.cluts[clut*32:clut*32+32])]
            for y in range(h):
                for x in range(w):
                    sx,sy=x+x0,y+y0;b=self.pages[tile*32768+(sy*256+sx)//2];index=(b>>(4*(sx&1)))&15
                    pos=((self.y+y)*256+self.x+x)*4;dest[pos:pos+4]=palette[index]
            self.cache[key]=(len(self.tiles)-1,self.x-x0,self.y-y0)
            self.x+=w;self.height=max(self.height,h)
        page,dx,dy=self.cache[key]
        return page,[(x+dx,y+dy) for x,y in uv]

def geometry(data,room=False):
    r=PSReader(data);out=bytearray()
    if room:r.take(2);n=r.number(2);out+=pack('H',n)
    else:
        out+=r.take(10);signed=r.read('h')[0];n=abs(signed);out+=pack('H',n)
    for _ in range(n):
        x,y,z,w=r.read('hhhH')
        out+=pack('hhh',x,y,z)
        if room:out+=pack('h',max(0,min(8191,8191-w*32)))
    if not room:
        out+=pack('h',signed)
        for _ in range(n):out+=r.take(6) if signed>0 else r.take(2);r.take(2 if signed>0 else 0)
    for vertices in (4,3):
        count=r.number(2);out+=pack('H',count)
        for _ in range(count):
            face=list(r.read('H'*(vertices+1)))
            if room and vertices==4:face[2],face[3]=face[3],face[2]
            out+=pack('H'*(vertices+1),*face)
    if room:out+=counted(r.block(4,True),4,True)
    else:out+=bytes(4) # PS1 stores coloured surfaces in its texture table too.
    return bytes(out)

def adpcm(data):
    coefficients=((0,0),(60,0),(115,-52),(98,-55),(122,-60));a=b=0;pcm=bytearray()
    if len(data)%16:raise ValueError('Unaligned SPU sample')
    for off in range(0,len(data),16):
        shift,filter_=data[off]&15,data[off]>>4;flags=data[off+1]
        if filter_>4 or shift>12:raise ValueError('Invalid SPU ADPCM predictor')
        c,d=coefficients[filter_]
        for byte in data[off+2:off+16]:
            for nibble in (byte&15,byte>>4):
                v=((nibble if nibble<8 else nibble-16)<<12)>>shift
                v=max(-32768,min(32767,v+((a*c+b*d+32)>>6)));b,a=a,v;pcm+=pack('h',v)
        if flags&1:break
    output=io.BytesIO()
    with wave.open(output,'wb') as f:f.setparams((1,2,11025,0,'NONE','not compressed'));f.writeframes(pcm)
    return output.getvalue()

def sections(data,cut=False):
    r=PSReader(data);samples=[]
    if cut:r.take(8)
    else:
        texture=struct.unpack_from('<I',data,16)[0]+8
        sounds=struct.unpack_from('<H',data,22)[0]
        if sounds>256:raise ValueError('Invalid VAB sample count')
        r.take(2086+sounds*512)
        sizes=[r.number(2)*8 for _ in range(sounds)]
        r=PSReader(data);r.take(2600+sounds*512)
        samples=[adpcm(r.take(size)) for size in sizes]
        if r.at>texture:raise ValueError('SPU bank overlaps textures')
        r.take(texture-r.at)
    atlas=Atlas(r.take(13,32768),r.take(1024,32));r.take(4)
    room_count=r.number(2);rooms=bytearray(pack('H',room_count))
    for _ in range(room_count):
        rooms+=r.take(16);rooms+=counted(geometry(r.block(2),True),2)
        rooms+=counted(r.block(32,True),32,True)
        nz,nx=r.read('HH');rooms+=pack('HH',nz,nx)+r.take(nz*nx,8)+r.take(2)
        count=r.number(2);rooms+=pack('H',count)
        for _ in range(count):rooms+=r.take(14);r.take(2);rooms+=r.take(4)
        count=r.number(2);rooms+=pack('H',count)
        for _ in range(count):rooms+=r.take(18);r.take(2)
        rooms+=r.take(4)
    result={'TEX8':pack('I',0),'ROOM':bytes(rooms),'FLOR':counted(r.block(2),2)}
    mesh=r.block(2);offsets=[x[0] for x in struct.iter_unpack('<I',r.block(4))];new_mesh=bytearray();mapping={}
    for off in sorted(set(offsets)):
        if off>=len(mesh) or off%2:raise ValueError('Invalid mesh offset')
        mapping[off]=len(new_mesh);new_mesh+=geometry(mesh[off:])
    result['MESH']=counted(new_mesh,2)+counted(b''.join(pack('I',mapping[o]) for o in offsets),4)
    result['ANIM']=b''.join(counted(r.block(w),w) for w in (32,6,8,2))
    models=bytearray()
    for w in (4,2):models+=counted(r.block(w),w)
    original_models=r.block(20);models+=counted(b''.join(original_models[i:i+18] for i in range(0,len(original_models),20)),18)
    models+=counted(r.block(32),32);result['MODL']=bytes(models)
    textures=bytearray();palette=bytearray()
    for number,i in enumerate(struct.iter_unpack('<BBHBBHBBHBBH',r.block(16))):
        x0,y0,clut,x1,y1,tile,x2,y2,unused,x3,y3,attr=i
        if attr not in (0,1,2):raise ValueError('Unsupported PS1 texture blending mode')
        # The first 256 entries are flat colours, sampled at UV0 by PS1.
        if number<256:
            if not 0<=tile&16383<13 or clut>=1024:raise ValueError('Invalid PS1 colour texture')
            byte=atlas.pages[(tile&16383)*32768+(y0*256+x0)//2]
            index=(byte>>(4*(x0&1)))&15
            colour=rgb555(struct.unpack_from('<H',atlas.cluts,clut*32+index*2)[0])
            palette.extend(c*63//255 for c in colour[:3])
            x1=x2=x3=x0;y1=y2=y3=y0
        page,uv=atlas.put(tile&16383,clut,[(x0,y0),(x1,y1),(x2,y2),(x3,y3)])
        textures+=pack('HH',attr,page)+b''.join(pack('HH',x*256+128,y*256+128) for x,y in uv)
    sprites=bytearray()
    for l,t,rr,b,clut,tile,u0,v0,u1,v1 in struct.iter_unpack('<hhhhHHBBBB',r.block(16)):
        page,uv=atlas.put(tile&16383,clut,[(u0,v0),(u1,v1)]);(x0,y0),(x1,y1)=uv
        sprites+=pack('HBBHHhhhh',page,x0,y0,(x1-x0)*256,(y1-y0)*256,l,t,rr,b)
    result['TEXI']=counted(textures,20)+counted(sprites,16)+counted(r.block(8),8)
    world=bytearray()
    for w in (16,16):world+=counted(r.block(w),w)
    boxes=r.block(20);world+=counted(boxes,20)+counted(r.block(2),2)+r.take(len(boxes)//20,12)
    world+=counted(r.block(2),2)+counted(r.block(22),22);result['WRLD']=bytes(world)
    if len(palette)!=768:raise ValueError('Missing PS1 flat colour table')
    # PS1 UI colours come from its own flat-colour textures. World shading
    # and translucent panels operate directly on RGB, not this index table.
    result['PALT']=bytes(range(256))*32+palette
    sound_map=r.take(512);details=r.block(8);r.take(4)
    result['CINE']=counted(r.block(16,True),16,True)+pack('H',0)
    if r.at!=len(data):raise ValueError('Unexpected PS1 trailing bytes')
    blob=bytearray();offsets=[]
    for sample in samples:offsets.append(len(blob));blob+=sample
    result['SFXB']=sound_map+counted(details,8)+counted(blob)+counted(b''.join(pack('I',o) for o in offsets),4)
    result['TEXC']=counted(b''.join(atlas.tiles),256*256*4)
    return result

def convert_level(source,destination):
    source,destination=Path(source),Path(destination);data=source.read_bytes()
    result=encode_sections(data,sections(data,source.stem.upper().startswith('CUT')),2)
    decode(result);destination.parent.mkdir(parents=True,exist_ok=True)
    temp=destination.with_suffix('.tmp')
    try:temp.write_bytes(result);temp.replace(destination)
    finally:
        if temp.exists():temp.unlink()

if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('destination',type=Path);args=p.parse_args()
    for file in sorted(args.source.glob('*.PSX')):
        convert_level(file,args.destination/(file.stem+'.TBL'));print(file.name,flush=True)
