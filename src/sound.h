#ifndef TOMB_SOUND_H
#define TOMB_SOUND_H
#include "level.h"
#define TOMB_SOUND_CHANNELS 24
#define TOMB_SOUND_RATE 44100
#define TOMB_SOUND_EVENTS 32
typedef struct TombSoundDetail { uint16_t sample,volume,chance,flags; } TombSoundDetail;
typedef struct TombSoundPlan { int sample,volume,pitch,mode; } TombSoundPlan;
/* source identifies persistent XYZ storage, read only on the gameplay thread
   during frame updates. NULL uses the XYZ snapshot (kind 9 is non-positional).
   Clear/close the mixer before releasing any source storage. */
typedef struct TombSoundEvent { int kind,id,environment; int32_t x,y,z; const int32_t *source; } TombSoundEvent;
typedef struct TombSoundSample { const unsigned char *pcm; size_t frames; unsigned rate,bits,channels; } TombSoundSample;
typedef struct TombSoundBank { int16_t map[256]; TombSoundDetail *details; size_t detail_count; TombSoundSample *samples; size_t sample_count; const unsigned char *sources; size_t source_count; } TombSoundBank;
typedef struct TombSoundVoice { int active,id,sample,volume,pitch,mode; double position; const int32_t *source; int32_t xyz[3]; int positional,effect,angle; } TombSoundVoice;
typedef struct TombSoundMixer { const TombSoundBank *bank; uint32_t random; TombSoundVoice voices[TOMB_SOUND_CHANNELS]; unsigned played,dropped; int loop_distance[TOMB_SOUND_CHANNELS]; double origin[3]; int yaw; } TombSoundMixer;
TOMB_EXPORT unsigned tomb_sound_random(uint32_t *);
/* DOS sample, volume, pitch, chance and environment rules; modern output below. */
TOMB_EXPORT int tomb_sound_plan(const TombSoundDetail *,int distance,int environment,int camera_wet,uint32_t *,TombSoundPlan *);
int tomb_sound_bank_load(TombSoundBank *,const TombLevel *);
void tomb_sound_bank_free(TombSoundBank *);
void tomb_sound_init(TombSoundMixer *,const TombSoundBank *);
void tomb_sound_clear(TombSoundMixer *);
void tomb_sound_event(TombSoundMixer *,const TombSoundEvent *,int camera_wet,const double listener[3]);
void tomb_sound_frame(TombSoundMixer *,const TombSoundEvent *,size_t count,int flipped,int camera_wet,const double listener[3]);
void tomb_sound_mix(TombSoundMixer *,int16_t *stereo,size_t frames);
TOMB_EXPORT int tomb_sound_pan(int angle);
TOMB_EXPORT void tomb_sound_gains(int angle,int volume,int gains[2]);
void tomb_sound_listener(TombSoundMixer *,const double origin[3],int yaw);
int tomb_sound_sample(TombSoundSample *,const unsigned char *,size_t);
#endif
