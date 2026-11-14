/* Title-only TR2 sample bank; shared Windows output-device selection. */
#include "sound_win.h"
enum { TR2_VOICES=16,TR2_BLOCKS=4,TR2_FRAMES=1024 };
typedef struct {const unsigned char *data;unsigned frames,rate;int channels,bits;} Tr2Sample;
typedef struct {Tr2Sample sample;double position;int volume,loop;} Tr2Voice;
static HWAVEOUT audio_device;
static WAVEHDR audio_headers[TR2_BLOCKS];
static int16_t audio_buffers[TR2_BLOCKS][TR2_FRAMES*2];
static unsigned char *audio_bank,*audio_music;
static Tr2Sample *audio_samples;
static Tr2Voice audio_voices[TR2_VOICES];
static unsigned audio_slot;
static unsigned char *asset_read(const char *name,size_t *size){
 FILE *f=asset_open(name);if(!f)return NULL;
 if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);rewind(f);
 if(n<=0 || n>256*1024*1024){fclose(f);return NULL;}
 unsigned char *p=malloc((size_t)n);if(!p){fclose(f);return NULL;}
 *size=fread(p,1,(size_t)n,f);fclose(f);if(*size!=(size_t)n){free(p);return NULL;}return p;
}
static Tr2Sample audio_wave(const unsigned char *p,size_t size){
 Tr2Sample s={0};int pcm=0;if(size<12 || memcmp(p,"RIFF",4) || memcmp(p+8,"WAVE",4))return s;
 for(size_t at=12;at+8<=size;){unsigned n=tr2_u32(p+at+4);if(n>size-at-8)break;
  if(!memcmp(p+at,"fmt ",4) && n>=16){pcm=tr2_u16(p+at+8)==1;s.channels=tr2_u16(p+at+10);s.rate=tr2_u32(p+at+12);s.bits=tr2_u16(p+at+22);}
  else if(!memcmp(p+at,"data",4)){s.data=p+at+8;s.frames=n;}
  at+=8+(size_t)n+(n&1);
 }
 if(!pcm || !s.rate || (s.channels!=1 && s.channels!=2) || (s.bits!=8 && s.bits!=16))return (Tr2Sample){0};
 s.frames/=(unsigned)(s.channels*s.bits/8);return s;
}
static void audio_close(void){
 if(audio_device){waveOutReset(audio_device);for(int i=0;i<TR2_BLOCKS;i++)if(audio_headers[i].dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(audio_device,audio_headers+i,sizeof(WAVEHDR));waveOutClose(audio_device);}
 audio_device=NULL;free(audio_bank);free(audio_music);free(audio_samples);audio_bank=audio_music=NULL;audio_samples=NULL;
 memset(audio_voices,0,sizeof audio_voices);memset(audio_headers,0,sizeof audio_headers);
}
static int audio_open(void){
 size_t size;audio_bank=asset_read("DATA/MAIN.SFX",&size);if(!audio_bank)return 0;
 audio_samples=calloc(level->sample_indices.count,sizeof *audio_samples);if(!audio_samples){audio_close();return 0;}
 unsigned ordinal=0;for(size_t at=0;at+12<=size;ordinal++){
  if(memcmp(audio_bank+at,"RIFF",4))break;size_t n=(size_t)tr2_u32(audio_bank+at+4)+8;if(n>size-at || n<12)break;
  for(unsigned i=0;i<level->sample_indices.count;i++)if(tr2_u32(level->sample_indices.data+i*4)==ordinal)audio_samples[i]=audio_wave(audio_bank+at,n);
  at+=n+(n&1);
 }
 WAVEFORMATEX format={WAVE_FORMAT_PCM,2,44100,176400,4,16,0};
 if(waveOutOpen(&audio_device,tomb_sound_output_device(),&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){audio_device=NULL;audio_close();return 0;}
 for(int i=0;i<TR2_BLOCKS;i++){
  audio_headers[i].lpData=(LPSTR)audio_buffers[i];audio_headers[i].dwBufferLength=sizeof audio_buffers[i];
  if(waveOutPrepareHeader(audio_device,audio_headers+i,sizeof(WAVEHDR))!=MMSYSERR_NOERROR){audio_close();return 0;}audio_headers[i].dwFlags|=WHDR_DONE;
 }return 1;
}
static int physical_track(int cue){const int omitted[]={2,19,20,26};int track=cue<2?2:cue;for(int i=0;i<4;i++)if(cue>omitted[i])track--;return track;}
static int audio_title(int cue){
 char name[80];snprintf(name,sizeof name,"AUDIO/track%02d.wav",physical_track(cue));size_t size;audio_music=asset_read(name,&size);if(!audio_music)return 0;
 Tr2Sample s=audio_wave(audio_music,size);if(!s.data)return 0;audio_voices[TR2_VOICES-1]=(Tr2Voice){s,0,32768,1};return 1;
}
static void menu_sound(int id){
 if(home_session){tr2_home_menu_sound(id);return;}
 if(!audio_samples || id<0 || id>=370)return;int detail=tr2_i16(level->sound_map+id*2);if(detail<0 || (unsigned)detail>=level->sound_details.count)return;
 const unsigned char *d=level->sound_details.data+detail*8;unsigned sample=tr2_u16(d);if(sample>=level->sample_indices.count)return;
 audio_voices[audio_slot++%(TR2_VOICES-1)]=(Tr2Voice){audio_samples[sample],0,tr2_u16(d+2),0};
}
static void audio_pump(void){
 if(!audio_device)return;
 for(int block=0;block<TR2_BLOCKS;block++)if(audio_headers[block].dwFlags&WHDR_DONE){
  for(int f=0;f<TR2_FRAMES;f++){int mix[2]={0};
   for(int i=0;i<TR2_VOICES;i++){Tr2Voice *v=audio_voices+i;Tr2Sample *s=&v->sample;if(!s->data || !s->frames)continue;
    if(v->position>=s->frames){if(v->loop)v->position=fmod(v->position,s->frames);else{s->data=NULL;continue;}}
    for(int c=0;c<2;c++){unsigned at=(unsigned)v->position*(unsigned)s->channels+(s->channels==2?(unsigned)c:0);int value=s->bits==16?tr2_i16(s->data+at*2):((int)s->data[at]-128)*256;
     mix[c]+=(int)((int64_t)value*v->volume*(i==TR2_VOICES-1?title_music:title_effects)/(32768*10));}
    v->position+=s->rate/44100.0;
   }for(int c=0;c<2;c++)audio_buffers[block][f*2+c]=(int16_t)clamp(mix[c],-32768,32767);
  }waveOutWrite(audio_device,audio_headers+block,sizeof(WAVEHDR));
 }
}
