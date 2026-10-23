#ifndef TOMB_SOUND_WIN_H
#define TOMB_SOUND_WIN_H
#include "sound.h"
#include <stddef.h>
/* Select a runtime-supplied Windows endpoint, or NULL for the desktop default.
   A missing endpoint falls back to the default device. Existing outputs must
   be reopened after a route change. */
int tomb_sound_output_route(const wchar_t *endpoint);
unsigned tomb_sound_output_device(void);
typedef struct TombSoundOutput TombSoundOutput;
typedef struct TombSoundOutputStats { unsigned blocks,starvations,played_samples; } TombSoundOutputStats;
TombSoundOutput *tomb_sound_output_open(TombSoundMixer *);
int tomb_sound_output_update(TombSoundOutput *,int paused);
/* All mixer access while the output is open must pass through this lock. */
void tomb_sound_output_event(TombSoundOutput *,const TombSoundEvent *,int camera_wet,const double listener[3]);
void tomb_sound_output_frame(TombSoundOutput *,const TombSoundEvent *,size_t count,int flipped,int camera_wet,const double listener[3]);
void tomb_sound_output_spatial_frame(TombSoundOutput *,const TombSoundEvent *,size_t,int,int,const double listener[3],const double origin[3],int yaw);
TombSoundOutputStats tomb_sound_output_stats(TombSoundOutput *);
void tomb_sound_output_reset(TombSoundOutput *);
void tomb_sound_output_close(TombSoundOutput *);
void tomb_sound_output_volume(TombSoundOutput *,int);
#endif
