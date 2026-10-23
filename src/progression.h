#ifndef TOMB_PROGRESSION_H
#define TOMB_PROGRESSION_H
#include "gym.h"
typedef struct TombProgress {
    TombGym music;
    uint16_t secrets;
    unsigned secret_serial;
    int complete,lara_state;
} TombProgress;
TOMB_EXPORT void tomb_progress_action(TombProgress *,int action,int value,uint16_t flags,int type);
/* Physical DOS playback mapping: -1 ignored, 0 stop, >0 CD track,
   -2 embedded effect 173 (secret), -3 embedded track+148 sound. */
TOMB_EXPORT int tomb_music_source(int track);
/* DOS level-start music table at 0xc3e0c, gameplay level indices 0..15. */
TOMB_EXPORT int tomb_music_level_track(int level);
#endif
