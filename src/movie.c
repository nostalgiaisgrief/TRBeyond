#include "movie.h"
#include <stdlib.h>
#include <string.h>
static uint32_t number(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
void tomb_movie_close(TombMovie *m){if(m->file)fclose(m->file);free(m->pixels);free(m->audio);memset(m,0,sizeof *m);}
int tomb_movie_open(TombMovie *m,const char *path){
    memset(m,0,sizeof *m);m->file=fopen(path,"rb");if(!m->file)return 0;
    unsigned char h[40];if(fread(h,1,40,m->file)!=40 || memcmp(h,"TRBMOV1",8))goto fail;
    m->width=number(h+8);m->height=number(h+12);m->numerator=number(h+16);m->denominator=number(h+20);m->frames=number(h+24);m->rate=number(h+28);m->channels=number(h+32);m->audio_bytes=number(h+36);
    if(!m->width || m->width>2048 || !m->height || m->height>2048 || !m->numerator || m->numerator>100000 || !m->denominator || m->denominator>100000 || !m->frames || m->frames>100000 || m->rate!=44100 || m->channels!=2 || m->audio_bytes>100000000 || m->audio_bytes%4)goto fail;
    uint64_t video=(uint64_t)m->width*m->height*3*m->frames,total=40+video+m->audio_bytes;
    if(total>INT32_MAX || fseek(m->file,0,SEEK_END) || (uint64_t)ftell(m->file)!=total)goto fail;
    m->pixels=malloc((size_t)m->width*m->height*3);m->audio=malloc(m->audio_bytes?m->audio_bytes:1);if(!m->pixels || !m->audio)goto fail;
    if(fseek(m->file,(long)(40+video),SEEK_SET) || fread(m->audio,1,m->audio_bytes,m->file)!=m->audio_bytes)goto fail;
    if(!tomb_movie_frame(m,0))goto fail;return 1;
fail:tomb_movie_close(m);return 0;
}
int tomb_movie_frame(TombMovie *m,unsigned frame){
    size_t size=(size_t)m->width*m->height*3;
    if(!m->file || frame>=m->frames || fseek(m->file,(long)(40+(uint64_t)size*frame),SEEK_SET))return 0;
    return fread(m->pixels,1,size,m->file)==size;
}
