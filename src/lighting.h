#ifndef TOMB_LIGHTING_H
#define TOMB_LIGHTING_H
#include "visual.h"
typedef struct TombLighting { int32_t adder,divider,direction[3]; } TombLighting;
/* Direction is world space. Matrices below are row-major fixed point (16384). */
TOMB_EXPORT void tomb_light_direction(const int32_t delta[3],const int16_t *sine,int32_t out[3]);
TOMB_EXPORT void tomb_light_room(const TombRoomLights *,const int32_t position[3],int depth,const int16_t *sine,TombLighting *);
TOMB_EXPORT void tomb_light_static(int16_t intensity,int depth,TombLighting *);
/* Room vertex depth shading before water modulation and final shade clamp. */
TOMB_EXPORT int tomb_light_room_depth(int16_t shade,int depth);
TOMB_EXPORT void tomb_light_vector(const TombLighting *,const int32_t rotation[9],int32_t local[3]);
TOMB_EXPORT int tomb_light_vertex(const TombMeshView *,size_t vertex,const TombLighting *,const int32_t local[3]);
#endif
