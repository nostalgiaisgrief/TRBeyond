/* CreatureAnimation 0x12910 and BadFloor 0x1283c, original sector radius
   tests and step/drop limits; no modern path steering or physics solver. */
#include "navigation.h"
#include "object_contact.h"
#include "fixed.h"
#include "enemies.h"
static int clamp(int x,int lo,int hi){return x<lo?lo:x>hi?hi:x;}
static int sample(const TombLevel *l,int x,int y,int z,int *room,TombSectorRef *ref,TombHeights *h) {
    if(!tomb_find_sector(l,x,y,z,*room,ref) || !tomb_item_heights(l,*ref,x,y,z,0,l->items,l->item_count,h))return 0;
    *room=ref->room;return 1;
}
static int box_at(const TombLevel *l,TombSectorRef ref){int b=l->rooms[ref.room].sectors[ref.index].box;return (size_t)b<l->box_count?b:-1;}
static int bad(const TombLevel *l,const TombNavigation *n,int x,int y,int z,int height,int next_height,int room) {
    TombSectorRef ref;if(!tomb_find_sector(l,x,y,z,room,&ref))return 1;int box=box_at(l,ref);if(box<0)return 1;
    const TombCameraBox *b=l->boxes+box;int delta=height-b->height;
    return (b->overlap&n->block) || delta>n->step || delta<n->drop || (delta< -n->step && b->height>next_height) || (n->fly && y>b->height+n->fly);
}
int tomb_creature_move(TombObject *o,TombCreature *c,TombNavigation *n,const TombObjects *w,TombAnimContext *ctx,int16_t turn,int16_t tilt) {
    TombActor *a=&o->actor;const TombLevel *l=w->level;int box=tomb_navigation_box(l,a);if(box<0)return -1;
    int oldx=a->x,oldy=a->y,oldz=a->z,oldheight=l->boxes[box].height;
    if(!tomb_object_animate_required(o,ctx,&c->required))return -1;c->touch=0;
    if((a->flags&6)==4){c->health=-16384;a->flags&=(uint8_t)~33u;o->active=0;return 0;}
    int16_t bounds[6];if(!tomb_visual_item_bounds(w->visual,o->object,a,bounds))return -1;
    int y=a->y+bounds[2],room=a->room;TombSectorRef ref;TombHeights h;
    if(!sample(l,a->x,y,a->z,&room,&ref,&h))return -1;
    int newbox=box_at(l,ref),height=newbox<0?oldheight:l->boxes[newbox].height;
    const int16_t *zones=l->zones+l->box_count*(n->fly?2:n->step==256?0:1);
    if(newbox<0 || zones[box]!=zones[newbox] || oldheight-height>n->step || oldheight-height<n->drop) {
        int cell=tomb_asr(a->x,10),oldcell=tomb_asr(oldx,10);
        if(cell<oldcell)a->x=oldx&~1023;else if(cell>oldcell)a->x=oldx|1023;
        /* The DOS build compares the retained X cell against old Z here. */
        oldcell=tomb_asr(oldz,10);if(cell<oldcell)a->z=oldz&~1023;else if(cell>oldcell)a->z=oldz|1023;
        if(!sample(l,a->x,y,a->z,&room,&ref,&h))return -1;newbox=box_at(l,ref);if(newbox<0)return -1;height=l->boxes[newbox].height;
    }
    int exit=n->nodes[newbox].exit,next_height=exit>=0 && (size_t)exit<l->box_count?l->boxes[exit].height:height;
    int x=a->x,z=a->z,rx=x&1023,rz=z&1023,radius=o->object==16 || o->object==17 || o->object==30 || o->object==33?204:o->object==9 || o->object==27 || o->object==28 || o->object==31 || o->object==32?102:341,shiftx=0,shiftz=0;
    if(rz<radius) {
        if(bad(l,n,x,y,z-radius,height,next_height,room))shiftz=radius-rz;
        if(rx<radius) {
            if(bad(l,n,x-radius,y,z,height,next_height,room))shiftx=radius-rx;
            else if(!shiftz && bad(l,n,x-radius,y,z-radius,height,next_height,room)) {
                if(a->yaw> -24576 && a->yaw<8192)shiftz=radius-rz;else shiftx=radius-rx;
            }
        } else if(rx>1024-radius) {
            if(bad(l,n,x+radius,y,z,height,next_height,room))shiftx=1024-radius-rx;
            else if(!shiftz && bad(l,n,x+radius,y,z-radius,height,next_height,room)) {
                if(a->yaw> -8192 && a->yaw<24576)shiftz=radius-rz;else shiftx=1024-radius-rx;
            }
        }
    } else if(rz>1024-radius) {
        if(bad(l,n,x,y,z+radius,height,next_height,room))shiftz=1024-radius-rz;
        if(rx<radius) {
            if(bad(l,n,x-radius,y,z,height,next_height,room))shiftx=radius-rx;
            else if(!shiftz && bad(l,n,x-radius,y,z+radius,height,next_height,room)) {
                if(a->yaw> -8192 && a->yaw<24576)shiftx=radius-rx;else shiftz=1024-radius-rz;
            }
        } else if(rx>1024-radius) {
            if(bad(l,n,x+radius,y,z,height,next_height,room))shiftx=1024-radius-rx;
            else if(!shiftz && bad(l,n,x+radius,y,z+radius,height,next_height,room)) {
                if(a->yaw> -24576 && a->yaw<8192)shiftx=1024-radius-rx;else shiftz=1024-radius-rz;
            }
        }
    } else if(rx<radius){if(bad(l,n,x-radius,y,z,height,next_height,room))shiftx=radius-rx;}
    else if(rx>1024-radius){if(bad(l,n,x+radius,y,z,height,next_height,room))shiftx=1024-radius-rx;}
    a->x+=shiftx;a->z+=shiftz;
    if(shiftx || shiftz) {
        if(!sample(l,a->x,y,a->z,&room,&ref,&h))return -1;
        a->yaw=tomb_word((int)a->yaw+turn);c->roll=tomb_word((int)c->roll+clamp(tomb_word(8*tilt-c->roll),-546,546));
    }
    /* Original linked-list order changes when a creature enters a room. */
    for(int j=w->enemies?w->enemies->room_head[a->room]:(int)w->count-1;j>=0;j=w->enemies?w->enemies->room_next[j]:j-1) {
        const TombObject *other=w->items+j;if(other==o)break;
        const TombActor *b=&other->actor;if(b->room!=a->room || (b->flags&6)!=2 || !b->speed)continue;
        int64_t dx=(int64_t)b->x-a->x,dy=(int64_t)b->y-a->y,dz=(int64_t)b->z-a->z;
        if(dx*dx+dy*dy+dz*dz<radius*radius){a->x=oldx;a->y=oldy;a->z=oldz;return 1;}
    }
    if(n->fly) {
        /* DOS 13270: swimming crocodiles test ceiling clearance at their origin. */
        int top=o->object==11?0:bounds[2];
        int dy=clamp(c->target_y-a->y,-n->fly,n->fly);
        if(a->y+dy>h.floor) {
            if(a->y>h.floor){a->x=oldx;a->z=oldz;dy=-n->fly;}
            else{a->y=h.floor;dy=0;}
        } else if(a->y+top+dy<h.ceiling) {
            if(a->y+top<h.ceiling){a->x=oldx;a->z=oldz;dy=n->fly;}else dy=0;
        }
        a->y+=dy;if(!sample(l,a->x,y,a->z,&room,&ref,&h))return -1;c->floor=h.floor;
        int16_t pitch=a->speed?tomb_object_angle(a->speed,-dy):0;c->pitch=tomb_word((int)c->pitch+clamp((int)pitch-c->pitch,-182,182));
    } else {
        if(a->y>c->floor)a->y=c->floor;else if(c->floor-a->y>64)a->y+=64;else if(a->y<c->floor)a->y=c->floor;
        c->pitch=0;if(!sample(l,a->x,a->y,a->z,&room,&ref,&h))return -1;c->floor=h.floor;
    }
    a->room=(int16_t)room;return 1;
}
