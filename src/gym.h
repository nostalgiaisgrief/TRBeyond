#ifndef TOMB_GYM_H
#define TOMB_GYM_H
#include "level.h"
typedef struct TombGym {
    uint16_t tracks[64];
    int current_track,completion_ticks,complete;
    unsigned audio_serial;
    int camera,camera_timer,camera_speed,last_camera;
    uint64_t camera_once;
    int last_music_trigger; /* PS1 floor-trigger history, independent of CD requests. */
} TombGym;
TOMB_EXPORT void tomb_gym_audio(TombGym *,int track,uint16_t flags,int type,int lara_state);
void tomb_gym_audio_source(TombGym *,int,uint16_t,int,int,int ps1);
/* Ordinary gym trigger lists. Validation is atomic; unsupported actions fail. */
int tomb_gym_trigger(TombGym *,const TombLevel *,size_t index,int lara_state);
void tomb_gym_begin_tick(TombGym *);
#endif
