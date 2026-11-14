#ifndef TR2_WATER_VISUAL_H
#define TR2_WATER_VISUAL_H
#include "tr2_runtime.h"
/* 44d840 builds 32 integer offsets; 402224 applies them to projected room
   vertices. The second lookup uses the already displaced x coordinate. */
static inline void tr2_water_wobble(float *x,float *y,int phase,int room_wet,int camera_wet,int flags){
 if(room_wet==camera_wet || (flags&0x8000))return;
 *x+=(float)tr2_asr(2*tr2_sin((((int)*y+phase)&31)*2048),14);
 *y+=(float)tr2_asr(2*tr2_sin((((int)*x+phase)&31)*2048),14);
}
#endif
