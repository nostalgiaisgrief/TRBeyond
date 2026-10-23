#ifndef TOMB_CINEMATIC_H
#define TOMB_CINEMATIC_H
#include "visual.h"
#include "objects.h"
#include "dos_camera.h"
typedef struct TombCinematic { const unsigned char *frames; unsigned count,frame; TombActor origin; } TombCinematic;
TOMB_EXPORT int tomb_cinematic_init(TombCinematic *,TombObjects *);
TOMB_EXPORT int tomb_cinematic_init_sequence(TombCinematic *,TombObjects *,int);
TOMB_EXPORT int tomb_cinematic_camera(const TombCinematic *,const int16_t *,TombDosCamera *,int *,int *);
TOMB_EXPORT int tomb_cinematic_tick(TombCinematic *,TombObjects *,TombAnimContext *);
#endif
