#include "combat.h"
#include "playtest.h"
#include "enemies.h"
#include "object_contact.h"
#include <stdlib.h>
int tomb_combat_fixture(TombPlaytest *p,TombObjects *w,const int16_t *s,int species) {
    if(!tomb_enemy_object(species))return -1;
    tomb_objects_reset(w);if(!tomb_playtest_init(p,w->level,s,w->visual))return -1;
    p->objects=w;p->movement_only=1;
    for(size_t id=0;id<w->count;id++) {
        const TombObject *o=w->items+id;if(o->object!=species)continue;
        for(int direction=0;direction<4;direction++) {
            static const int offsets[4][2]={{0,-2048},{-2048,0},{0,2048},{2048,0}};
            TombActor a=p->lara.actor;a.x=o->actor.x+offsets[direction][0];a.z=o->actor.z+offsets[direction][1];a.y=o->actor.y;a.room=o->actor.room;
            TombSectorRef ref;TombHeights h;
            if(!tomb_find_sector(w->level,a.x,a.y,a.z,a.room,&ref) || !tomb_item_heights(w->level,ref,a.x,a.y,a.z,0,w->level->items,w->level->item_count,&h))continue;
            if(h.floor==-32512 || h.floor-h.ceiling<900 || (species!=9 && abs(h.floor-o->actor.y)>256))continue;
            a.y=h.floor;a.room=(int16_t)ref.room;a.yaw=tomb_object_angle(o->actor.z-a.z,o->actor.x-a.x);
            TombCameraPoint from={a.x,a.y-650,a.z,a.room},to;
            if(!tomb_gun_target_point(w->visual,&o->actor,s,&to) || !tomb_dos_camera_los(w->level,&from,&to,0))continue;
            p->lara.actor=a;p->animation.move_angle=a.yaw;
            for(int k=0;k<3;k++)w->enemies->camera[k]=k==0?a.x:k==1?a.y:a.z;
            tomb_enemies_trigger(w,id,0,0x3e00);return (int)id;
        }
    }
    return -1;
}
