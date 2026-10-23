#ifndef TOMB_STEP_H
#define TOMB_STEP_H
#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
#define TOMB_EXPORT __declspec(dllexport)
#else
#define TOMB_EXPORT
#endif

/* Semantic types: no overlays on DOS structs. */
typedef struct TombActor {
    int32_t x, y, z;
    int16_t current, goal, animation, frame, speed, fall_speed, yaw, room;
    uint8_t flags;
} TombActor;
typedef struct TombAnim {
    int32_t velocity, acceleration;
    int16_t state, first_frame, last_frame, next_animation, next_frame;
    int16_t change_count, change_index, command_count, command_index;
} TombAnim;
typedef struct TombChange { int16_t goal, count, index; } TombChange;
typedef struct TombRange { int16_t first, last, animation, frame; } TombRange;
/* Sound event kind 5, effect kind 6. Effects may alter the actor synchronously. */
typedef void (*TombAnimEvent)(void *, int, int16_t, TombActor *);
typedef struct TombAnimContext {
    const TombAnim *animations;
    size_t animation_count;
    const TombChange *changes;
    size_t change_count;
    const TombRange *ranges;
    size_t range_count;
    const int16_t *commands;
    size_t command_count;
    const int16_t *sine_quarter; /* Exactly 1025 entries, recovered from original data. */
    int16_t move_angle, fall_override, weapon_status;
    TombAnimEvent event;
    void *user;
} TombAnimContext;

/* Advance one Lara animation invocation (0x28820). Returns 0 for invalid data
   or unavailable event service. On failure, state may already be changed.
   Control dispatch and its turn damping happen outside this function. */
TOMB_EXPORT int tomb_animate(TombActor *, TombAnimContext *);

typedef struct TombContact {
    int32_t floor, positive_limit, negative_limit, ceiling_limit;
    int32_t shift_x, shift_y, shift_z, old_x, old_y, old_z;
    int16_t facing, type;
    uint8_t flags;
} TombContact;
typedef void (*TombQuery)(void *, TombActor *, TombContact *, int32_t height);
typedef int (*TombContactAction)(void *, TombActor *, TombContact *);
typedef struct TombWalkContext {
    TombQuery query;
    TombContactAction vault, slide;
    void *user;
    int16_t move_angle;
} TombWalkContext;

TOMB_EXPORT void tomb_contact_shift(TombActor *, TombContact *);
TOMB_EXPORT int tomb_ceiling_response(TombActor *, const TombContact *);
TOMB_EXPORT int tomb_wall_response(TombActor *, TombContact *);
/* Walking collision orchestration (0x26084). All three services are required;
   room sampling, vaulting and sliding are deliberately not stubbed in the core. */
TOMB_EXPORT int tomb_walk_collision(TombActor *, TombContact *, TombWalkContext *);
#endif
