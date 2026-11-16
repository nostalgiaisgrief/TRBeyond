#include <windows.h>
#include <GL/gl.h>
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tr3_level.h"
#include "tr3_title_ring.h"
#include "tr3_title_font.h"
#include "tr3_title.h"
#include "ring.h"

#define PI 3.14159265358979323846
static Tr3Level *level;
static GLuint textures[256],mesh_lists,title_picture;
static char asset_root[MAX_PATH];
static int font_base;
static SDL_AudioStream *music;
static Uint8 *music_pcm;
static int music_size;
static int audio_initialized;
static Tr3TitleRing title_ring;
static int16_t title_spin[5];
static const char *const title_names[]={"Game","Controls","Sound","Detail Levels","Lara's Home"};

static int asset_path(char path[MAX_PATH],const char *name){
    int n=snprintf(path,MAX_PATH,"%s/%s",asset_root,name);
    return n>0 && n<MAX_PATH;
}
int tr3_title_available(const char *root){
    const char *dirs[]={"Source/TR3/PC","data_reference_files/TR3/PC/Tomb Raider III"};
    const char *files[]={"data/title.tr2","pix/titleuk.bmp","data/tombpc.dat","audio/cdaudio.wad"};
    for(unsigned i=0;i<2;i++){
        if(snprintf(asset_root,sizeof asset_root,"%s/%s",root,dirs[i])>=MAX_PATH)continue;
        int ok=1;
        for(unsigned j=0;j<4;j++){
            char path[MAX_PATH];
            if(!asset_path(path,files[j]) || GetFileAttributesA(path)==INVALID_FILE_ATTRIBUTES)ok=0;
        }
        if(ok)return 1;
    }
    return 0;
}
static uint16_t tr3_texture_uv(uint16_t raw){return (uint16_t)((raw&0xff00)+(raw&0x80?-1:256));}
static float brightness(int shade){float v=(8192-shade)/8192.f;return v<0?0:v>1?1:v;}
static void set_texture(uint16_t index){
    const uint8_t *p=level->textures.data+(index&0x7fff)*20;
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,textures[tr3_u16(p+2)&0x7fff]);
    if(tr3_u16(p)){glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0.5f);}else glDisable(GL_ALPHA_TEST);
}
static void texcoord(uint16_t index,int vertex){
    const uint8_t *p=level->textures.data+(index&0x7fff)*20+4+vertex*4;
    glTexCoord2f(tr3_texture_uv(tr3_u16(p))/65536.f,tr3_texture_uv(tr3_u16(p+2))/65536.f);
}
static void face_sides(int double_sided){
    glFrontFace(GL_CW);glCullFace(GL_BACK);
    if(double_sided)glDisable(GL_CULL_FACE);else glEnable(GL_CULL_FACE);
}
static void mesh_faces(const uint8_t **cursor,const uint8_t *vertices,int n,int coloured,const uint8_t *lights){
    int count=tr3_u16(*cursor);*cursor+=2;
    for(int i=0;i<count;i++){
        const uint8_t *p=*cursor;*cursor+=(n+1)*2;
        unsigned texture=tr3_u16(p+n*2);
        if(coloured){glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);}else set_texture((uint16_t)texture);
        face_sides(!coloured && (texture&0x8000));
        glBegin(n==4?GL_QUADS:GL_TRIANGLES);
        for(int j=0;j<n;j++){
            unsigned index=tr3_u16(p+j*2);
            const uint8_t *v=vertices+index*6;
            float light=lights?brightness(tr3_i16(lights+index*2)):.82f;
            if(coloured){
                const uint8_t *rgb=level->palette16+(texture>>8)*4;
                glColor3f(rgb[0]*light/255,rgb[1]*light/255,rgb[2]*light/255);
            }else{texcoord((uint16_t)texture,j);glColor3f(light,light,light);}
            glVertex3s(tr3_i16(v),tr3_i16(v+2),tr3_i16(v+4));
        }
        glEnd();
    }
    glDisable(GL_CULL_FACE);
}
static void compile_mesh(unsigned index){
    const uint8_t *p=level->mesh_data.data+tr3_u32(level->mesh_offsets.data+index*4)+10;
    unsigned count=tr3_u16(p);const uint8_t *vertices=p+2;p=vertices+count*6;
    int normals=tr3_i16(p);p+=2;
    const uint8_t *lights=normals<0 && -normals>=(int)count?p:NULL;
    p+=normals<0?-normals*2:normals*6;
    glNewList(mesh_lists+index,GL_COMPILE);
    mesh_faces(&p,vertices,4,0,lights);mesh_faces(&p,vertices,3,0,lights);
    mesh_faces(&p,vertices,4,1,lights);mesh_faces(&p,vertices,3,1,lights);
    glEndList();
}
static void rotate_frame(const uint8_t **cursor,const uint8_t *end){
    if(end-*cursor<2)return;
    uint16_t a=tr3_u16(*cursor);*cursor+=2;unsigned x=0,y=0,z=0;
    switch(a>>14){
    case 1:x=a&1023;break;
    case 2:y=a&1023;break;
    case 3:z=a&1023;break;
    default:
        if(end-*cursor<2)return;
        {uint16_t b=tr3_u16(*cursor);*cursor+=2;x=(a>>4)&1023;y=((a&15)<<6)|(b>>10);z=b&1023;}break;
    }
    glRotatef(y*(360.f/1024),0,1,0);glRotatef(x*(360.f/1024),1,0,0);glRotatef(z*(360.f/1024),0,0,1);
}
static void draw_model(int id){
    const uint8_t *m=tr3_model(level,(unsigned)id);
    if(!m)return;
    unsigned animation=tr3_u16(m+16);
    if(animation>=level->animations.count)return;
    unsigned offset=tr3_u32(level->animations.data+animation*32);
    size_t bytes=(size_t)level->frames.count*2;
    if(offset>bytes || bytes-offset<18)return;
    const uint8_t *frame=level->frames.data+offset,*end=level->frames.data+bytes;
    glPushMatrix();
    glTranslated(tr3_i16(frame+12),tr3_i16(frame+14),tr3_i16(frame+16));frame+=18;
    unsigned stack=0;
    for(unsigned i=0;i<tr3_u16(m+4);i++){
        if(i){
            const uint8_t *bone=level->trees.data+4*(tr3_u32(m+8)+(i-1)*4);
            unsigned flags=tr3_u32(bone);
            if((flags&1)&&stack){glPopMatrix();stack--;}
            if(flags&2){glPushMatrix();stack++;}
            glTranslated(tr3_i32(bone+4),tr3_i32(bone+8),tr3_i32(bone+12));
        }
        rotate_frame(&frame,end);
        glCallList(mesh_lists+tr3_u16(m+6)+i);
    }
    while(stack){glPopMatrix();stack--;}glPopMatrix();
}
static void init_geometry(void){
    glGenTextures((GLsizei)level->tile_count,textures);uint8_t pixels[256*256*4];
    for(uint32_t tile=0;tile<level->tile_count;tile++){
        for(int i=0;i<65536;i++){
            uint16_t c=tr3_u16(level->tiles16+tile*131072+i*2);
            pixels[i*4]=(uint8_t)(((c>>10)&31)*255/31);pixels[i*4+1]=(uint8_t)(((c>>5)&31)*255/31);
            pixels[i*4+2]=(uint8_t)((c&31)*255/31);pixels[i*4+3]=(c&0x8000)?255:0;
        }
        glBindTexture(GL_TEXTURE_2D,textures[tile]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    }
}
static void init_meshes(void){
    mesh_lists=glGenLists((GLsizei)level->mesh_offsets.count);
    for(uint32_t i=0;i<level->mesh_offsets.count;i++)compile_mesh(i);
}
static GLuint load_picture_texture(const char *path){
    HBITMAP bitmap=LoadImageA(NULL,path,IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_CREATEDIBSECTION);
    if(!bitmap)return 0;
    BITMAP bm;GetObject(bitmap,sizeof bm,&bm);size_t bytes=(size_t)bm.bmWidth*bm.bmHeight*4;
    unsigned char *pixels=malloc(bytes);if(!pixels){DeleteObject(bitmap);return 0;}
    BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=bm.bmWidth;info.bmiHeader.biHeight=bm.bmHeight;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    HDC dc=GetDC(NULL);int rows=GetDIBits(dc,bitmap,0,(UINT)bm.bmHeight,pixels,&info,DIB_RGB_COLORS);DeleteObject(bitmap);
    if(!rows){ReleaseDC(NULL,dc);free(pixels);return 0;}
    for(size_t i=0;i<bytes;i+=4){unsigned char b=pixels[i];pixels[i]=pixels[i+2];pixels[i+2]=b;pixels[i+3]=255;}
    GLuint picture;glGenTextures(1,&picture);glBindTexture(GL_TEXTURE_2D,picture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,bm.bmWidth,bm.bmHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);free(pixels);
    ReleaseDC(NULL,dc);return picture;
}

static void draw_glyph(void *unused,const TombTextGlyph *g){
    (void)unused;
    if(g->glyph<0 || g->glyph>=110)return;
    const uint8_t *sp=level->sprites.data+(font_base+g->glyph)*16;
    double sx=g->scale_x/65536.,sy=g->scale_y/65536.;
    double u=(sp[2]+.5)/256.,v=(sp[3]+.5)/256.;
    double du=(tr3_u16(sp+4)+1)/65536.-1/256.,dv=(tr3_u16(sp+6)+1)/65536.-1/256.;
    glBindTexture(GL_TEXTURE_2D,textures[tr3_u16(sp)]);
    glBegin(GL_QUADS);
    for(int k=0;k<4;k++){
        int right=k==1||k==2,bottom=k>=2;
        glTexCoord2d(u+right*du,v+bottom*dv);
        glVertex2d(g->x+sx*tr3_i16(sp+8+4*right),g->y+sy*tr3_i16(sp+10+4*bottom));
    }
    glEnd();
}
void tr3_title_close(TombLauncherStyle *style){
    if(level){
        glDeleteTextures((GLsizei)level->tile_count,textures);
        if(mesh_lists)glDeleteLists(mesh_lists,(GLsizei)level->mesh_offsets.count);
        tr3_free(level);level=NULL;
    }
    glDeleteTextures(1,&title_picture);title_picture=mesh_lists=0;
    memset(textures,0,sizeof textures);
    if(style)memset(style,0,sizeof *style);
}
int tr3_title_style(TombLauncherStyle *style){
    char path[MAX_PATH],error[200];
    if(!asset_path(path,"data/title.tr2") || !(level=tr3_load(path,error,sizeof error)))return 0;
    font_base=-1;
    for(unsigned i=0;i<level->sequences.count;i++){
        const uint8_t *p=level->sequences.data+8*i;
        if(tr3_u32(p)==356)font_base=tr3_i16(p+6);
    }
    if(font_base<0 || (unsigned)font_base+110>level->sprites.count)goto fail;
    for(int i=0;i<110;i++)if(tr3_u16(level->sprites.data+16*(font_base+i))>=level->tile_count)goto fail;
    for(int i=0;i<5;i++)if(!tr3_model(level,tr3_title_items[i].object))goto fail;
    init_geometry();
    style->glyph=draw_glyph;style->palette=level->palette;
    return 1;
fail:
    tr3_title_close(style);return 0;
}
static void start_music(void){
    char path[MAX_PATH];unsigned char header[340],entry[8];
    if(!asset_path(path,"data/tombpc.dat"))return;
    FILE *f=fopen(path,"rb");if(!f)return;
    int ok=fread(header,1,sizeof header,f)==sizeof header;fclose(f);
    if(!ok || tr3_u32(header)!=3)return;
    unsigned cue=tr3_u16(header+338);
    if(cue>=130 || !asset_path(path,"audio/cdaudio.wad"))return;
    f=fopen(path,"rb");if(!f)return;
    if(!fseek(f,268*cue+260,SEEK_SET) && fread(entry,1,8,f)==8){
        unsigned bytes=tr3_u32(entry),offset=tr3_u32(entry+4);
        if(bytes && bytes<64*1024*1024 && offset>=268*130 && offset<=INT32_MAX && !fseek(f,(long)offset,SEEK_SET)){
            void *raw=malloc(bytes);
            if(raw && fread(raw,1,bytes,f)==bytes){
                SDL_AudioSpec spec;Uint8 *pcm;Uint32 length;
                if(SDL_LoadWAV_IO(SDL_IOFromConstMem(raw,bytes),true,&spec,&pcm,&length)){
                    if(length && length<=INT32_MAX && (audio_initialized=SDL_InitSubSystem(SDL_INIT_AUDIO))){
                        music=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,NULL,NULL);
                        if(music){
                            music_pcm=pcm;pcm=NULL;music_size=(int)length;
                            SDL_PutAudioStreamData(music,music_pcm,music_size);
                            SDL_ResumeAudioStreamDevice(music);
                        }
                    }
                    SDL_free(pcm);
                }
            }
            free(raw);
        }
    }
    fclose(f);
}
int tr3_title_begin(int play_music){
    TombLauncherStyle style={0};char path[MAX_PATH];
    if(!tr3_title_style(&style))return 0;
    if(!asset_path(path,"pix/titleuk.bmp") || !(title_picture=load_picture_texture(path))){
        tr3_title_close(NULL);return 0;
    }
    init_meshes();
    memset(title_spin,0,sizeof title_spin);
    tr3_title_ring_init(&title_ring,0);if(play_music)start_music();return 1;
}
void tr3_title_end(void){
    if(music){SDL_DestroyAudioStream(music);music=NULL;}
    SDL_free(music_pcm);music_pcm=NULL;music_size=0;
    if(audio_initialized){SDL_QuitSubSystem(SDL_INIT_AUDIO);audio_initialized=0;}
    tr3_title_close(NULL);
}
void tr3_title_tick(unsigned buttons){
    if(music && SDL_GetAudioStreamQueued(music)<music_size/2)
        SDL_PutAudioStreamData(music,music_pcm,music_size);
    if(buttons&TOMB_RING_LEFT)tr3_title_ring_rotate(&title_ring,1);
    else if(buttons&TOMB_RING_RIGHT)tr3_title_ring_rotate(&title_ring,-1);
    tr3_title_ring_tick(&title_ring);
    for(int i=0;i<5;i++){
        if(i==title_ring.current && !title_ring.remaining)title_spin[i]=(int16_t)(title_spin[i]+256);
        else if(title_spin[i]>0)title_spin[i]-=title_spin[i]<256?title_spin[i]:256;
        else if(title_spin[i]<0)title_spin[i]+=title_spin[i]>-256?-title_spin[i]:256;
    }
}
void tr3_title_draw(int w,int h){
    int cw=w,ch=h;
    if(cw*3>ch*4)cw=ch*4/3;else ch=cw*3/4;
    if(!cw||!ch)return;
    glViewport((w-cw)/2,(h-ch)/2,cw,ch);
    glDisable(GL_SCISSOR_TEST);glClearColor(0,0,0,1);glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_ALPHA_TEST);glDisable(GL_CULL_FACE);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,640,480,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    float fade=(32-title_ring.opening)/32.f;
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,title_picture);glColor3f(fade,fade,fade);
    glBegin(GL_QUADS);
    glTexCoord2f(0,1);glVertex2i(0,0);glTexCoord2f(1,1);glVertex2i(640,0);
    glTexCoord2f(1,0);glVertex2i(640,480);glTexCoord2f(0,0);glVertex2i(0,480);glEnd();
    glEnable(GL_DEPTH_TEST);glMatrixMode(GL_PROJECTION);glLoadIdentity();
    double top=16*tan(80*PI/360)*.75;glFrustum(-top*cw/ch,top*cw/ch,-top,top,16,4096);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    double pitch=atan2(-96-title_ring.camera_y,598)-1024*2*PI/65536.;
    glRotated(pitch*180/PI,1,0,0);glScaled(-1,-1,1);glTranslated(0,-title_ring.camera_y,-title_ring.radius-598);
    glRotated(title_ring.yaw*360./65536.,0,1,0);
    for(int i=0;i<5;i++){
        glPushMatrix();glRotated(i*13107*360./65536.,0,1,0);glTranslated(title_ring.radius,0,0);
        glRotated(90,0,1,0);glRotated(title_spin[i]*360./65536.,0,1,0);
        glRotated(tr3_title_items[i].x_idle*360./65536.,1,0,0);
        draw_model(tr3_title_items[i].object);glPopMatrix();
    }
    if(!title_ring.opening && !title_ring.remaining){
        const char *name=title_names[title_ring.current];
        int length=0;
        for(const char *p=name;*p;p++)length+=*p==' '?6:T_textSpacing[T_remapASCII[*p-32]];
        glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,640,480,0,-1,1);
        glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
        glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1f);glColor3f(1,1,1);
        int x=320-length/2;
        for(const char *p=name;*p;p++){
            if(*p==' '){x+=6;continue;}
            int glyph=T_remapASCII[*p-32];
            TombTextGlyph g={glyph,x,432,65536,65536};draw_glyph(NULL,&g);x+=T_textSpacing[glyph];
        }
    }
}
