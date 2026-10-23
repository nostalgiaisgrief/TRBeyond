/* PickupCollision 0x33e80; Test/Align/MoveLaraPosition 0x168b0..0x16d10. */
#include "inventory.h"
#include "object_contact.h"
#include "fixed.h"
#include <math.h>
#include <stdlib.h>
static int16_t approach(int16_t a,int16_t b){int d=tomb_word(b-a);return tomb_word(a+(d>364?364:d< -364?-364:d));}
int tomb_pickup_contact(TombObject *o,TombActor *a,TombAnimContext *ctx,uint32_t input,int water,int16_t *pitch,int16_t *roll) {
    if(!tomb_pickup_object(o->object) || (o->actor.flags&4) || (water!=0 && water!=1))return 0;
    o->actor.yaw=a->yaw;int16_t op=water?-4550:0;
    int dp=tomb_word(*pitch-op),limit=water?8190:1820;
    if(dp< -limit || dp>limit || *roll<(water?-8190:0) || *roll>(water?8190:0))return 0;
    int32_t delta[3]={a->x-o->actor.x,a->y-o->actor.y,a->z-o->actor.z},local[3];
    tomb_rotate_vector(o->actor.yaw,op,0,delta,1,ctx->sine_quarter,local);
    if(water){for(int j=0;j<3;j++)if(local[j]< -512 || local[j]>512)return 0;}
    else if(local[0]< -256 || local[0]>256 || local[1]< -100 || local[1]>100 || local[2]< -256 || local[2]>100)return 0;
    if(a->current==39) {
        if(a->frame!=(water?2970:3443))return 0;
        o->actor.flags|=6;o->active=0;return 2;
    }
    if(!(input&64) || a->current!=(water?13:2) || (!water && (ctx->weapon_status || (a->flags&8))))return 0;
    const int32_t offset[3]={0,water?-200:0,water?-350:-100};int32_t pos[3];
    tomb_rotate_vector(o->actor.yaw,op,0,offset,0,ctx->sine_quarter,pos);
    pos[0]+=o->actor.x;pos[1]+=o->actor.y;pos[2]+=o->actor.z;
    if(water) {
        int dx=pos[0]-a->x,dy=pos[1]-a->y,dz=pos[2]-a->z;
        int distance=(int)sqrt((double)((int64_t)dx*dx+(int64_t)dy*dy+(int64_t)dz*dz));
        if(distance>16){a->x+=dx*16/distance;a->y+=dy*16/distance;a->z+=dz*16/distance;}else{a->x=pos[0];a->y=pos[1];a->z=pos[2];}
        *pitch=approach(*pitch,op);*roll=approach(*roll,0);
        if(a->x!=pos[0] || a->y!=pos[1] || a->z!=pos[2] || *pitch!=op || *roll)return 0;
    } else {a->x=pos[0];a->y=pos[1];a->z=pos[2];*pitch=0;*roll=0;}
    a->goal=39;int budget=120;while(a->current!=39 && budget--)if(!tomb_animate(a,ctx))return -1;
    if(a->current!=39)return -1;
    a->goal=water?13:2;if(!water)ctx->weapon_status=1;return 1;
}
