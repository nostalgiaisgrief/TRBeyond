#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <mmddk.h>
#include <wchar.h>
#include <avrt.h>
#include <stdlib.h>
#include "sound_win.h"
#define BLOCKS 3
#define FRAMES 1024
static unsigned output_device=WAVE_MAPPER;
unsigned tomb_sound_output_device(void){return output_device;}
int tomb_sound_output_route(const wchar_t *endpoint){
    output_device=WAVE_MAPPER;if(!endpoint || !*endpoint)return 1;
    for(UINT i=0;i<waveOutGetNumDevs();i++){
        ULONG bytes=0;HWAVEOUT device=(HWAVEOUT)(UINT_PTR)i;
        if(waveOutMessage(device,DRV_QUERYFUNCTIONINSTANCEIDSIZE,(DWORD_PTR)&bytes,0)!=MMSYSERR_NOERROR || bytes<sizeof(wchar_t) || bytes>65536)continue;
        wchar_t *id=calloc(1,bytes+sizeof(wchar_t));if(!id)return 0;
        int match=waveOutMessage(device,DRV_QUERYFUNCTIONINSTANCEID,(DWORD_PTR)id,bytes)==MMSYSERR_NOERROR && wcscmp(id,endpoint)==0;
        free(id);if(match){output_device=i;return 1;}
    }
    return 0;
}
struct TombSoundOutput {
    HWAVEOUT device; TombSoundMixer *mixer;
    WAVEHDR header[BLOCKS]; int16_t pcm[BLOCKS][FRAMES*2];
    CRITICAL_SECTION lock; HANDLE ready,stop,thread;
    int paused,failed,primed,volume;unsigned prepared;
    TombSoundOutputStats stats;
};
static DWORD WINAPI pump(void *user) {
    TombSoundOutput *o=user;HANDLE events[2]={o->stop,o->ready};
    DWORD task=0;HANDLE scheduling=AvSetMmThreadCharacteristicsW(L"Audio",&task);
    while(WaitForMultipleObjects(2,events,FALSE,INFINITE)==WAIT_OBJECT_0+1) {
        EnterCriticalSection(&o->lock);
        if(!o->paused && !o->failed) {
            unsigned queued=0;for(int i=0;i<BLOCKS;i++)if(o->header[i].dwFlags&WHDR_INQUEUE)queued++;
            if(o->primed && !queued)o->stats.starvations++;
            for(int i=0;i<BLOCKS;i++)if(!(o->header[i].dwFlags&WHDR_INQUEUE)) {
                tomb_sound_mix(o->mixer,o->pcm[i],FRAMES);
                for(int s=0;s<FRAMES*2;s++)o->pcm[i][s]=(int16_t)(o->pcm[i][s]*o->volume/10);
                if(waveOutWrite(o->device,o->header+i,sizeof(WAVEHDR))!=MMSYSERR_NOERROR){o->failed=1;break;}
                o->stats.blocks++;o->primed=1;
            }
        }
        LeaveCriticalSection(&o->lock);
    }
    if(scheduling)AvRevertMmThreadCharacteristics(scheduling);
    return 0;
}
TombSoundOutput *tomb_sound_output_open(TombSoundMixer *m) {
    TombSoundOutput *o=calloc(1,sizeof *o);if(!o)return NULL;o->mixer=m;o->volume=10;
    InitializeCriticalSection(&o->lock);
    o->ready=CreateEventW(NULL,FALSE,FALSE,NULL);o->stop=CreateEventW(NULL,TRUE,FALSE,NULL);
    if(!o->ready || !o->stop){tomb_sound_output_close(o);return NULL;}
    WAVEFORMATEX fmt={WAVE_FORMAT_PCM,2,TOMB_SOUND_RATE,TOMB_SOUND_RATE*4,4,16,0};
    if(waveOutOpen(&o->device,output_device,&fmt,(DWORD_PTR)o->ready,0,CALLBACK_EVENT)!=MMSYSERR_NOERROR){
        o->device=NULL;
        if(output_device==WAVE_MAPPER || waveOutOpen(&o->device,WAVE_MAPPER,&fmt,(DWORD_PTR)o->ready,0,CALLBACK_EVENT)!=MMSYSERR_NOERROR){o->device=NULL;tomb_sound_output_close(o);return NULL;}
    }
    for(int i=0;i<BLOCKS;i++) {
        o->header[i].lpData=(LPSTR)o->pcm[i];o->header[i].dwBufferLength=sizeof o->pcm[i];
        if(waveOutPrepareHeader(o->device,o->header+i,sizeof(WAVEHDR))!=MMSYSERR_NOERROR){tomb_sound_output_close(o);return NULL;}
        o->prepared++;
    }
    o->thread=CreateThread(NULL,0,pump,o,0,NULL);
    if(!o->thread){tomb_sound_output_close(o);return NULL;}
    SetEvent(o->ready);return o;
}
int tomb_sound_output_update(TombSoundOutput *o,int paused) {
    if(!o)return 0;EnterCriticalSection(&o->lock);
    if(paused!=o->paused) {
        MMRESULT r=paused?waveOutPause(o->device):waveOutRestart(o->device);
        if(r!=MMSYSERR_NOERROR)o->failed=1;
        o->paused=paused;if(!paused)SetEvent(o->ready);
    }
    int ok=!o->failed;LeaveCriticalSection(&o->lock);return ok;
}
void tomb_sound_output_event(TombSoundOutput *o,const TombSoundEvent *event,int wet,const double listener[3]) {
    if(!o)return;EnterCriticalSection(&o->lock);
    tomb_sound_event(o->mixer,event,wet,listener);
    LeaveCriticalSection(&o->lock);
}
void tomb_sound_output_frame(TombSoundOutput *o,const TombSoundEvent *events,size_t count,int flipped,int wet,const double listener[3]){
    if(!o)return;EnterCriticalSection(&o->lock);
    tomb_sound_frame(o->mixer,events,count,flipped,wet,listener);
    LeaveCriticalSection(&o->lock);
}
void tomb_sound_output_spatial_frame(TombSoundOutput *o,const TombSoundEvent *events,size_t count,int flipped,int wet,const double listener[3],const double origin[3],int yaw){
    if(!o)return;EnterCriticalSection(&o->lock);
    tomb_sound_listener(o->mixer,origin,yaw);
    tomb_sound_frame(o->mixer,events,count,flipped,wet,listener);
    LeaveCriticalSection(&o->lock);
}
TombSoundOutputStats tomb_sound_output_stats(TombSoundOutput *o) {
    TombSoundOutputStats result={0};if(!o)return result;
    EnterCriticalSection(&o->lock);result=o->stats;MMTIME t={0};t.wType=TIME_SAMPLES;if(waveOutGetPosition(o->device,&t,sizeof t)==MMSYSERR_NOERROR && t.wType==TIME_SAMPLES)result.played_samples=t.u.sample;LeaveCriticalSection(&o->lock);return result;
}
void tomb_sound_output_reset(TombSoundOutput *o) {
    if(!o)return;EnterCriticalSection(&o->lock);
    if(waveOutReset(o->device)!=MMSYSERR_NOERROR)o->failed=1;
    if(o->paused && waveOutPause(o->device)!=MMSYSERR_NOERROR)o->failed=1;
    tomb_sound_clear(o->mixer);o->primed=0;SetEvent(o->ready);LeaveCriticalSection(&o->lock);
}
void tomb_sound_output_close(TombSoundOutput *o) {
    if(!o)return;
    if(o->thread){SetEvent(o->stop);WaitForSingleObject(o->thread,INFINITE);CloseHandle(o->thread);}
    if(o->device){waveOutReset(o->device);for(unsigned i=0;i<o->prepared;i++)waveOutUnprepareHeader(o->device,o->header+i,sizeof(WAVEHDR));waveOutClose(o->device);}
    if(o->ready)CloseHandle(o->ready);if(o->stop)CloseHandle(o->stop);
    DeleteCriticalSection(&o->lock);free(o);
}

void tomb_sound_output_volume(TombSoundOutput *o,int volume){if(!o)return;EnterCriticalSection(&o->lock);o->volume=volume<0?0:volume>10?10:volume;LeaveCriticalSection(&o->lock);}
