#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "tr2_audio.h"
#include "sound_win.h"
static char asset_root[MAX_PATH];
void tr2_audio_pause(int paused);

enum {VOICE_COUNT=24,BLOCKS=4,FRAMES=1024};
typedef struct {const uint8_t *data;uint32_t frames,rate;int channels,bits;} Sample;
typedef struct {Sample sample;double position,step;int volume,pan,id,loop,age;} Voice;
static int music_volume=10,effects_volume=10;
static TombSoundOutput *music_output;
static TombSoundMixer music_mixer;
static TombSoundBank music_bank;
static TombSoundSample music_sample;
static int inventory_open,output_paused;
void tr2_audio_volume(int music,int effects){music_volume=music<0?0:music>10?10:music;effects_volume=effects<0?0:effects>10?10:effects;tomb_sound_output_volume(music_output,music_volume);}
static HWAVEOUT device;
static WAVEHDR headers[BLOCKS];static int16_t output[BLOCKS][FRAMES*2];
static uint8_t *sfx,*music;static Sample *samples;static uint32_t sample_count;
static Voice voices[VOICE_COUNT];static int age,last_track=-1;static unsigned random_state=0x1234;
static unsigned random15(void){random_state=random_state*1103515245+12345;return (random_state>>10)&32767;}
static uint8_t *read_file(const char *path,size_t *size){FILE *f=fopen(path,"rb");if(!f)return NULL;fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);if(n<=0){fclose(f);return NULL;}uint8_t *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}*size=fread(p,1,(size_t)n,f);fclose(f);if(*size!=(size_t)n){free(p);return NULL;}return p;}
static Sample wave(const uint8_t *p,size_t size){
    Sample s={0};int pcm=0;if(size<12 || memcmp(p,"RIFF",4) || memcmp(p+8,"WAVE",4))return s;
    for(size_t at=12;at+8<=size;){uint32_t n=tr2_u32(p+at+4);if(n>size-at-8)break;
        if(!memcmp(p+at,"fmt ",4) && n>=16){pcm=tr2_u16(p+at+8)==1;s.channels=tr2_u16(p+at+10);s.rate=tr2_u32(p+at+12);s.bits=tr2_u16(p+at+22);}
        else if(!memcmp(p+at,"data",4)){s.data=p+at+8;s.frames=n;}
        at+=8+(size_t)n+(n&1);
    }
    if(!pcm || (s.channels!=1 && s.channels!=2) || (s.bits!=8 && s.bits!=16) || !s.rate){memset(&s,0,sizeof s);return s;}
    s.frames/=(unsigned)(s.channels*s.bits/8);return s;
}
int tr2_audio_init(const Tr2Level *l,const char *root){
    snprintf(asset_root,sizeof asset_root,"%s",root);char path[MAX_PATH];snprintf(path,sizeof path,"%s/DATA/MAIN.SFX",asset_root);size_t size;sfx=read_file(path,&size);if(!sfx)return 0;
    samples=calloc(l->sample_indices.count,sizeof *samples);if(!samples){tr2_audio_close();return 0;}sample_count=l->sample_indices.count;
    uint32_t ordinal=0;for(size_t at=0;at+12<=size;ordinal++){
        if(memcmp(sfx+at,"RIFF",4))break;size_t n=(size_t)tr2_u32(sfx+at+4)+8;if(n>size-at || n<12)break;
        for(uint32_t i=0;i<sample_count;i++)if(tr2_u32(l->sample_indices.data+i*4)==ordinal)samples[i]=wave(sfx+at,n);
        at+=n+(n&1);
    }
    WAVEFORMATEX format={0};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=44100;format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=176400;
    if(waveOutOpen(&device,tomb_sound_output_device(),&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){device=NULL;tr2_audio_close();return 0;}
    for(int i=0;i<BLOCKS;i++){headers[i].lpData=(LPSTR)output[i];headers[i].dwBufferLength=sizeof output[i];waveOutPrepareHeader(device,headers+i,sizeof headers[i]);headers[i].dwFlags|=WHDR_DONE;}
    return 1;
}
static void play_sample(const Tr2Level *l,int id,const Tr2Actor *source,const Tr2Actor *listener,int pitch){
    if(!device || id<0 || id>=370)return;int detail=tr2_i16(l->sound_map+id*2);if(detail<0 || (uint32_t)detail>=l->sound_details.count)return;
    const uint8_t *d=l->sound_details.data+detail*8;int flags=tr2_u16(d+6),volume=tr2_u16(d+2),chance=tr2_u16(d+4),mode=flags&3;
    if(chance && (int)random15()>chance)return;
    double dx=source->x-listener->x,dy=source->y-listener->y,dz=source->z-listener->z,distance=sqrt(dx*dx+dy*dy+dz*dz);if(distance>8192)return;
    volume=(int)(volume*(1-distance/8192));if(flags&0x4000)volume-=(int)(random15()&8191);if(volume<=0)return;
    unsigned count=(flags>>2)&15,index=tr2_u16(d);if(count>1)index+=(random15()*count)>>15;if(index>=sample_count || !samples[index].data)return;
    int slot=-1;for(int i=0;i<VOICE_COUNT-1;i++)if(voices[i].sample.data && voices[i].id==id){if(mode==1)return;if(mode==2 || mode==3){slot=i;break;}}
    if(slot<0)for(int i=0;i<VOICE_COUNT-1;i++)if(!voices[i].sample.data){slot=i;break;}
    if(slot<0){slot=0;for(int i=1;i<VOICE_COUNT-1;i++)if(voices[i].age<voices[slot].age)slot=i;}
    Voice *v=voices+slot;double pos=mode==3 && v->id==id?v->position:0;memset(v,0,sizeof *v);v->sample=samples[index];v->position=pos;v->step=v->sample.rate/44100.0;
    if(flags&0x2000)pitch+=((int)random15()*6000>>14)-6000;
    v->step*=pitch/65536.0;
    v->volume=volume;v->id=id;v->age=++age;v->loop=mode==3?2:0;
    if(!(flags&0x1000) && distance>0)v->pan=(int)((dx*tr2_cos(listener->yaw)-dz*tr2_sin(listener->yaw))/16384/distance*12000);
}
static void events_pitch(const Tr2Level *l,const Tr2Actor *a,const Tr2Actor *listener,int pitch){for(int i=0;i<a->event_count;i++)if(a->event_kind[i]==5 || a->event_kind[i]==7){int id=a->event_value[i];play_sample(l,id,a->event_kind[i]==7?listener:a,listener,(id==155 || id==195 || id==197)?pitch:65536);}}
static void events(const Tr2Level *l,const Tr2Actor *a,const Tr2Actor *listener){events_pitch(l,a,listener,65536);}
static int secret_track;
void tr2_audio_set_secret_track(int track){secret_track=track;}
static void play_music(int track,int loop,int restart){
    if(track>1 && (restart || track!=last_track)){
        char path[MAX_PATH];snprintf(path,sizeof path,"%s/AUDIO/track%02d.wav",asset_root,tr2_cd_track(track));size_t size;uint8_t *p=read_file(path,&size);
        if(p){TombSoundSample sample={0};if(tomb_sound_sample(&sample,p,size)){
            if(!music_output){memset(&music_bank,0,sizeof music_bank);tomb_sound_init(&music_mixer,&music_bank);music_output=tomb_sound_output_open_gain(&music_mixer,0);}
            if(music_output){
                tomb_sound_output_unity(music_output,1);
                /* Separate music transport shares TR1's output infrastructure.
                   Pausing it preserves queued PCM as well as the playback cursor;
                   the effects device remains available for inventory sounds. */
                tomb_sound_output_update(music_output,inventory_open || output_paused);
                tomb_sound_output_track(music_output,&music_bank,&music_sample,&sample,loop,32767,music_volume);
                free(music);music=p;last_track=track;
            }else free(p);
        }else free(p);}
    }
}
void tr2_audio_tick(const Tr2Level *l,const Tr2Actor *a,const Tr2World *w){
    if(!device)return;for(int i=0;i<VOICE_COUNT-1;i++)if(voices[i].loop && !--voices[i].loop)voices[i].sample.data=NULL;
    for(unsigned i=0;i<l->sounds.count;i++){
        Tr2Position p;int id=tr2_sound_source(l,i,w->flip_status,&p);if(id<0)continue;
        Tr2Actor source={0};source.x=p.x;source.y=p.y;source.z=p.z;
        play_sample(l,id,&source,a,65536);
    }
    if(w->flip_sound>=0 && w->flip_sound!=79)play_sample(l,w->flip_sound,a,a,65536);
    if(w->flip_sound==79){Tr2Actor source={0};source.x=w->flip_sound_position.x;source.y=w->flip_sound_position.y;source.z=w->flip_sound_position.z;source.room=w->flip_sound_position.room;play_sample(l,79,&source,a,65536);}
    events(l,a,a);for(uint32_t i=0;i<w->count;i++){const Tr2Object *o=w->objects+i;
        if(o->id==259 || o->id==216){int shift=o->id==216?6:2;Tr2Actor source=o->pose;source.x=a->x+tr2_asr(source.x-a->x,shift);source.y=a->y+tr2_asr(source.y-a->y,shift);source.z=a->z+tr2_asr(source.z-a->z,shift);events(l,&source,a);}
        else if(o->id==228){Tr2Actor source=o->pose;Tr2Position p;tr2_object228_sound(l,o,a,&p);source.x=p.x;source.y=p.y;source.z=p.z;events(l,&source,a);}
        else if(o->id==51)events_pitch(l,&o->pose,a,(int)((0xd8f004u+25600u*(uint32_t)o->pose.speed)>>8));
        else if(o->id==13)events_pitch(l,&o->pose,a,(int)((0xd8f004u+25600u*(uint32_t)o->snow.drive.engine_pitch)>>8));
        else if(o->id==14)events_pitch(l,&o->pose,a,tr2_boat_sound_pitch(&o->boat));
        else events(l,&o->pose,a);
    }
    if(w->hazards)for(int i=0;i<TR2_MAX_DARTS;i++)events(l,&w->hazards->darts[i].pose,a);
    if(w->hazards)for(int i=0;i<TR2_MAX_EFFECTS;i++){const Tr2Fragment *f=w->hazards->effects+i;
        if(w->effect_sounds[i]){Tr2Actor source={0};source.x=f->x;source.y=f->y;source.z=f->z;source.room=f->room;play_sample(l,w->effect_sounds[i],&source,a,65536);}
        if(f->active && f->object==238 && f->counter==4 && w->hazards->owner[i]==-3){Tr2Actor source={0};source.x=f->x;source.y=f->y;source.z=f->z;source.room=f->room;play_sample(l,10,&source,a,65536);}}
    for(int i=0;i<a->event_count;i++)if(a->event_kind[i]==8 && a->event_value[i]>=190 && a->event_value[i]<=192)play_music(secret_track,0,1);
    play_music(w->cd_play,0,0);
}
static int sample_value(const Sample *s,uint32_t frame,int channel){unsigned index=frame*(unsigned)s->channels+(s->channels==2?(unsigned)channel:0);return s->bits==16?tr2_i16(s->data+index*2):((int)s->data[index]-128)*256;}
void tr2_audio_pump(void){
    if(!device)return;
    for(int block=0;block<BLOCKS;block++)if(headers[block].dwFlags&WHDR_DONE){
        for(int frame=0;frame<FRAMES;frame++){int mix[2]={0};
            for(int i=0;i<VOICE_COUNT;i++){Voice *v=voices+i;if(!v->sample.data || !v->sample.frames)continue;
                if(v->position>=v->sample.frames){if(v->loop)v->position=fmod(v->position,v->sample.frames);else{v->sample.data=NULL;continue;}}
                for(int ch=0;ch<2;ch++){int volume=v->volume*(32767+(ch?v->pan:-v->pan))/65534;volume=volume*effects_volume/10;mix[ch]+=sample_value(&v->sample,(uint32_t)v->position,ch)*volume/32768;}
                v->position+=v->step;
            }
            for(int ch=0;ch<2;ch++)output[block][frame*2+ch]=(int16_t)(mix[ch]<-32768?-32768:mix[ch]>32767?32767:mix[ch]);
        }waveOutWrite(device,headers+block,sizeof headers[block]);
    }
}
void tr2_audio_close(void){tomb_sound_output_close(music_output);music_output=NULL;inventory_open=output_paused=0;if(device){waveOutReset(device);for(int i=0;i<BLOCKS;i++)waveOutUnprepareHeader(device,headers+i,sizeof headers[i]);waveOutClose(device);device=NULL;}free(samples);free(sfx);free(music);samples=NULL;sfx=music=NULL;sample_count=0;memset(voices,0,sizeof voices);last_track=-1;}


void tr2_audio_cut_music(int track){if(device)play_music(track,0,0);}
void tr2_audio_level_music(int track){if(device)play_music(track,1,0);}

void tr2_audio_menu(const Tr2Level *l,int sound){Tr2Actor origin={0};play_sample(l,sound,&origin,&origin,65536);}
void tr2_audio_stop_music(void){last_track=-1;tomb_sound_output_close(music_output);music_output=NULL;free(music);music=NULL;memset(&music_sample,0,sizeof music_sample);}
int tr2_audio_music_cue(void){return music_output?last_track:-1;}
int tr2_audio_music_active(void){return music_output!=NULL;}




void tr2_audio_pause(int paused){output_paused=paused;if(device){if(paused)waveOutPause(device);else waveOutRestart(device);}tomb_sound_output_update(music_output,inventory_open || output_paused);}
void tr2_audio_inventory(int open){inventory_open=open;tomb_sound_output_update(music_output,inventory_open || output_paused);}

int tr2_audio_inventory_test(void){
    if(!music_output || !device || !music_sample.frames)return 0;
    tr2_audio_inventory(1);unsigned held=tomb_sound_output_stats(music_output).played_samples;
    MMTIME before={0},after={0};before.wType=after.wType=TIME_SAMPLES;
    tr2_audio_pump();waveOutGetPosition(device,&before,sizeof before);
    Sleep(80);tr2_audio_pump();waveOutGetPosition(device,&after,sizeof after);
    int ok=tomb_sound_output_stats(music_output).played_samples==held && after.u.sample>before.u.sample;
    /* Focus changes must not resume music while the ring is still open. */
    tr2_audio_pause(1);tr2_audio_pause(0);Sleep(40);
    ok=ok && tomb_sound_output_stats(music_output).played_samples==held;
    tr2_audio_inventory(0);Sleep(80);
    return ok && tomb_sound_output_stats(music_output).played_samples>held;
}

