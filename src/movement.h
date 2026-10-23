#ifndef TOMB_MOVEMENT_H
#define TOMB_MOVEMENT_H

#include <stdint.h>

/* Values observed in the original DOS control-state table. */
enum TombControl {
    TOMB_WALK = 0, TOMB_RUN = 1, TOMB_STOP = 2,
    TOMB_HOP_BACK = 5, TOMB_TURN_RIGHT = 6, TOMB_TURN_LEFT = 7, TOMB_SPLAT = 12, TOMB_WALK_BACK = 16, TOMB_FAST_TURN = 20, TOMB_STEP_RIGHT = 21, TOMB_STEP_LEFT = 22
};

/* A semantic view, not a packed overlay on the original DOS memory. */
typedef struct TombMovement {
    uint32_t input;
    int16_t health, current_state, goal_state, animation, frame;
    int16_t turn_rate, lean;
    uint8_t item_flags;
    int16_t weapon_status;
    int16_t head_yaw, head_pitch, torso_yaw, torso_pitch;
    uint8_t camera_mode;
} TombMovement;

/* One control-handler invocation. Does not advance animation, apply damping,
   translate position or resolve collision. Returns 0 for unsupported handlers,
   leaving state unchanged. */
int tomb_movement_control(enum TombControl handler, TombMovement *state);

#endif
