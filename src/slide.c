#include "slide.h"
#include "fixed.h"
#include <stdlib.h>
int tomb_slide_start(TombActor *a,TombContact *c,TombSlideContext *s) {
    int x=s->tilt_x,z=s->tilt_z;
    if(abs(x)<=2 && abs(z)<=2)return 0;
    int16_t angle=x>2?-16384:x< -2?16384:0;
    if(z>2 && z>abs(x))angle=-32768;
    else if(z< -2 && -z>abs(x))angle=0;
    int16_t diff=tomb_word((int)angle-a->yaw);
    tomb_contact_shift(a,c);
    int forward=diff>=-16384 && diff<=16384;
    int state=forward?24:32;
    if(a->current!=state || s->last_angle!=angle) {
        a->animation=forward?70:104;a->frame=forward?1133:1677;
        a->current=a->goal=(int16_t)state;
        s->move_angle=s->last_angle=angle;
        a->yaw=forward?angle:tomb_word((int)angle-32768);
    }
    return 1;
}
void tomb_slide_control(TombActor *a,uint32_t input,int32_t *camera_mode,int16_t *camera_elevation) {
    if(a->current==24){*camera_mode=2;*camera_elevation=-8190;}
    if(input&16)a->goal=a->current==24?3:25;
}
void tomb_slide_collision(TombActor *a,TombContact *c,TombSlideContext *s) {
    s->move_angle=tomb_word((int)a->yaw+(a->current==32?-32768:0));
    c->positive_limit=32512;c->negative_limit=-512;c->ceiling_limit=0;c->facing=s->move_angle;
    s->query(s->user,a,c,762);
    if(tomb_ceiling_response(a,c))return;
    tomb_wall_response(a,c);
    if(c->floor>200) {
        int forward=a->current==24;
        a->animation=forward?34:93;a->frame=forward?492:1473;
        a->current=a->goal=forward?3:29;a->fall_speed=0;a->flags|=8;
        return;
    }
    tomb_slide_start(a,c,s);
    a->y=tomb_long((int64_t)a->y+c->floor);
    if(abs(s->tilt_x)<=2 && abs(s->tilt_z)<=2)a->goal=2;
}
