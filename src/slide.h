#ifndef TOMB_SLIDE_H
#define TOMB_SLIDE_H
#include "step.h"
typedef struct TombSlideContext {
    TombQuery query;
    void *user;
    int16_t move_angle, last_angle;
    int8_t tilt_x, tilt_z;
} TombSlideContext;
/* DOS 0x28128, 0x25a64/0x25b70 and 0x26cc8/0x26eb8. */
TOMB_EXPORT int tomb_slide_start(TombActor *,TombContact *,TombSlideContext *);
TOMB_EXPORT void tomb_slide_control(TombActor *,uint32_t input,int32_t *camera_mode,int16_t *camera_elevation);
TOMB_EXPORT void tomb_slide_collision(TombActor *,TombContact *,TombSlideContext *);
#endif
