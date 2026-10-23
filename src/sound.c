#include "sound.h"
#include "fixed.h"
#include "object_contact.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static unsigned word(const unsigned char *p){return p[0]|(unsigned)p[1]<<8;}
static uint32_t dword(const unsigned char *p){return (uint32_t)word(p)|(uint32_t)word(p+2)<<16;}
unsigned tomb_sound_random(uint32_t *seed) { *seed=*seed*UINT32_C(1103515245)+12345;return (*seed>>10)&32767; }
int tomb_sound_plan(const TombSoundDetail *d,int distance,int environment,int wet,uint32_t *seed,TombSoundPlan *out) {
    if(!d || !seed || !out || distance<0 || (environment!=2 && (environment&1)!=!!wet))return 0;
    if(d->chance && tomb_sound_random(seed)>d->chance)return 0;
    int volume=(int)d->volume-distance*4;
    if(d->flags&0x4000)volume-=(int)(tomb_sound_random(seed)>>2);
    int mode=d->flags&3;
    if(volume<=0 && mode!=2)return 0;
    int pitch=100;
    if(d->flags&0x2000)pitch=90+(int)(10*tomb_sound_random(seed)/16384);
    unsigned count=(d->flags>>2)&15;
    if(!count || mode==3)return 0;
    int sample=d->sample;
    if(count!=1)sample+=(int)(count*tomb_sound_random(seed)/32768);
    if(volume>32767)volume=32767;
    *out=(TombSoundPlan){sample,volume,pitch,mode};return 1;
}
static const unsigned char *take(const TombLevel *l,size_t *at,size_t count,size_t width) {
    if(*at>l->file_size || (width && count>(l->file_size-*at)/width))return NULL;
    const unsigned char *p=l->file_data+*at;*at+=count*width;return p;
}
int tomb_sound_sample(TombSoundSample *out,const unsigned char *p,size_t bytes) {
    if(bytes<12 || memcmp(p,"RIFF",4) || memcmp(p+8,"WAVE",4))return 0;
    size_t end=(size_t)dword(p+4)+8;if(end>bytes || end<12)return 0;
    unsigned align=0;const unsigned char *pcm=NULL;size_t pcm_bytes=0;
    for(size_t at=12;at+8<=end;) {
        size_t n=dword(p+at+4);if(n>end-at-8)return 0;
        if(!memcmp(p+at,"fmt ",4)) {
            if(n<16 || word(p+at+8)!=1)return 0;
            out->channels=word(p+at+10);out->rate=dword(p+at+12);align=word(p+at+20);out->bits=word(p+at+22);
        } else if(!memcmp(p+at,"data",4)){pcm=p+at+8;pcm_bytes=n;}
        at+=8+n+(n&1);
    }
    if(!pcm || !out->rate || out->rate>192000 || (out->bits!=8 && out->bits!=16) || !out->channels || out->channels>2 || align!=out->channels*out->bits/8 || pcm_bytes%align)return 0;
    out->pcm=pcm;out->frames=pcm_bytes/align;return out->frames>0;
}
void tomb_sound_bank_free(TombSoundBank *b){if(b){free(b->details);free(b->samples);memset(b,0,sizeof *b);}}
int tomb_sound_bank_load(TombSoundBank *b,const TombLevel *l) {
    if(!b || !l)return 0;
    memset(b,0,sizeof *b);size_t source_at=l->sound_sources_offset;const unsigned char *p;
    p=take(l,&source_at,1,4);if(!p)goto fail;b->source_count=dword(p);
    b->sources=take(l,&source_at,b->source_count,16);if(!b->sources)goto fail;
    size_t at=l->sound_bank_offset;
    TombLevel bank_view=*l;
    if(l->asset.version)bank_view.file_size=l->asset.span[BA_SFXB].offset+l->asset.span[BA_SFXB].size;
    l=&bank_view;
    p=take(l,&at,256,2);if(!p)goto fail;
    for(int i=0;i<256;i++)b->map[i]=(int16_t)word(p+2*i);
    p=take(l,&at,1,4);if(!p)goto fail;b->detail_count=dword(p);
    p=take(l,&at,b->detail_count,8);if(!p || !b->detail_count)goto fail;
    b->details=calloc(b->detail_count,sizeof *b->details);if(!b->details)goto fail;
    for(size_t i=0;i<b->detail_count;i++)b->details[i]=(TombSoundDetail){(uint16_t)word(p+i*8),(uint16_t)word(p+i*8+2),(uint16_t)word(p+i*8+4),(uint16_t)word(p+i*8+6)};
    p=take(l,&at,1,4);if(!p)goto fail;size_t bytes=dword(p);
    const unsigned char *blob=take(l,&at,bytes,1);if(!blob)goto fail;
    p=take(l,&at,1,4);if(!p)goto fail;b->sample_count=dword(p);
    p=take(l,&at,b->sample_count,4);if(!p || !b->sample_count || at!=l->file_size)goto fail;
    b->samples=calloc(b->sample_count,sizeof *b->samples);if(!b->samples)goto fail;
    for(size_t i=0;i<b->sample_count;i++) {size_t offset=dword(p+4*i);if(offset>=bytes || !tomb_sound_sample(b->samples+i,blob+offset,bytes-offset))goto fail;}
    for(int i=0;i<256;i++)if(b->map[i]<-1 || (b->map[i]>=0 && (size_t)b->map[i]>=b->detail_count))goto fail;
    for(size_t i=0;i<b->detail_count;i++) {TombSoundDetail *d=b->details+i;unsigned count=(d->flags>>2)&15;if(!count || d->sample>=b->sample_count || count>b->sample_count-d->sample)goto fail;}
    return 1;
fail:tomb_sound_bank_free(b);return 0;
}
void tomb_sound_init(TombSoundMixer *m,const TombSoundBank *b){memset(m,0,sizeof *m);m->bank=b;m->random=0x1234;for(int i=0;i<TOMB_SOUND_CHANNELS;i++)m->loop_distance[i]=0x7fffffff;}
void tomb_sound_clear(TombSoundMixer *m){if(m)memset(m->voices,0,sizeof m->voices);}
/* Preserve the original angular pan (38b90), but correct its half-range
   mismatch with the DOS digital mixer: 16384 is centre, 0/32768 are sides. */
int tomb_sound_pan(int angle){int pan=tomb_word(angle+16384);return pan<0?-pan:pan;}
void tomb_sound_gains(int angle,int volume,int gains[2]){
    int pan=tomb_sound_pan(angle);
    int left=pan<=16384?16384:32768-pan,right=pan>=16384?16384:pan;
    gains[0]=volume*left/16384;gains[1]=volume*right/16384;
}
void tomb_sound_listener(TombSoundMixer *m,const double origin[3],int yaw){memcpy(m->origin,origin,sizeof m->origin);m->yaw=yaw;}
static int spatial(TombSoundMixer *m,const int32_t xyz[3],int positional,int flags,const double listener[3],int *angle){
    *angle=0;if(!positional)return 0;
    double dx=xyz[0]-listener[0],dy=xyz[1]-listener[1],dz=xyz[2]-listener[2];
    if(fabs(dx)>8192 || fabs(dy)>8192 || fabs(dz)>8192)return -1;
    int distance=(int)sqrt(dx*dx+dy*dy+dz*dz);
    if((dx || dy || dz) && !(flags&0x1000))
        *angle=tomb_word(tomb_object_angle((int32_t)(xyz[2]-m->origin[2]),(int32_t)(xyz[0]-m->origin[0]))-m->yaw);
    return distance;
}
static int same_source(const TombSoundVoice *v,const TombSoundEvent *e){
    if(v->positional!=(e->kind!=9))return 0;
    if(e->kind==9)return 1;
    if(v->source || e->source)return v->source==e->source;
    return v->xyz[0]==e->x && v->xyz[1]==e->y && v->xyz[2]==e->z;
}
void tomb_sound_event(TombSoundMixer *m,const TombSoundEvent *e,int wet,const double listener[3]) {
    if(!m || !m->bank || !e)return;
    if(e->kind==7){for(int i=0;i<TOMB_SOUND_CHANNELS;i++)if(m->voices[i].id==e->id)m->voices[i].active=0;return;}
    int id=e->id,environment=e->environment;
    if(e->kind==6 && id==3) { /* DOS bubble effect emits sound only when its random bubble count is nonzero. */
        if(3*tomb_sound_random(&m->random)/32768==0)return;
        id=37;environment=1;
    } else if(e->kind!=5 && e->kind!=9)return;
    if(id<0 || id>=256 || m->bank->map[id]<0)return;
    int32_t xyz[3]={e->x,e->y,e->z};int angle;
    int distance=spatial(m,xyz,e->kind!=9,m->bank->details[m->bank->map[id]].flags,listener,&angle);if(distance<0)return;
    TombSoundPlan plan;if(!tomb_sound_plan(m->bank->details+m->bank->map[id],distance,environment,wet,&m->random,&plan))return;
    int slot=-1;
    for(int i=0;i<TOMB_SOUND_CHANNELS;i++) {
        TombSoundVoice *v=m->voices+i;
        if(v->active && v->id==id && (plan.mode==2 || same_source(v,e))){if(plan.mode==0)return;if(plan.mode==2){if(distance<m->loop_distance[i]){m->loop_distance[i]=distance;v->volume=plan.volume;v->pitch=plan.pitch;v->angle=angle;v->source=e->source;memcpy(v->xyz,xyz,sizeof xyz);}return;}slot=i;break;}
        if(!v->active)slot=i;
    }
    if(slot<0){m->dropped++;return;}
    m->voices[slot]=(TombSoundVoice){.active=1,.id=id,.sample=plan.sample,.volume=plan.volume,.pitch=plan.pitch,.mode=plan.mode,.source=e->source,.xyz={xyz[0],xyz[1],xyz[2]},.positional=e->kind!=9,.effect=1,.angle=angle};m->played++;m->loop_distance[slot]=distance;
}
/* DOS SoundEffects 0x1d584: normal=0x80, flipped=0x40, dry environment.
   Mode-two sources refresh each tick; the nearest request owns each loop. */
void tomb_sound_frame(TombSoundMixer *m,const TombSoundEvent *events,size_t count,int flipped,int wet,const double listener[3]){
    if(!m || !m->bank)return;
    for(int i=0;i<TOMB_SOUND_CHANNELS;i++)m->loop_distance[i]=0x7fffffff;
    for(size_t i=0;i<m->bank->source_count;i++){
        const unsigned char *p=m->bank->sources+16*i;
        if(!(word(p+14)&(flipped?0x40:0x80)))continue;
        TombSoundEvent e={5,(int16_t)word(p+12),0,(int32_t)dword(p),(int32_t)dword(p+4),(int32_t)dword(p+8),NULL};
        tomb_sound_event(m,&e,wet,listener);
    }
    for(size_t i=0;i<count;i++)tomb_sound_event(m,events+i,wet,listener);
    for(int i=0;i<TOMB_SOUND_CHANNELS;i++){
        TombSoundVoice *v=m->voices+i;if(!v->active || !v->effect || !v->positional || v->mode==2)continue;
        if(v->source)memcpy(v->xyz,v->source,sizeof v->xyz);
        const TombSoundDetail *d=m->bank->details+m->bank->map[v->id];
        int distance=spatial(m,v->xyz,1,d->flags,listener,&v->angle);
        v->volume=distance<0?0:(int)d->volume-distance*4;
        if(v->volume>32767)v->volume=32767;
        if(v->volume<=0)v->active=0;
    }
    for(int i=0;i<TOMB_SOUND_CHANNELS;i++)if(m->voices[i].mode==2 &&
       (m->loop_distance[i]==0x7fffffff || m->voices[i].volume<=0))m->voices[i].active=0;
}
static double value(const TombSoundSample *s,size_t frame,unsigned channel) {
    const unsigned char *p=s->pcm+(frame*s->channels+(s->channels==1?0:channel))*s->bits/8;
    return s->bits==8?((int)p[0]-128)*256:(int16_t)word(p);
}
void tomb_sound_mix(TombSoundMixer *m,int16_t *out,size_t frames) {
    for(size_t f=0;f<frames;f++) {
        double sum[2]={0,0};
        for(int i=0;i<TOMB_SOUND_CHANNELS;i++) {
            TombSoundVoice *v=m->voices+i;if(!v->active)continue;
            const TombSoundSample *s=m->bank->samples+v->sample;
            if(v->position>=s->frames){if(v->mode==2)v->position=fmod(v->position,s->frames);else {v->active=0;continue;}}
            size_t a=(size_t)v->position,b=a+1<s->frames?a+1:a;double t=v->position-a;
            int gains[2]={v->volume,v->volume};if(v->effect)tomb_sound_gains(v->angle,v->volume,gains);
            for(unsigned c=0;c<2;c++)sum[c]+=(value(s,a,c)*(1-t)+value(s,b,c)*t)*gains[c]/32767.0;
            v->position+=(double)s->rate*v->pitch/(100*TOMB_SOUND_RATE);
        }
        /* Preserve recorded stereo for music; effects use corrected symmetric gains. Modern mix headroom. */
        for(int c=0;c<2;c++){double v=sum[c]*.65;if(v>32767)v=32767;if(v<-32768)v=-32768;out[f*2+c]=(int16_t)v;}
    }
}
