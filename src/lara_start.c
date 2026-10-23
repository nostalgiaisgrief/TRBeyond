#include "lara_start.h"
#include "fixed.h"
int tomb_lara_start(const TombLevel *l,TombLaraStart *out)
{
    if(!l || !out || !l->items || !l->rooms || !l->animations) return 0;
    const TombItem *item=NULL; size_t index=0;
    for(size_t i=0;i<l->item_count;++i) if(l->items[i].object==0) {
        if(item) return 0;
        item=l->items+i; index=i;
    }
    if(!item || index>32767 || item->flags || item->room<0 || (size_t)item->room>=l->room_count) return 0;
    const TombRoom *r=l->rooms+item->room;
    int32_t x=tomb_asr(tomb_long((int64_t)item->x-r->x),10);
    int32_t z=tomb_asr(tomb_long((int64_t)item->z-r->z),10);
    if(!r->sectors || x<0 || x>=r->nx || z<0 || z>=r->nz) return 0;
    TombLaraStart s={0};
    s.actor.x=item->x; s.actor.y=item->y; s.actor.z=item->z;
    s.actor.room=item->room; s.actor.yaw=item->yaw; s.item_index=(uint32_t)index;
    s.sector_floor=r->sectors[x*r->nz+z].floor*256;
    s.health=1000; s.air=1800; s.water_status=(r->flags&1)?1:0;
    s.actor.current=s.actor.goal=s.water_status?13:2;
    s.actor.animation=s.water_status?108:11; s.actor.frame=s.water_status?1736:185;
    if((size_t)s.actor.animation>=l->animation_count) return 0;
    const TombAnim *anim=l->animations+s.actor.animation;
    if(s.actor.frame<anim->first_frame || s.actor.frame>anim->last_frame || anim->state!=s.actor.current) return 0;
    /* Generic item setup zeroes speed, fall speed, pitch and lean. Lara reset
       clears rotation controls and the generic collision flag. Position/yaw
       stay at their file values; move_angle is zero, not copied from yaw. */
    s.services_pending=3; *out=s; return 1;
}
