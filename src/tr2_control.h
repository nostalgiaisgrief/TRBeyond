#ifndef TR2_CONTROL_H
#define TR2_CONTROL_H
#include <stdint.h>

enum Tr2Input {
    TR2_FORWARD=1,TR2_BACK=2,TR2_LEFT=4,TR2_RIGHT=8,TR2_JUMP=16,
    TR2_ACTION=64,TR2_WALK=128,TR2_LOOK=512,TR2_STEP_LEFT=1024,
    TR2_STEP_RIGHT=2048,TR2_ROLL=4096,TR2_DRAW_SELECTED=1u<<20
};
typedef struct {
    int16_t health,current,goal,animation,frame,roll,turn_rate,water_status;
    uint16_t flags;
    int32_t run_jump_allowed;
} Tr2Control;

/* State decisions only. Animation advancement and collision are separate. */
void tr2_control_walk(Tr2Control *state,uint32_t input);
void tr2_control_run(Tr2Control *state,uint32_t input,int16_t roll_frame_base);
#endif
