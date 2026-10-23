#ifndef TOMB_OBJECTS_H
#define TOMB_OBJECTS_H
#include "visual.h"
typedef struct TombObject {
    TombActor actor;
    uint16_t flags;
    int16_t timer, object;
    int active;
} TombObject;
typedef struct TombDoorPart {
    TombSectorRef ref;
    TombSector saved;
    int box;
} TombDoorPart;
typedef struct TombDoor {
    TombDoorPart parts[4];
    unsigned count;
    int open;
} TombDoor;
typedef struct TombObjects {
    TombLevel *level;
    const TombVisual *visual;
    TombObject *items;
    TombDoor *doors;
    uint16_t *box_overlap;
    size_t count;
    unsigned deferred_actions;
    /* active follows DOS camera types: 0 chase, 1 fixed, 5 heavy. */
    struct { int index,last,target,timer,speed,active; } camera;
    uint8_t camera_once[128]; /* Camera action IDs are ten bits. */
    struct TombHazards *hazards;
    struct TombEnemies *enemies;
    struct TombProgress *progress;
    int last_target;
    struct TombPeru *peru;
    struct TombNavigation *current;
    int16_t current_strength;
    int invincible; /* Derived from the active game's custom options. */
    size_t source_count; /* Original trigger indices precede reserved spawned items. */
    int no_enemies; /* Derived from gameplay options; Scion remains interactive. */
} TombObjects;
int tomb_objects_spawn(TombObjects *,int,const TombActor *);
TOMB_EXPORT int tomb_object_supported(int);
TOMB_EXPORT int tomb_trigger_active(TombObject *);
TOMB_EXPORT void tomb_object_trigger(TombObject *,int type,uint16_t flags);
TOMB_EXPORT int tomb_switch_trigger(TombObject *,int16_t timer);
/* -1 leaves sector data alone, otherwise 0 closes and 1 opens it. */
TOMB_EXPORT void tomb_trapdoor_control(TombObject *);
TOMB_EXPORT int tomb_key_trigger(TombObject *,int);
TOMB_EXPORT int tomb_door_control(TombObject *);
/* Late-level object state decisions: bit 1 removes, bit 2 flips the cabin map. */
TOMB_EXPORT int tomb_late_object_control(TombObject *);
TOMB_EXPORT int tomb_object_animate(TombObject *,TombAnimContext *);
TOMB_EXPORT int tomb_object_animate_required(TombObject *,TombAnimContext *,int16_t *);
TOMB_EXPORT int tomb_switch_bounds(const TombActor *,const TombActor *,int16_t pitch,int16_t roll);
TOMB_EXPORT int tomb_objects_init(TombObjects *,TombLevel *,const TombVisual *);
TOMB_EXPORT void tomb_objects_reset(TombObjects *);
TOMB_EXPORT void tomb_objects_free(TombObjects *);
TOMB_EXPORT int tomb_objects_tick(TombObjects *,TombAnimContext *);
TOMB_EXPORT void tomb_objects_camera_begin(TombObjects *);
int tomb_objects_interact(TombObjects *,TombActor *,TombAnimContext *,uint32_t input,int16_t pitch,int16_t roll);
TOMB_EXPORT int tomb_objects_trigger_keys(TombObjects *,size_t,int,int);
int tomb_objects_trigger_activated(TombObjects *,size_t);
TOMB_EXPORT int tomb_objects_trigger_camera(TombObjects *,size_t,int,int,int camera_mode);
TOMB_EXPORT int tomb_objects_trigger_heavy(TombObjects *,size_t);
TOMB_EXPORT int tomb_objects_trigger(TombObjects *,size_t trigger,int grounded);
TOMB_EXPORT void tomb_current_reset(TombObjects *);
#endif
