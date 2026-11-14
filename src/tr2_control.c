#include "tr2_control.h"

static int16_t word(int n){unsigned v=(unsigned)n&65535;return (int16_t)(v<32768?(int)v:(int)v-65536);}

/* Recovered from supplied Tomb2.exe 0x427880–0x42790a. */
void tr2_control_walk(Tr2Control *s,uint32_t input){
    if(s->health<=0){s->goal=2;return;}
    if(input&TR2_LEFT){s->turn_rate=word(s->turn_rate-409);if(s->turn_rate< -728)s->turn_rate=-728;}
    else if(input&TR2_RIGHT){s->turn_rate=word(s->turn_rate+409);if(s->turn_rate>728)s->turn_rate=728;}
    if(!(input&TR2_FORWARD))s->goal=2;
    else if(s->water_status==4)s->goal=65;
    else s->goal=(input&TR2_WALK)?0:1;
}

/* Recovered from supplied Tomb2.exe 0x427910–0x427a52. */
void tr2_control_run(Tr2Control *s,uint32_t input,int16_t roll_frame_base){
    if(s->health<=0){s->goal=8;return;}
    if(input&TR2_ROLL){s->animation=146;s->frame=word(roll_frame_base+2);s->current=45;s->goal=2;return;}
    if(input&TR2_LEFT){
        s->turn_rate=word(s->turn_rate-409);if(s->turn_rate< -1456)s->turn_rate=-1456;
        s->roll=word(s->roll-273);if(s->roll< -2002)s->roll=-2002;
    }else if(input&TR2_RIGHT){
        s->turn_rate=word(s->turn_rate+409);if(s->turn_rate>1456)s->turn_rate=1456;
        s->roll=word(s->roll+273);if(s->roll>2002)s->roll=2002;
    }
    if(s->animation==6)s->run_jump_allowed=0;
    else if(s->animation!=0 || s->frame==4)s->run_jump_allowed=1;
    if((input&TR2_JUMP) && s->run_jump_allowed && !(s->flags&8)){s->goal=3;return;}
    if(!(input&TR2_FORWARD))s->goal=2;
    else if(s->water_status==4)s->goal=65;
    else s->goal=(input&TR2_WALK)?0:1;
}
