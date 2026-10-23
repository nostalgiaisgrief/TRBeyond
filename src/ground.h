#ifndef TOMB_GROUND_H
#define TOMB_GROUND_H
#include "movement.h"
#include "step.h"
typedef struct TombGroundContext {
    TombWalkContext walk;
    int32_t front_floor,front_type;
    int16_t lean;
} TombGroundContext;
TOMB_EXPORT void tomb_surface_look(TombMovement *);
TOMB_EXPORT void tomb_look_relax(TombMovement *);
TOMB_EXPORT void tomb_ground_damping(TombMovement *,TombActor *);
/* Ground collision dispatch including wall-hit and backward-walk reactions;
   forward walk uses the existing routine. */
TOMB_EXPORT int tomb_ground_collision(int,TombActor *,TombContact *,TombGroundContext *);
#endif
