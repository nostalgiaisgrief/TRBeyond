/* Reconstructed from the user's DOS executable, SHA-256 99503b7c...ffe0fac.
   See analysis/movement-findings.md for evidence and analysis addresses.
   No TRX function bodies are used here. */
#include "movement.h"

enum Input {
    FORWARD = 1, BACK = 2, LEFT = 4, RIGHT = 8, JUMP = 0x10,
    SLOW = 0x80, LOOK = 0x200, STEP_LEFT = 0x400,
    STEP_RIGHT = 0x800, ROLL = 0x1000
};

/* Original x86 word arithmetic wraps BEFORE the signed clamp/comparison.
   Convert only representable signed values, avoiding implementation-defined
   unsigned-to-signed narrowing and undefined signed overflow. */
static int16_t word_wrap(int32_t value)
{
    uint32_t low = (uint32_t)value & UINT32_C(65535);
    int32_t signed_value = low < 32768 ? (int32_t)low : (int32_t)low - 65536;
    return (int16_t)signed_value;
}

static void steer(TombMovement *s, int16_t limit)
{
    if (s->input & LEFT) {
        s->turn_rate = word_wrap((int32_t)s->turn_rate - 409);
        if (s->turn_rate < -limit) s->turn_rate = (int16_t)-limit;
    } else if (s->input & RIGHT) {
        s->turn_rate = word_wrap((int32_t)s->turn_rate + 409);
        if (s->turn_rate > limit) s->turn_rate = limit;
    }
}

static void roll(TombMovement *s)
{
    s->animation = 146;
    s->frame = 3857;
    s->current_state = 45;
    s->goal_state = TOMB_STOP;
}

/* 0x2515C */
static void walk(TombMovement *s)
{
    if (s->health <= 0) { s->goal_state = TOMB_STOP; return; }
    steer(s, 728);
    s->goal_state = !(s->input & FORWARD) ? TOMB_STOP
        : (s->input & SLOW) ? TOMB_WALK : TOMB_RUN;
}

/* 0x251EC */
static void run(TombMovement *s)
{
    if (s->health <= 0) { s->goal_state = 8; return; }
    if (s->input & ROLL) { roll(s); return; }
    steer(s, 1456);
    if (s->input & LEFT) {
        s->lean = word_wrap((int32_t)s->lean - 273);
        if (s->lean < -2002) s->lean = -2002;
    } else if (s->input & RIGHT) {
        s->lean = word_wrap((int32_t)s->lean + 273);
        if (s->lean > 2002) s->lean = 2002;
    }
    if ((s->input & JUMP) && !(s->item_flags & 8)) {
        s->goal_state = 3;
    } else {
        s->goal_state = !(s->input & FORWARD) ? TOMB_STOP
            : (s->input & SLOW) ? TOMB_WALK : TOMB_RUN;
    }
}

/* 0x258A0: also called by standing when back+slow is held. */
static void walk_back(TombMovement *s)
{
    if (s->health <= 0) { s->goal_state = TOMB_STOP; return; }
    s->goal_state = (s->input & BACK) && (s->input & SLOW)
        ? TOMB_WALK_BACK : TOMB_STOP;
    steer(s, 728);
}

/* 0x252F8 */
static void stop(TombMovement *s)
{
    if (s->health <= 0) { s->goal_state = 8; return; }
    if (s->input & ROLL) { roll(s); return; }
    s->goal_state = TOMB_STOP;
    if (s->input & LOOK) {
        s->camera_mode = 2;
        /* Check before changing the angle; do not replace with a post-clamp.
           At a limit, the opposite direction can win if both are pressed. */
        if ((s->input & LEFT) && s->head_yaw > -8008)
            s->head_yaw = word_wrap((int32_t)s->head_yaw - 364);
        else if ((s->input & RIGHT) && s->head_yaw < 8008)
            s->head_yaw = word_wrap((int32_t)s->head_yaw + 364);
        s->torso_yaw = s->head_yaw;
        if ((s->input & FORWARD) && s->head_pitch > -7644)
            s->head_pitch = word_wrap((int32_t)s->head_pitch - 364);
        else if ((s->input & BACK) && s->head_pitch < 4004)
            s->head_pitch = word_wrap((int32_t)s->head_pitch + 364);
        s->torso_pitch = s->head_pitch;
        return;
    }
    if (s->camera_mode == 2) s->camera_mode = 0;
    if (s->input & STEP_LEFT) s->goal_state = 22;
    else if (s->input & STEP_RIGHT) s->goal_state = 21;
    if (s->input & LEFT) s->goal_state = TOMB_TURN_LEFT;
    else if (s->input & RIGHT) s->goal_state = TOMB_TURN_RIGHT;
    if (s->input & JUMP) { s->goal_state = 15; return; }
    if (s->input & FORWARD) {
        if (s->input & SLOW) walk(s);
        else run(s);
    } else if (s->input & BACK) {
        if (s->input & SLOW) walk_back(s);
        else s->goal_state = 5;
    }
}

/* 0x255BC (right), 0x25644 (left). Preserve the old goal when no
   branch replaces it; a handler invocation is not an animation transition. */
static void turn(TombMovement *s, int right)
{
    if (s->health <= 0) { s->goal_state = TOMB_STOP; return; }
    s->turn_rate = word_wrap((int32_t)s->turn_rate + (right ? 409 : -409));
    if (s->weapon_status == 4) {
        s->goal_state = 20;
    } else if (right ? s->turn_rate > 728 : s->turn_rate < -728) {
        if (s->input & SLOW) s->turn_rate = right ? 728 : -728;
        else s->goal_state = 20;
    }
    if (s->input & FORWARD)
        s->goal_state = (s->input & SLOW) ? TOMB_WALK : TOMB_RUN;
    else if (!(s->input & (right ? RIGHT : LEFT)))
        s->goal_state = TOMB_STOP;
}

int tomb_movement_control(enum TombControl handler, TombMovement *state)
{
    if (!state) return 0;
    switch (handler) {
    case TOMB_HOP_BACK: state->goal_state=TOMB_STOP; steer(state,1092); break; /* 0x2555c */
    case TOMB_SPLAT: break; /* DOS 0x25798 is an immediate return. */
    case TOMB_FAST_TURN:
        if(state->health<=0) state->goal_state=TOMB_STOP;
        else {
            int left=state->turn_rate<0;
            state->turn_rate=left?-1456:1456;
            if(!(state->input & (left?LEFT:RIGHT))) state->goal_state=TOMB_STOP;
        }
        break;
    case TOMB_STEP_RIGHT: case TOMB_STEP_LEFT:
        if(state->health<=0)state->goal_state=TOMB_STOP;
        else {if(!(state->input&(handler==TOMB_STEP_RIGHT?STEP_RIGHT:STEP_LEFT)))state->goal_state=TOMB_STOP;steer(state,728);}
        break;
    case TOMB_WALK: walk(state); break;
    case TOMB_RUN: run(state); break;
    case TOMB_STOP: stop(state); break;
    case TOMB_TURN_RIGHT: turn(state, 1); break;
    case TOMB_TURN_LEFT: turn(state, 0); break;
    case TOMB_WALK_BACK: walk_back(state); break;
    default: return 0;
    }
    return 1;
}
