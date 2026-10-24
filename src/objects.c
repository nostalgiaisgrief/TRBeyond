/* Independently reconstructed from the DOS object/trigger routines.
   Switch/door control lives here; bridges use height callbacks and the Caves
   falling-floor/dart controllers live in hazards.c. */
#include "objects.h"
#include "fixed.h"
#include "hazards.h"
#include "enemies.h"
#include "inventory.h"
#include "progression.h"
#include "city.h"
#include "peru.h"
#include "folly.h"
#include "navigation.h"
#include "atlantean.h"
#include <stdlib.h>
#include <string.h>
static int is_door(int id){return id>=57 && id<=64;}
int tomb_late_object_control(TombObject *o){
    TombActor *a=&o->actor;
    if(o->object==41)a->goal=(int16_t)(tomb_trigger_active(o)!=0);
    else if(o->object==162){
        if((o->flags&0x3e00)==0x3e00){if(a->current>=0 && a->current<=2)a->goal=(int16_t)(a->current+1);o->flags=0;}
        if(a->current==4)return 3;
    }else if(o->object==182){
        if(a->current==1 || a->current==2)a->goal=(int16_t)(a->current+1);
        else if(a->current==3)return 1;
    }
    return 0;
}
int tomb_object_supported(int id){return id==83 || tomb_folly_object(id) || tomb_peru_object(id) || tomb_pickup_object(id) || tomb_enemy_object(id) || id==180 || id==181 || id==177 || id==170 || id==183 || id==161 || id==163 || id==162 || id==182 || id==146 || id==147 || id==35 || id==36 || id==40 || (id>=68 && id<=70) || id==55 || id==56 || (id>=48 && id<=51) || id==41 || id==42 || id==128 || id==179 || id==65 || id==66 || (id>=118 && id<=125) || (id>=137 && id<=140) || is_door(id);}
#define supported tomb_object_supported
int tomb_trigger_active(TombObject *o) {
    int normal=!(o->flags&0x4000);
    if((o->flags&0x3e00)!=0x3e00)return !normal;
    if(!o->timer)return normal;
    if(o->timer==-1)return !normal;
    o->timer=tomb_word((int)o->timer-1);if(!o->timer)o->timer=-1;
    return normal;
}
void tomb_object_trigger(TombObject *o,int type,uint16_t flags) {
    if(o->flags&0x100)return;
    o->timer=(int16_t)(flags&255);if(o->timer!=1)o->timer=(int16_t)(o->timer*30);
    uint16_t mask=flags&0x3e00;
    if(type==2)o->flags^=mask;
    else if(type==6)o->flags&=(uint16_t)~mask;
    else o->flags|=mask;
    if((o->flags&0x3e00)!=0x3e00)return;
    if(flags&0x100)o->flags|=0x100;
    if(!o->active){o->active=1;o->actor.flags=(o->actor.flags&~6u)|3;}
}
int tomb_switch_trigger(TombObject *o,int16_t timer) {
    if((o->actor.flags&6)!=4)return 0;
    if(o->actor.current==0 && timer>0) {
        o->timer=timer==1?1:tomb_word((int)timer*30);
        o->actor.flags=(o->actor.flags&~6u)|2;
    } else {o->active=0;o->actor.flags&=(uint8_t)~7u;}
    return 1;
}
void tomb_trapdoor_control(TombObject *o) {
    if(tomb_trigger_active(o)){if(o->actor.current==0)o->actor.goal=1;}
    else if(o->actor.current==1)o->actor.goal=0;
}
int tomb_key_trigger(TombObject *o,int weapon){
    if((o->actor.flags&6)!=2 || weapon)return 0;
    o->actor.flags=(o->actor.flags&~6u)|4;return 1;
}
int tomb_door_control(TombObject *o) {
    if(tomb_trigger_active(o)) {
        if(o->actor.current==0){o->actor.goal=1;return -1;}
        return 1;
    }
    if(o->actor.current==1){o->actor.goal=0;return -1;}
    return 0;
}
/* AnimateItem 0x16ff4 for Caves switches, doors, floors and darts. Their
   commands use sound/deactivate; other object command classes remain guarded. */
int tomb_object_animate_required(TombObject *o,TombAnimContext *c,int16_t *required) {
    TombActor *a=&o->actor;
    if(a->animation<0 || (size_t)a->animation>=c->animation_count)return 0;
    const TombAnim *anim=c->animations+a->animation;
    a->flags&=(uint8_t)~16u;a->frame=tomb_word((int)a->frame+1);
    if(a->current!=a->goal) {
        int changed=0;
        for(int i=0;i<anim->change_count && !changed;i++) {
            int ci=anim->change_index+i;if(ci<0 || (size_t)ci>=c->change_count)return 0;
            const TombChange *ch=c->changes+ci;if(ch->goal!=a->goal)continue;
            for(int j=0;j<ch->count;j++) {
                int ri=ch->index+j;if(ri<0 || (size_t)ri>=c->range_count)return 0;
                const TombRange *r=c->ranges+ri;
                if(a->frame<r->first || a->frame>r->last)continue;
                if(r->animation<0 || (size_t)r->animation>=c->animation_count)return 0;
                a->animation=r->animation;a->frame=r->frame;anim=c->animations+a->animation;
                a->current=anim->state;if(required && *required==a->current)*required=0;changed=1;break;
            }
        }
    }
    int end=a->frame>anim->last_frame;
    if(end) {
        int at=anim->command_index;
        for(int i=0;i<anim->command_count;i++) {
            if(at<0 || (size_t)at>=c->command_count)return 0;
            int op=c->commands[at++];
            if(op==1){
                if((size_t)at+3>c->command_count)return 0;
                int x=c->commands[at],y=c->commands[at+1],z=c->commands[at+2];at+=3;
                int si=tomb_hazard_sine(c->sine_quarter,a->yaw),co=tomb_hazard_sine(c->sine_quarter,a->yaw+16384);
                a->x+=tomb_asr(x*co+z*si,14);a->y+=y;a->z+=tomb_asr(z*co-x*si,14);
            } else if(op==4)a->flags=(a->flags&~6u)|4;
            else if(op==5 || op==6){if((size_t)at+2>c->command_count)return 0;at+=2;}
            else return 0;
        }
        if(anim->next_animation<0 || (size_t)anim->next_animation>=c->animation_count)return 0;
        a->animation=anim->next_animation;a->frame=anim->next_frame;anim=c->animations+a->animation;
        a->current=a->goal=anim->state;if(required && *required==a->current)*required=0;
    }
    int at=anim->command_index;
    for(int i=0;i<anim->command_count;i++) {
        if(at<0 || (size_t)at>=c->command_count)return 0;
        int op=c->commands[at++];
        if(op==4)continue;
        if(op==1){if((size_t)at+3>c->command_count)return 0;at+=3;continue;}
        if((op!=5 && op!=6) || (size_t)at+2>c->command_count)return 0;
        if(a->frame==c->commands[at]){if(!c->event)return 0;c->event(c->user,op,c->commands[at+1],a);}
        at+=2;
    }
    if(a->flags&8){a->fall_speed=tomb_word(a->fall_speed+(a->fall_speed<128?6:1));a->y+=a->fall_speed;}
    else a->speed=tomb_word(tomb_asr(tomb_long((int64_t)anim->velocity+(int64_t)anim->acceleration*(a->frame-anim->first_frame)),16));
    a->x+=tomb_asr(a->speed*tomb_hazard_sine(c->sine_quarter,a->yaw),14);
    a->z+=tomb_asr(a->speed*tomb_hazard_sine(c->sine_quarter,a->yaw+16384),14);return 1;
}
int tomb_object_animate(TombObject *o,TombAnimContext *c){return tomb_object_animate_required(o,c,NULL);}
/* Caves switches are cardinal, upright placements. Exact original local bounds
   at 0xc3d74, inclusive. No approach assist or positional teleport. */
int tomb_switch_bounds(const TombActor *a,const TombActor *s,int16_t pitch,int16_t roll) {
    int16_t yaw=tomb_word((int)a->yaw-s->yaw);
    if(pitch< -1820 || pitch>1820 || roll< -1820 || roll>1820 || yaw< -5460 || yaw>5460)return 0;
    int64_t dx=(int64_t)a->x-s->x,dz=(int64_t)a->z-s->z,x,z;
    if(s->yaw==0){x=dx;z=dz;}
    else if(s->yaw==16384){x=-dz;z=dx;}
    else if(s->yaw==-32768){x=-dx;z=-dz;}
    else if(s->yaw==-16384){x=dz;z=-dx;}
    else return 0;
    return x>=-200 && x<=200 && a->y==s->y && z>=312 && z<=512;
}
static TombSector *get_sector(TombLevel *l,TombSectorRef r) {
    if(r.room<0 || (size_t)r.room>=l->room_count)return NULL;
    TombRoom *room=l->rooms+r.room;
    return r.index<0 || (size_t)r.index>=(size_t)room->nx*room->nz?NULL:room->sectors+r.index;
}
static int sector_at(TombLevel *l,int room,int32_t x,int32_t z,TombSectorRef *out) {
    if(room<0 || (size_t)room>=l->room_count)return 0;
    TombRoom *r=l->rooms+room;int ix=tomb_asr(x-r->x,10),iz=tomb_asr(z-r->z,10);
    if(ix<0 || iz<0 || ix>=r->nx || iz>=r->nz)return 0;
    *out=(TombSectorRef){room,ix*r->nz+iz};return 1;
}
static int linked_room(const TombLevel *l,const TombSector *s) {
    size_t at=s->floor_index;if(!at)return 255;
    if(at>=l->floor_count)return -1;
    if(l->floor_data[at]==2)at+=2;
    if(at>=l->floor_count)return -1;
    if(l->floor_data[at]==3)at+=2;
    if(at>=l->floor_count)return -1;
    if((l->floor_data[at]&255)!=1)return 255;
    return at+1<l->floor_count?l->floor_data[at+1]:-1;
}
static int save_part(TombObjects *w,TombDoor *d,int room,int32_t x,int32_t z) {
    TombSectorRef ref;if(!sector_at(w->level,room,x,z,&ref) || d->count==4)return 0;
    TombSector *s=get_sector(w->level,ref);int next=linked_room(w->level,s);if(next<0)return 0;
    int box=s->box;
    if(next!=255){TombSectorRef other;if(!sector_at(w->level,next,x,z,&other))return 0;box=get_sector(w->level,other)->box;}
    if(box<0 || (size_t)box>=w->level->box_count || !(w->level->boxes[box].overlap&0x8000))box=-1;
    d->parts[d->count++]=(TombDoorPart){ref,*s,box};return 1;
}
static void door_set(TombObjects *w,TombDoor *d,int open) {
    for(unsigned i=0;i<d->count;i++) {
        TombDoorPart *p=d->parts+i;TombSector *s=get_sector(w->level,p->ref);
        if(open)*s=p->saved;
        else *s=(TombSector){0,65535,255,-127,255,-127};
        if(p->box>=0) {
            if(open)w->level->boxes[p->box].overlap&=(uint16_t)~0x4000u;
            else w->level->boxes[p->box].overlap|=0x4000;
        }
    }
    d->open=open;
}
static int door_init(TombObjects *w,size_t i) {
    TombActor *a=&w->items[i].actor;TombDoor *d=w->doors+i;
    int dx=a->yaw==16384?-1024:a->yaw==0 || a->yaw==-32768?0:1024;
    int dz=a->yaw==0?-1024:a->yaw==-32768?1024:0;
    if(!save_part(w,d,a->room,a->x+dx,a->z+dz))return 0;
    int next=linked_room(w->level,&d->parts[0].saved),alt=w->level->rooms[a->room].alternate;
    if(alt!=-1 && !save_part(w,d,alt,a->x+dx,a->z+dz))return 0;
    /* DOS blocks the first room before inspecting the other side. */
    door_set(w,d,0);
    if(next!=255) {
        if(next<0 || (size_t)next>=w->level->room_count || !save_part(w,d,next,a->x,a->z))return 0;
        alt=w->level->rooms[next].alternate;
        if(alt!=-1 && !save_part(w,d,alt,a->x,a->z))return 0;
    }
    door_set(w,d,0);return 1;
}
static int item_reset(TombObjects *w,size_t i) {
    const TombItem *p=w->level->items+i;TombObject *o=w->items+i;memset(o,0,sizeof *o);o->object=p->object;
    o->actor.room=p->room;o->actor.x=p->x;o->actor.y=p->y;o->actor.z=p->z;
    if(tomb_pickup_object(p->object)){
        o->actor.yaw=p->yaw;o->actor.flags=32;o->flags=p->flags;
        /* DOS InitialiseItem 24944 applies invisibility to sprites too. */
        if(o->flags&0x100){o->flags&=(uint16_t)~0x100;o->actor.flags|=6;}
        if((o->flags&0x3e00)==0x3e00){o->flags=(o->flags&~0x3e00u)|0x4000;o->active=1;o->actor.flags=(o->actor.flags&~6u)|3;}
        return 1;
    }
    if(!supported(p->object))return 1;
    TombModel m;if(!tomb_visual_model(w->visual,p->object,&m) || m.animation>=w->level->animation_count)return 0;
    const TombAnim *a=w->level->animations+m.animation;
    o->actor=(TombActor){p->x,p->y,p->z,a->state,a->state,(int16_t)m.animation,a->first_frame,0,0,p->yaw,p->room,32};
    o->flags=p->flags;
    if(o->flags&0x100){o->flags&=(uint16_t)~0x100;o->actor.flags|=6;}
    if((o->flags&0x3e00)==0x3e00){o->flags=(o->flags&~0x3e00u)|0x4000;o->active=1;o->actor.flags=(o->actor.flags&~6u)|3;}
    if(o->object==83 && (o->actor.flags&6)!=6){o->active=1;o->actor.flags=(o->actor.flags&~6u)|2;}
    w->level->items[i].state=o->actor.current;w->level->items[i].state_known=1;
    return 1;
}
int tomb_objects_init(TombObjects *w,TombLevel *l,const TombVisual *v) {
    memset(w,0,sizeof *w);w->camera.index=w->camera.last=w->camera.target=w->last_target=-1;w->level=l;w->visual=v;w->count=l->item_count;
    w->source_count=l->item_count;
    int reserve=0;for(size_t i=0;i<l->item_count;i++){int id=l->items[i].object;if(id==161 || id==163 || id==181 || id==28 || (id>=30 && id<=32))reserve=128;}
    if(reserve){
        TombItem *items=realloc(l->items,(l->item_count+(size_t)reserve)*sizeof *items);if(!items)return 0;l->items=items;
        for(int j=0;j<reserve;j++){TombItem *item=items+l->item_count+(size_t)j;memset(item,0,sizeof *item);item->object=-1;}
        l->item_count+=(size_t)reserve;w->count=l->item_count;
    }
    w->peru=calloc(1,sizeof *w->peru);if(!w->peru)return 0;w->peru->effect=w->peru->scion_item=-1;
    w->items=calloc(w->count,sizeof *w->items);w->doors=calloc(w->count,sizeof *w->doors);
    w->box_overlap=calloc(l->box_count,sizeof *w->box_overlap);
    if(w->box_overlap)for(size_t i=0;i<l->box_count;i++)w->box_overlap[i]=l->boxes[i].overlap;
    if(!w->items || !w->doors || !w->box_overlap)goto fail;
    for(size_t i=0;i<w->count;i++)if(!item_reset(w,i))goto fail;
    for(size_t i=0;i<w->count;i++)if(is_door(w->items[i].object) && !door_init(w,i))goto fail;
    for(size_t i=0;i<w->count;i++)if(((w->items[i].object>=48 && w->items[i].object<=51) || w->items[i].object==52) && (w->items[i].actor.flags&6)!=6 && !tomb_block_floor(w,i,w->items[i].object==52?-2048:-1024))goto fail;
    w->hazards=calloc(1,sizeof *w->hazards);if(!w->hazards)goto fail;
    w->progress=calloc(1,sizeof *w->progress);if(!w->progress)goto fail;
    if(!tomb_enemies_init(w) || !tomb_atlantean_spawners_init(w))goto fail;
    w->current=calloc(1,sizeof *w->current);if(!w->current)goto fail;
    w->current->nodes=calloc(l->box_count+1,sizeof *w->current->nodes);if(!w->current->nodes)goto fail;
    tomb_current_reset(w);
    return 1;
fail:tomb_objects_free(w);return 0;
}
static void restore(TombObjects *w) {
    if(w->peru && w->peru->flipped)tomb_flip_map(w,0);
    if(w->doors)for(size_t i=w->count;i>0;i--)for(unsigned j=w->doors[i-1].count;j>0;j--) {
        TombDoorPart *p=w->doors[i-1].parts+j-1;*get_sector(w->level,p->ref)=p->saved;
    }
    if(w->box_overlap)for(size_t i=0;i<w->level->box_count;i++)w->level->boxes[i].overlap=w->box_overlap[i];
}
void tomb_current_reset(TombObjects *w){
    w->current_strength=0;if(!w->current || !w->current->nodes)return;
    TombNavNode *nodes=w->current->nodes;memset(w->current,0,sizeof *w->current);w->current->nodes=nodes;
    w->current->head=w->current->tail=w->current->target_box=w->current->required_box=-1;
    w->current->step=20480;w->current->drop=-20480;w->current->fly=256;w->current->block=0x4000;
    memset(nodes,0,(w->level->box_count+1)*sizeof *nodes);
    for(size_t i=0;i<=w->level->box_count;i++)nodes[i].exit=nodes[i].next=-1;
}
void tomb_objects_reset(TombObjects *w) {
    w->invincible=0;
    w->no_enemies=0;
    tomb_current_reset(w);
    if(w->progress)memset(w->progress,0,sizeof *w->progress);
    restore(w);memset(w->peru,0,sizeof *w->peru);w->peru->effect=w->peru->scion_item=-1;w->deferred_actions=0;if(w->hazards)memset(w->hazards,0,sizeof *w->hazards);
    memset(&w->camera,0,sizeof w->camera);memset(w->camera_once,0,sizeof w->camera_once);
    w->camera.index=w->camera.last=w->camera.target=w->last_target=-1;
    for(size_t i=w->source_count;i<w->count;i++){memset(w->level->items+i,0,sizeof *w->level->items);w->level->items[i].object=-1;}
    for(size_t i=0;i<w->count;i++){item_reset(w,i);if(is_door(w->items[i].object) && w->doors[i].count)door_set(w,w->doors+i,0);else if((w->items[i].object>=48 && w->items[i].object<=51) || w->items[i].object==52){w->doors[i].count=0;if((w->items[i].actor.flags&6)!=6)tomb_block_floor(w,i,w->items[i].object==52?-2048:-1024);}}
    tomb_enemies_reset(w);if(!tomb_atlantean_spawners_init(w))++w->deferred_actions;
}
void tomb_objects_free(TombObjects *w) {
    if(w->level)restore(w);
    if(w->current){tomb_navigation_free(w->current);free(w->current);}
    tomb_enemies_free(w);free(w->items);free(w->doors);free(w->box_overlap);free(w->hazards);free(w->progress);free(w->peru);if(w->level)w->level->item_count=w->source_count;memset(w,0,sizeof *w);
}
int tomb_objects_spawn(TombObjects *w,int object,const TombActor *origin){
    TombModel model;if(!tomb_pickup_object(object) && !tomb_visual_model(w->visual,object,&model))return -1;
    for(size_t i=w->source_count;i<w->count;i++)if(w->items[i].object<0){
        TombItem *item=w->level->items+i;memset(item,0,sizeof *item);item->object=(int16_t)object;item->room=origin->room;item->x=origin->x;item->y=origin->y;item->z=origin->z;item->yaw=origin->yaw;item->intensity=-1;
        if(tomb_pickup_object(object))item->intensity=4096;
        if(!item_reset(w,i)){item->object=-1;w->items[i].object=-1;return -1;}
        tomb_enemies_room_sync(w);return (int)i;
    }
    return -1;
}
int tomb_objects_tick(TombObjects *w,TombAnimContext *c) {
    /* Active list insertion is at its head in DOS: descending activation order
       is immaterial to these independent doors; switch timing still runs before Lara. */
    for(size_t i=0;i<w->count;i++) {
        TombObject *o=w->items+i;if(!o->active)continue;
        if(o->object==83){if(!tomb_object_animate(o,c))return 0;continue;}
        if(o->object==47 || o->object==52 || (o->object>=74 && o->object<=76)){if(!tomb_peru_tick(w,i,c))return 0;continue;}
        if((o->object>=48 && o->object<=51)){if(!tomb_block_tick(w,i,c))return 0;continue;}
        if(o->object==41 || o->object==162 || o->object==182 || o->object==146 || o->object==147){
            int action=tomb_late_object_control(o);
            if(action&2){w->peru->flip_flags[3]=0x3e00;if(!tomb_flip_map(w,1))return 0;}
            if(!tomb_object_animate(o,c))return 0;
            /* DOS KillItem removes the item from the room draw list even if
               the final AnimateItem command changes its status afterwards.
               Our hidden status represents that removal, so apply it last. */
            if(action&1){o->active=0;o->actor.flags=(o->actor.flags&~33u)|6;}
            if(o->object==41){TombSectorRef r;if(tomb_find_sector(w->level,o->actor.x,o->actor.y,o->actor.z,o->actor.room,&r))o->actor.room=(int16_t)r.room;}
            w->level->items[i].state=o->actor.current;continue;
        }
        if(o->object==65 || o->object==66){tomb_trapdoor_control(o);if(!tomb_object_animate(o,c))return 0;w->level->items[i].state=o->actor.current;continue;}
        if(o->object!=55 && o->object!=56 && !is_door(o->object))continue;
        if(is_door(o->object)){int action=tomb_door_control(o);if(action>=0)door_set(w,w->doors+i,action);}
        else {o->flags|=0x3e00;if(!tomb_trigger_active(o)){o->actor.goal=1;o->timer=0;}}
        if(!tomb_object_animate(o,c))return 0;
    }
    return 1;
}
int tomb_objects_interact(TombObjects *w,TombActor *a,TombAnimContext *c,uint32_t input,int16_t pitch,int16_t roll) {
    if(!(input&64) || c->weapon_status || (a->flags&8) || a->current!=2)return 1;
    for(size_t i=0;i<w->count;i++) {
        TombObject *o=w->items+i;
        if(o->object!=55 || o->actor.room!=a->room || (o->actor.flags&6) || !tomb_switch_bounds(a,&o->actor,pitch,roll))continue;
        if(o->actor.current!=0 && o->actor.current!=1)continue;
        a->yaw=o->actor.yaw;int goal=o->actor.current==1?40:41;a->goal=(int16_t)goal;
        unsigned budget=120;while(a->current!=goal && budget--)if(!tomb_animate(a,c))return 0;
        if(a->current!=goal)return 0;
        a->goal=2;c->weapon_status=1;o->actor.goal=o->actor.current==1?0:1;
        o->active=1;o->actor.flags=(o->actor.flags&~6u)|3;
        if(!tomb_object_animate(o,c))return 0;
        return 1;
    }
    return 1;
}
void tomb_objects_camera_begin(TombObjects *w) {
    /* DOS FixedCamera / CalculateCamera (14534 / 14a41): ordinary shots
       need floor refresh; heavy shots retain their identity/target until expiry. */
    if(w->camera.active && w->camera.timer>0 && !--w->camera.timer)w->camera.timer=-1;
    w->camera.last=w->camera.index;
    if(w->camera.active==5 && w->camera.timer!=-1)return;
    w->last_target=w->camera.target;w->camera.last=w->camera.index;w->camera.index=-1;w->camera.target=-1;w->camera.active=0;
}
static int trigger(TombObjects *w,size_t index,int grounded,int heavy,int weapon,int camera_mode,int activated) {
    int interrupt=w->camera.active!=5 && (camera_mode==2 || camera_mode==3);
    if(!index)return 1;
    const TombLevel *l=w->level;
    if(index>=l->floor_count)return 0;
    unsigned header=l->floor_data[index];if((header&255)==5){if(header&0x8000)return 1;if(++index>=l->floor_count)return 0;header=l->floor_data[index];}if((header&255)!=4)return 1;
    unsigned type=(header>>8)&63;
    if(heavy){if(type!=5)return 1;}else if(type!=0 && type!=1 && type!=2 && type!=3 && type!=4 && type!=6)return 1;
    if((type==1 || type==6) && !grounded)return 1;
    if(l->floor_count-index<3)return 0;
    uint16_t flags=l->floor_data[index+1];size_t at=index+2;
    size_t switch_id=0;
    if(type==3){switch_id=l->floor_data[at++]&1023;if(switch_id>=w->count)return 0;}
    if(type==4){switch_id=l->floor_data[at++]&1023;if(switch_id>=w->count)return 0;}
    if(type==2){switch_id=l->floor_data[at++]&1023;if(switch_id>=w->count || (w->items[switch_id].object!=55 && w->items[switch_id].object!=56))return 0;}
    /* Validate the list completely before altering timers, latches or switches. */
    size_t first=at;int ended=0;
    while(at<l->floor_count) {
        unsigned word=l->floor_data[at++],action=(word&0x3fff)>>10,id=word&1023;
        if(action==0 && id>=w->count)return 0;
        if(action==6 && id>=w->count)return 0;
        if(action==2 && (id>=l->camera_count || l->cameras[id].flags>=l->box_count))return 0;
        if(action==1){if(id>=l->camera_count || at>=l->floor_count)return 0;word=l->floor_data[at++];}
        if(word&0x8000){ended=1;break;}
    }
    if(!ended)return 0;
    /* DOS 0x1790c refreshes a shot before the switch completion gate, even
       after a one-shot camera has latched. */
    int target=-1;
    at=first;
    while(w->camera.active!=5 && at<l->floor_count) {
        unsigned word=l->floor_data[at++],action=(word&0x3fff)>>10,id=word&1023;
        if(action==1){word=l->floor_data[at++];if((int)id==w->camera.last){w->camera.index=(int)id;if(interrupt)w->camera.timer=-1;w->camera.active=w->camera.timer>=0;}}
        if(action==6 && !interrupt)target=(int)id;
        if(word&0x8000)break;
    }
    if(w->camera.active==1)w->camera.target=target;
    /* DOS PickupTrigger (34c9c) consumes invisible -> deactivated once.
       RefreshCamera must run first so the initial shot survives consumption. */
    if(type==4){
        TombActor *pickup=&w->items[switch_id].actor;
        if((pickup->flags&6)!=6)return 1;
        pickup->flags=(pickup->flags&~6u)|4;
    }
    /* DOS refreshes the camera at 17ac9 before KeyTrigger at 17b52.
       The consumed key rejects further activation, but must not end the shot. */
    if(type==3 && !activated && !tomb_key_trigger(w->items+switch_id,weapon))return 1;
    if(type==2 && !activated && !tomb_switch_trigger(w->items+switch_id,(int16_t)(flags&255)))return 1;
    int flip=0,effect=-1;
    at=first;
    while(at<l->floor_count) {
        unsigned word=l->floor_data[at++],action=(word&0x3fff)>>10,id=word&1023;
        if(action==0 && tomb_object_supported(w->items[id].object) && !tomb_enemy_object(w->items[id].object))tomb_object_trigger(w->items+id,(int)type,flags);
        else if(action==0 && tomb_enemy_object(w->items[id].object))tomb_enemies_trigger(w,id,(int)type,flags);
        else if(action==1) {
            word=l->floor_data[at++];
            if(!(w->camera_once[id/8]&(1u<<(id%8))) && !(l->cameras[id].flags&0x100)) {
                w->camera.index=(int)id;
                if(!interrupt && !(type==2 && (flags&255) && (activated?w->items[switch_id].actor.goal:w->items[switch_id].actor.current)==1) && (type==2 || (int)id!=w->camera.last)) {
                    w->camera.timer=word&255;if(w->camera.timer!=1)w->camera.timer*=30;
                    w->camera.speed=((word&0x3e00)>>6)+1;w->camera.active=heavy?5:1;
                    if(word&0x100)w->camera_once[id/8]|=(uint8_t)(1u<<(id%8));
                }
            }
        } else if(action==6){if(!interrupt)target=(int)id;
        } else if(action==2 && w->current){
            const TombFixedCamera *sink=l->cameras+id;
            if(w->current->required_box!=(int16_t)sink->flags){w->current->required_box=(int16_t)sink->flags;w->current->x=sink->x;w->current->y=sink->y;w->current->z=sink->z;}
            w->current_strength=tomb_word(sink->room*6);
        } else if(action>=3 && action<=5)flip|=tomb_flip_action(w,(int)action,(int)id,(int)type,flags);
        else if(action==9)effect=(int)id;
        else if(action==7 || action==8 || action==10)tomb_progress_action_source(w->progress,(int)action,(int)id,flags,(int)type,w->level->music_edition==2);
        else if(action!=6)++w->deferred_actions;
        if(word&0x8000)break;
    }
    if(flip){if(!tomb_flip_map(w,1))return 0;if(effect>=0){w->peru->effect=effect;w->peru->effect_ticks=0;}}
    if(target>=0 || w->camera.active!=5)w->camera.target=target;
    if(!w->camera.active && target>=0 && (w->items[target].actor.flags&64) && target!=w->last_target)w->camera.target=-1;
    return 1;
}


int tomb_objects_trigger(TombObjects *w,size_t index,int grounded){return trigger(w,index,grounded,0,1,0,0);}
int tomb_objects_trigger_heavy(TombObjects *w,size_t index){return trigger(w,index,1,1,1,0,0);}

int tomb_objects_trigger_keys(TombObjects *w,size_t index,int grounded,int weapon){return trigger(w,index,grounded,0,weapon,0,0);}
int tomb_objects_trigger_camera(TombObjects *w,size_t index,int grounded,int weapon,int mode){return trigger(w,index,grounded,0,weapon,mode,0);}

int tomb_objects_trigger_activated(TombObjects *w,size_t index){return trigger(w,index,1,0,0,0,1);}
