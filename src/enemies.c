#include "enemies.h"
#include "pistols.h"
#include "peru.h"
#include "folly.h"
#include "atlantean.h"
#include "water.h"
#include "dos_camera.h"
#include "hazards.h"
#include "fixed.h"
#include "object_contact.h"
#include "progression.h"
#include <stdlib.h>
#include <string.h>
int tomb_enemy_object(int id){return id==33 || id==34 || id==6 || id==145 || (id>=30 && id<=32) || (id>=20 && id<=23) || id==16 || id==17 || id==10 || id==11 || id==12 || id==13 || id==14 || id==15 || id==28 || (id>=7 && id<=9) || id==18 || id==19 || id==24 || id==27;}
static int slot(const TombEnemies *e,size_t id){for(int k=0;k<8;k++)if(e->slots[k]==(int)id)return k;return -1;}
static int32_t distance(const TombEnemies *e,const TombActor *a) {
    int32_t x=tomb_asr(a->x-e->camera[0],8),y=tomb_asr(a->y-e->camera[1],8),z=tomb_asr(a->z-e->camera[2],8);
    return tomb_long((int64_t)x*x+(int64_t)y*y+(int64_t)z*z);
}
int tomb_enemies_activate(TombObjects *w,size_t id,int always) {
    TombEnemies *e=w->enemies;if(!e || id>=w->count || !tomb_enemy_object(w->items[id].object))return 0;
    if(w->items[id].object==24 || w->items[id].object==6 || w->items[id].object==145)return 1;
    if(slot(e,id)>=0)return 1;
    int use=-1;for(int k=0;k<8;k++)if(e->slots[k]<0){use=k;break;}
    if(use<0) {
        int32_t far=always?0:distance(e,&w->items[id].actor);
        for(int k=0;k<8;k++){int32_t d=distance(e,&w->items[e->slots[k]].actor);if(d>far){far=d;use=k;}}
        if(use<0)return 0;
        w->items[e->slots[use]].actor.flags|=6;tomb_navigation_free(e->navigation+use);e->slots[use]=-1;
    }
    TombCreature *c=e->items+id;c->head=c->neck=c->flags=0;c->maximum_turn=182;c->mood=0;
    if(!tomb_navigation_init(e->navigation+use,w->level,&w->items[id].actor,w->items[id].object))return 0;
    e->slots[use]=(int)id;return 1;
}
static void add(TombObjects *w,size_t id) {
    TombEnemies *e=w->enemies;
    for(int at=e->head;at>=0;at=e->next[at])if(at==(int)id)return;
    e->next[id]=e->head;e->head=(int)id;w->items[id].active=1;w->items[id].actor.flags|=1;
}
static int removable_enemy(int object){return object!=145 && tomb_enemy_object(object);}
static int skip_enemy(TombObjects *w,size_t id){
    TombObject *o=w->items+id;TombCreature *c=w->enemies->items+id;
    if(c->health==-16384)return 1;
    /* Mark first: a death trigger may activate this same item recursively. */
    c->health=-16384;c->touch=0;o->active=0;o->actor.flags=(o->actor.flags&~33u)|6;
    int k=slot(w->enemies,id);if(k>=0){tomb_navigation_free(w->enemies->navigation+k);w->enemies->slots[k]=-1;}
    int *link=&w->enemies->head;
    while(*link>=0 && *link!=(int)id)link=w->enemies->next+*link;
    if(*link>=0)*link=w->enemies->next[id];w->enemies->next[id]=-1;
    if(o->object==28 && (o->flags&0x100)){
        const int drops[]={86,144,129};for(int j=0;j<3;j++)if(tomb_objects_spawn(w,drops[j],&o->actor)<0)return 0;
    }else if(o->object>=30 && o->object<=32){
        if(tomb_objects_spawn(w,o->object==30?87:o->object==31?86:85,&o->actor)<0)return 0;
    }
    if(o->object==27){
        /* Qualopec's Larson death animation ends the level. Other Larson
           placements do not: use this level's animation command data. */
        TombModel model;
        if(tomb_visual_model(w->visual,27,&model) && (size_t)(model.animation+15)<w->level->animation_count){
            const TombAnim *anim=w->level->animations+model.animation+15;size_t at=anim->command_index;
            for(int j=0;j<anim->command_count && at<w->level->command_count;j++){
                int op=w->level->commands[at++],n=op==1?3:op==2 || op==5 || op==6?2:0;
                if(at+n>w->level->command_count)return 0;
                if(op==6 && w->level->commands[at+1]==4)w->progress->complete=1;
                at+=n;
            }
        }
    }
    if(o->object==34){
        TombSectorRef r;TombHeights h;
        if(!tomb_find_sector(w->level,o->actor.x,o->actor.y,o->actor.z,o->actor.room,&r) || !tomb_item_heights(w->level,r,o->actor.x,o->actor.y,o->actor.z,0,w->level->items,w->count,&h) || !tomb_objects_trigger_heavy(w,h.trigger_index))return 0;
    }else if(o->object==6){
        /* Its initial placement is outside the mirror arena. Find the lava
           death pad nearest the DOS reflection centre (0x9000, 0xf000). */
        size_t death_trigger=0;int64_t nearest=INT64_MAX;
        for(size_t room_id=0;room_id<w->level->room_count;room_id++){
            TombRoom *room=w->level->rooms+room_id;
            for(int x=0;x<room->nx;x++)for(int z=0;z<room->nz;z++){
                TombHeights h;int px=room->x+x*1024+512,pz=room->z+z*1024+512;
                if(!tomb_static_heights(w->level,(TombSectorRef){(int)room_id,x*room->nz+z},px,pz,0,&h))continue;
                size_t at=h.trigger_index;if(!at || at+1>=w->level->floor_count || w->level->floor_data[at]!=5 || (w->level->floor_data[at+1]&0x3fff)!=0x504)continue;
                int64_t dx=px-0x9000,dz=pz-0xf000,d=dx*dx+dz*dz;
                if(d<nearest){nearest=d;death_trigger=at;}
            }
        }
        if(!death_trigger || !tomb_objects_trigger_heavy(w,death_trigger))return 0;
    }
    return 1;
}
int tomb_enemies_configure(TombObjects *w,int no_enemies){
    w->no_enemies=no_enemies;
    if(!no_enemies)return 1;
    for(size_t id=0;id<w->count;id++)if(removable_enemy(w->items[id].object)){
        if(w->items[id].active && !skip_enemy(w,id))return 0;
        w->items[id].actor.flags|=6;
    }
    return 1;
}
int tomb_enemies_birth(TombObjects *w,size_t id){
    if(id>=w->count || !tomb_enemy_object(w->items[id].object))return 0;
    TombObject *o=w->items+id;TombCreature *c=w->enemies->items+id;
    memset(c,0,sizeof *c);c->health=(int16_t)(o->object==34?500:o->object==23?120:50);
    if(w->no_enemies && removable_enemy(o->object))return skip_enemy(w,id);
    TombSectorRef r;TombHeights h;
    if(!tomb_find_sector(w->level,o->actor.x,o->actor.y,o->actor.z,o->actor.room,&r) || !tomb_static_heights(w->level,r,o->actor.x,o->actor.z,0,&h))return 0;
    c->floor=h.floor;add(w,id);o->actor.flags=(o->actor.flags&~6u)|2;
    if(!tomb_enemies_activate(w,id,1))o->actor.flags|=6;
    tomb_enemies_room_sync(w);return 1;
}
void tomb_enemies_room_sync(TombObjects *w) {
    TombEnemies *e=w->enemies;if(!e)return;
    for(size_t id=0;id<w->count;id++) {
        int room=w->items[id].actor.room,old=e->room_id[id];if(room==old || room<0 || (size_t)room>=w->level->room_count)continue;
        int *link=e->room_head+old;while(*link>=0 && *link!=(int)id)link=e->room_next+*link;
        if(*link==(int)id)*link=e->room_next[id];
        e->room_next[id]=e->room_head[room];e->room_head[room]=(int)id;e->room_id[id]=room;
    }
}
void tomb_enemies_trigger(TombObjects *w,size_t id,int type,uint16_t flags) {
    if(w->no_enemies && removable_enemy(w->items[id].object)){
        if(w->enemies->items[id].health==-16384)return;
        tomb_object_trigger(w->items+id,type,flags);
        if(w->items[id].active)skip_enemy(w,id);
        w->items[id].actor.flags|=6;return;
    }
    /* Exploded creatures must not be mistaken for temporarily evicted creatures. */
    if(w->items[id].object>=20 && w->items[id].object<=23 && w->enemies->items[id].health==-16384)return;
    TombObject *o=w->items+id;uint8_t before=o->actor.flags;int active=o->active;
    tomb_object_trigger(o,type,flags);
    if(active || !o->active)return;
    o->active=0;o->actor.flags=before;
    if((before&6)==0){o->actor.flags=(before&~6u)|2;w->enemies->items[id].touch=0;add(w,id);tomb_enemies_activate(w,id,1);}
    else if((before&6)==6){w->enemies->items[id].touch=0;if(tomb_enemies_activate(w,id,0))o->actor.flags=(before&~6u)|2;add(w,id);}
}
void tomb_enemies_reset(TombObjects *w) {
    TombEnemies *e=w->enemies;if(!e)return;
    for(int k=0;k<8;k++){tomb_navigation_free(e->navigation+k);e->slots[k]=-1;}
    e->head=-1;e->random=(uint32_t)-747505337;memset(e->items,0,w->count*sizeof *e->items);
    for(size_t j=0;j<w->level->room_count;j++)e->room_head[j]=-1;
    for(size_t j=0;j<w->count;j++) {
        e->next[j]=-1;TombObject *o=w->items+j;int room=o->actor.room;
        e->room_id[j]=room;e->room_next[j]=e->room_head[room];e->room_head[room]=(int)j;
        tomb_folly_init(w,j);
        if(o->object==38 || o->object==53){TombCreature *c=e->items+j;c->health=o->object==24?18:-16384;TombSectorRef r;TombHeights h;if(tomb_find_sector(w->level,o->actor.x,o->actor.y,o->actor.z,o->actor.room,&r) && tomb_static_heights(w->level,r,o->actor.x,o->actor.z,0,&h))c->floor=h.floor;continue;}
        if(!tomb_enemy_object(o->object))continue;
        TombCreature *c=e->items+j;c->health=o->object==33?400:o->object==34?500:o->object==6?1000:o->object==145?5:o->object==30?125:o->object==31?150:o->object==32?200:o->object==23?120:(o->object>=20 && o->object<=22)?50:o->object==16 || o->object==17?5:o->object==28?70:o->object==15?22:o->object==12?30:o->object==13?25:o->object==14?45:o->object==10 || o->object==11?20:o->object==24?18:o->object==27?50:o->object==7?6:o->object==18?100:o->object==8 || o->object==19?20:1;
        if(o->object!=24 && o->object!=6 && o->object!=145)o->actor.yaw=tomb_word((int)o->actor.yaw+tomb_asr((int)tomb_control_random(&e->random)-16384,1));
        if(o->object==7)o->actor.frame=96;
        if(o->object==30 || o->object==32)o->actor.current=(int16_t)(o->object==30?2:3);
        TombSectorRef ref;TombHeights h;if(tomb_find_sector(w->level,o->actor.x,o->actor.y,o->actor.z,o->actor.room,&ref) && tomb_static_heights(w->level,ref,o->actor.x,o->actor.z,0,&h))c->floor=h.floor;
        if(o->active){add(w,j);tomb_enemies_activate(w,j,1);}
    }
}
int tomb_enemies_init(TombObjects *w) {
    TombEnemies *e=calloc(1,sizeof *e);if(!e)return 0;w->enemies=e;
    e->items=calloc(w->count,sizeof *e->items);e->next=calloc(w->count,sizeof *e->next);
    e->room_head=calloc(w->level->room_count,sizeof(int));e->room_next=calloc(w->count,sizeof(int));e->room_id=calloc(w->count,sizeof(int));
    if(!e->items || !e->next || !e->room_head || !e->room_next || !e->room_id){tomb_enemies_free(w);return 0;}
    tomb_enemies_reset(w);return 1;
}
void tomb_enemies_free(TombObjects *w) {
    TombEnemies *e=w->enemies;if(!e)return;for(int k=0;k<8;k++)tomb_navigation_free(e->navigation+k);
    free(e->items);free(e->next);free(e->room_head);free(e->room_next);free(e->room_id);free(e);w->enemies=NULL;
}
int tomb_enemies_tick(TombObjects *w,TombAnimContext *ctx,TombLaraStart *lara) {
    TombEnemies *e=w->enemies;if(!e)return 1;
    tomb_enemies_room_sync(w);
    int previous=-1,index=e->head;
    while(index>=0) {
        TombObject *o=w->items+index;TombCreature *c=e->items+index;int next=e->next[index];
        if(o->object==6 || o->object==145){previous=index;index=next;continue;}
        /* Only one non-final Pierre encounter can own the active slot. The
           active list is newest-first, so its oldest surviving Pierre owns it. */
        if(o->object==28 && !(o->flags&0x100)){
            int owner=index;for(int at=e->head;at>=0;at=e->next[at])if(w->items[at].object==28 && w->items[at].active)owner=at;
            if(owner!=index){
                int old=slot(e,(size_t)index);if(old>=0){tomb_navigation_free(e->navigation+old);e->slots[old]=-1;}
                o->active=0;o->flags|=0x100;o->actor.flags=(o->actor.flags&~33u)|6;
                if(previous<0)e->head=next;else e->next[previous]=next;e->next[index]=-1;index=next;continue;
            }
        }
        if((o->actor.flags&6)==6) {
            if(!tomb_enemies_activate(w,(size_t)index,0)){previous=index;index=next;continue;}
            o->actor.flags=(o->actor.flags&~6u)|2;
        }
        if(o->object==24){
            tomb_mummy_control(o,c,&lara->actor);if(!tomb_object_animate(o,ctx))return 0;
            if((o->actor.flags&6)==4){o->active=0;o->actor.flags&=(uint8_t)~1u;c->health=-16384;if(previous<0)e->head=next;else e->next[previous]=next;e->next[index]=-1;}else previous=index;
            index=next;continue;
        }
        int k=slot(e,(size_t)index);if(k<0){previous=index;index=next;continue;}
        if(o->object>=20 && o->object<=22 && c->health<=0){
            if(!tomb_explode_meshes(w,o,c,UINT32_MAX,100,ctx->sine_quarter))return 0;
            if(ctx->event)ctx->event(ctx->user,5,171,&o->actor);
            c->health=-16384;o->active=0;o->actor.flags=(o->actor.flags&~33u)|6;
            tomb_navigation_free(e->navigation+k);e->slots[k]=-1;
            if(previous<0)e->head=next;else e->next[previous]=next;e->next[index]=-1;index=next;continue;
        }
        TombNavigation *n=e->navigation+k;TombCreatureInfo info={0};int16_t turn=0;int targetable=0;
        if(o->object==28 && !(o->flags&0x100) && c->health<=40){c->health=40;c->flags++;}
        if(c->health>0 || (o->object==33 && c->health<=-16384)) {
            int16_t bounds[6];if(!tomb_visual_bounds(w->visual,&lara->actor,bounds))return 0;
            if(o->object==33 || (o->object>=20 && o->object<=22)){n->step=256;n->drop=-256;n->fly=0;}
            tomb_creature_info(&info,c,n,w->level,&o->actor,o->object,&lara->actor,ctx->sine_quarter);
            if(((o->object>=30 && o->object<=33) || (o->object>=20 && o->object<=23) || o->object==27 || o->object==28) && info.ahead && info.distance<0x3100000){TombCameraPoint from={o->actor.x,o->actor.y-768,o->actor.z,o->actor.room},to={lara->actor.x,lara->actor.y-768,lara->actor.z,lara->actor.room};targetable=tomb_dos_camera_los(w->level,&from,&to,0);}
            if(o->object==28 && c->flags){info.enemy_zone=-1;o->actor.flags|=16;}
            tomb_atlantean_flight(o,c,&info,targetable);
            if(o->object==33){
                int was_flying=o->actor.current==2 && (c->flags&0x8000) && c->health>200;
                tomb_natla_flight(o,c,&info,targetable,&e->random);
                if(was_flying){
                    if(!(c->flags&0x8000))tomb_creature_mood(c,n,w->level,&o->actor,23,&info,&lara->actor,lara->health,lara->water_status,bounds[2],&e->random);
                    n->step=20480;n->drop=-20480;n->fly=32;
                    tomb_creature_info(&info,c,n,w->level,&o->actor,33,&lara->actor,ctx->sine_quarter);
                }
                if(c->health<=200 || o->actor.current!=2 || (c->flags&0x8000))tomb_creature_mood(c,n,w->level,&o->actor,c->health<=200?23:33,&info,&lara->actor,lara->health,lara->water_status,bounds[2],&e->random);
                if(c->health>200)o->actor.yaw=tomb_word(o->actor.yaw-c->yaw_offset);
            }else if(o->object==20 && o->actor.current==13){
                if(!(c->flags&4))tomb_creature_mood(c,n,w->level,&o->actor,o->object,&info,&lara->actor,lara->health,lara->water_status,bounds[2],&e->random);
                n->step=30720;n->drop=-30720;n->fly=32;
                TombCreatureInfo flying={0};tomb_creature_info(&flying,c,n,w->level,&o->actor,o->object,&lara->actor,ctx->sine_quarter);
                if(c->flags&4)tomb_creature_mood(c,n,w->level,&o->actor,o->object,&flying,&lara->actor,lara->health,lara->water_status,bounds[2],&e->random);
            }else tomb_creature_mood(c,n,w->level,&o->actor,o->object,&info,&lara->actor,lara->health,lara->water_status,bounds[2],&e->random);
            /* DOS CrocodileControl 192bc skips ordinary steering in state 4;
               folly_control applies that state's fixed 1092-unit turn. */
            turn=o->object==34 || (o->object==10 && o->actor.current==4)?0:tomb_creature_turn(&o->actor,c,o->object==33?(c->health<=200?1092:910):o->object==23 || o->object==30?728:o->object==9?3640:o->object==16?1092:o->object==17 || o->object==10 || o->object==11?546:c->maximum_turn);
        } else if(o->object==8)turn=o->object==34?0:tomb_creature_turn(&o->actor,c,182);
        TombModel model;if(!tomb_visual_model(w->visual,o->object,&model))return 0;
        /* PierreControl 32c44: final encounter drops all three items on
           entering death, before advancing the death animation. */
        if(o->object==28 && (o->flags&0x100) && c->health<=0 && o->actor.current!=5){
            const int drops[]={86,144,129};
            for(int j=0;j<3;j++)if(tomb_objects_spawn(w,drops[j],&o->actor)<0)return 0;
        }
        if(o->object>=30 && o->object<=32 && c->health<=0 && o->actor.current!=5){if(tomb_objects_spawn(w,o->object==30?87:o->object==31?86:85,&o->actor)<0)return 0;}
        if(w->invincible && o->object==18){info.bite=0;c->touch=0;if(o->actor.current==7 || o->actor.goal==7 || o->actor.goal==8){o->actor.animation=(int16_t)model.animation;o->actor.frame=ctx->animations[model.animation].first_frame;o->actor.current=o->actor.goal=1;}}
        if(w->invincible && o->object==34)c->touch=0;
        int16_t torso_angle=tomb_word(tomb_object_angle(c->target_z-o->actor.z,c->target_x-o->actor.x)-o->actor.yaw);
        int natla_recover=o->object==33 && o->actor.current==5 && (c->flags&32767)==480;
        TombCreatureDecision d=o->object==33?tomb_natla_control(o,c,&info,turn,targetable):o->object==34?tomb_torso_control(o,c,&info,torso_angle,lara->health,&e->random,ctx,model.animation):(o->object>=30 && o->object<=32)?tomb_mines_control(o,c,&info,turn,&e->random,ctx,model.animation,targetable):(o->object>=20 && o->object<=23)?tomb_atlantean_control(o,c,&info,turn,&e->random,ctx,model.animation,targetable):o->object==27?tomb_larson_control(o,c,&info,turn,&e->random,ctx,model.animation,targetable):(o->object==16 || o->object==17 || o->object==10 || o->object==11 || o->object==12 || o->object==13 || o->object==14 || o->object==15 || o->object==28)?tomb_folly_control(o,c,&info,turn,&e->random,ctx,model.animation,targetable):tomb_creature_control(o,c,&info,turn,lara->health,&e->random,ctx,model.animation);
        if(natla_recover && ctx->event)ctx->event(ctx->user,11,54,&o->actor);
        if(o->object==30 && c->health<120 && ctx->event)ctx->event(ctx->user,11,56,&o->actor);
        if(w->invincible){d.damage=0;if(d.blood<172 && !(o->object>=27 && o->object<=32))d.blood=0;}
        if(d.damage){lara->health=tomb_word(lara->health-d.damage);if(o->object!=18 || d.damage>=10000)lara->actor.flags|=16;}
        if((o->object==18 || o->object==34) && d.damage>=10000){
            /* Lara's dedicated T. rex death is animation 1 of model 5. */
            TombModel extra;if(!tomb_visual_model(w->visual,5,&extra) || (size_t)extra.animation+1>=ctx->animation_count)return 0;
            TombActor *a=&lara->actor;a->x=o->actor.x;a->y=o->actor.y;a->z=o->actor.z;a->room=o->actor.room;a->yaw=o->actor.yaw;
            a->animation=(int16_t)(extra.animation+(o->object==18));a->frame=ctx->animations[a->animation].first_frame;a->current=a->goal=46;a->flags&=(uint8_t)~8u;
            lara->pitch=0;lara->health=lara->air=-1;ctx->weapon_status=1;
        }
        if(d.blood>=172 && d.blood<=174){
            int32_t muzzle[3]={o->object==23?11:d.blood==172?-35:51,o->object==23?415:d.blood==172?269:213,o->object==23?41:0};
            int joint=o->object==23?13:d.blood==172?9:14;
            if(o->object==33){muzzle[0]=5;muzzle[1]=220;muzzle[2]=7;joint=4;}
            if(!tomb_object_joint_joints(w->visual,o->object,&o->actor,c->pitch,c->roll,c->head,c->neck,joint,muzzle,ctx->sine_quarter))return 0;
            TombActor source=o->actor;source.x=muzzle[0];source.y=muzzle[1];source.z=muzzle[2];
            int fx=tomb_missile_spawn(w,d.blood==174?173:d.blood,&source,&lara->actor,ctx->sine_quarter);
            if(o->object==23 && fx>=0)c->neck=w->hazards->effects[fx].frame;
            if(o->object==33){
                if(fx>=0){c->head=w->hazards->effects[fx].frame;if(d.blood!=174 && ctx->event)ctx->event(ctx->user,5,123,&source);}
                if(d.blood==174)for(int extra=0;extra<2;extra++){fx=tomb_missile_spawn(w,173,&source,&lara->actor,ctx->sine_quarter);if(fx>=0)w->hazards->effects[fx].yaw=tomb_word(w->hazards->effects[fx].yaw+((int)tomb_control_random(&e->random)-16384)/4);}
            }
        }else if((o->object==27 || o->object==28 || (o->object>=30 && o->object<=32)) && d.blood){
            if(d.damage && ctx->event)ctx->event(ctx->user,5,50,&lara->actor);
            if(d.damage){
                int joint=tomb_control_random(&e->random)*25/32767;
                int32_t point[3]={0,0,0};TombActor blood=lara->actor;
                if(joint<15 && tomb_object_joint(w->visual,0,&lara->actor,lara->pitch,lara->lean,0,joint,point,ctx->sine_quarter)){blood.x=point[0];blood.y=point[1];blood.z=point[2];}
                tomb_effect_spawn(w,158,&blood,lara->actor.speed,lara->actor.yaw);
            }
            for(int gun=0;gun<(o->object==28 || o->object==30 || o->object==31?2:1);gun++){
            if(o->object>=30 && !(d.blood&(1<<gun)))continue;
            int32_t muzzle[3]={o->object==28?(gun?-57:60):-60,o->object==28?200:170,0};
            int joint=o->object==28 && !gun?11:14;
            if(o->object==30){muzzle[0]=0;muzzle[1]=150;muzzle[2]=gun?37:34;joint=gun?4:7;}
            if(o->object==31){muzzle[0]=gun?-2:1;muzzle[1]=200;muzzle[2]=gun?40:41;joint=gun?8:5;}
            if(o->object==32){muzzle[0]=-20;muzzle[1]=440;muzzle[2]=20;joint=9;}
            if(tomb_object_joint(w->visual,o->object,&o->actor,c->pitch,c->roll,c->head,joint,muzzle,ctx->sine_quarter))for(int j=0;j<256;j++)if(!w->hazards->effects[j].active){
                w->hazards->effects[j]=(TombHazardEffect){muzzle[0],muzzle[1],muzzle[2],o->actor.room,166,0,3,0,tomb_word(o->actor.yaw+info.angle),1};break;
            }
            }
        }else if(d.blood){
            int32_t bite[3]={0,o->object==7?-14:o->object==8?96:o->object==19?66:16,o->object==7?174:o->object==8?335:o->object==19?318:45};
            int joint=o->object==7?6:o->object==8?14:o->object==19?22:4;
            if(o->object==16 || o->object==17){bite[0]=0;bite[1]=-11;bite[2]=108;joint=3;}
            if(o->object==12 || o->object==13 || o->object==14){bite[0]=-2;bite[1]=-10;bite[2]=132;joint=21;}
            if(o->object==15){bite[0]=0;bite[1]=-19;bite[2]=75;joint=15;}
            if(o->object==10 || o->object==11){bite[0]=5;bite[1]=-21;bite[2]=467;joint=9;}
            if(o->object>=20 && o->object<=22){bite[0]=-27;bite[1]=98;bite[2]=0;joint=10;}
            if(o->object==23){bite[0]=50;bite[1]=30;bite[2]=0;joint=5;}
            if(!tomb_object_joint_joints(w->visual,o->object,&o->actor,c->pitch,c->roll,c->head,c->neck,joint,bite,ctx->sine_quarter))return 0;
            TombActor point=o->actor;point.x=bite[0];point.y=bite[1];point.z=bite[2];tomb_effect_spawn(w,158,&point,o->actor.speed,o->actor.yaw);
        }
        int oldx=o->actor.x,oldy=o->actor.y,oldz=o->actor.z;
        if(o->object==15 && o->actor.current!=11){
            if(c->flags&2){o->actor.yaw=tomb_word(o->actor.yaw-16384);c->flags-=2;}
            else if(o->flags&4){o->actor.yaw=tomb_word(o->actor.yaw+16384);c->flags-=4;}
        }
        int moved;
        int rat_water=o->object==17?tomb_water_height(w->level,o->actor.x,o->actor.z,o->actor.room):-32512;
        if(o->object==34 && o->actor.current==9){
            if(!tomb_object_animate(o,ctx))return 0;moved=1;
            if(o->actor.y>c->floor){o->actor.y=c->floor;o->actor.goal=1;o->actor.flags&=(uint8_t)~8u;if(ctx->event)ctx->event(ctx->user,10,500,&o->actor);}
        }else if(o->object==17 && c->health<=0){
            if(!tomb_object_animate(o,ctx))return 0;
            moved=(o->actor.flags&6)!=4;
            if(!moved){
                c->health=-16384;o->actor.flags&=(uint8_t)~32u;
                /* DOS WaterRatControl keeps Cistern corpses updating for draining water. */
                if(w->level->dos_level==8){o->actor.flags=(o->actor.flags&~6u)|2;moved=1;}
                else{o->active=0;o->actor.flags&=(uint8_t)~1u;}
            }
            if(rat_water==-32512){TombModel land;if(tomb_visual_model(w->visual,16,&land)){o->object=16;o->actor.animation=(int16_t)(land.animation+8);o->actor.frame=ctx->animations[o->actor.animation].first_frame;o->actor.current=o->actor.goal=5;}}
        }else if(o->object==11 && c->health<=0){
            int height=tomb_water_height(w->level,o->actor.x,o->actor.z,o->actor.room);
            if(height!=-32512){if(o->actor.y>height+32)o->actor.y-=32;else if(o->actor.y<height)o->actor.y=height;}
            else{TombModel m;if(!tomb_visual_model(w->visual,10,&m))return 0;o->object=10;o->actor.animation=(int16_t)(m.animation+11);o->actor.frame=ctx->animations[o->actor.animation].first_frame;o->actor.current=o->actor.goal=7;o->actor.y=c->floor;c->pitch=0;}
            if(!tomb_object_animate(o,ctx))return 0;
            TombSectorRef ref;if(tomb_find_sector(w->level,o->actor.x,o->actor.y,o->actor.z,o->actor.room,&ref))o->actor.room=(int16_t)ref.room;
            moved=1;
        }else{
            if(o->object==10 || o->object==11){
                int wet=(w->level->rooms[o->actor.room].flags&1)!=0;
                int water=tomb_water_height(w->level,o->actor.x,o->actor.z,o->actor.room);
                int change=o->object==10?wet:water==-32512;
                if(change){
                    int object=o->object==10?11:10;TombModel m;if(!tomb_visual_model(w->visual,object,&m))return 0;
                    o->object=(int16_t)object;o->actor.animation=(int16_t)m.animation;o->actor.frame=ctx->animations[m.animation].first_frame;
                    o->actor.current=o->actor.goal=ctx->animations[m.animation].state;c->pitch=0;
                    n->step=object==11?20480:256;n->drop=object==11?-20480:-256;n->fly=object==11?16:0;
                    if(object==10)o->actor.y=c->floor;
                }else if(o->object==11 && o->actor.y<water+256){
                    /* DOS 191b8..191c2: clamp living swimmers before CreatureAnimation. */
                    o->actor.y=water+256;
                }
            }
            if(o->object==16 || o->object==17){
                int water=tomb_water_height(w->level,o->actor.x,o->actor.z,o->actor.room);
                int next_object=o->object==16 && water!=-32512?17:o->object==17 && water==-32512?16:o->object;
                if(next_object!=o->object){
                    TombModel m;if(tomb_visual_model(w->visual,next_object,&m)){
                        o->object=(int16_t)next_object;o->actor.animation=(int16_t)m.animation;
                        o->actor.frame=ctx->animations[m.animation].first_frame;o->actor.current=o->actor.goal=ctx->animations[m.animation].state;
                        if(next_object==17){o->actor.y=water;oldy=water;rat_water=water;}
                    }
                }
                if(o->object==17)o->actor.y=c->floor;
            }
            if(o->object==33)o->actor.yaw=tomb_word(o->actor.yaw-c->yaw_offset);
            moved=tomb_creature_move(o,c,n,w,ctx,d.turn,o->object==27 || o->object==28 || (o->object>=30 && o->object<=33)?0:d.tilt);if(moved<0)return 0;
            if(o->object==33)o->actor.yaw=tomb_word(o->actor.yaw+c->yaw_offset);
        }
        if(o->object==17 && c->health>0 && rat_water!=-32512){int dy=rat_water-oldy;o->actor.y=oldy+(dy< -32?-32:dy>32?32:dy);}
        if(o->object==15 && o->actor.current!=11 && o->actor.y<=oldy-384){
            int x=o->actor.x>>10,z=o->actor.z>>10,px=oldx>>10,pz=oldz>>10;
            if(z==pz && x!=px){o->actor.yaw=x>px?16384:-16384;o->actor.x=x>px?(x*1024-75):(px*1024+75);}
            else if(x==px && z!=pz){o->actor.yaw=z>pz?0:-32768;o->actor.z=z>pz?(z*1024-75):(pz*1024+75);}
            if(x!=px || z!=pz){o->actor.animation=(int16_t)(model.animation+19);o->actor.frame=ctx->animations[o->actor.animation].first_frame;o->actor.current=11;o->actor.y=oldy;}
        }
        if(o->object==28 && c->flags){
            TombCameraPoint from={e->camera[0],e->camera[1],e->camera[2],lara->actor.room},to={o->actor.x,o->actor.y-1024,o->actor.z,o->actor.room};
            TombSectorRef ref;if(tomb_find_sector(w->level,from.x,from.y,from.z,from.room,&ref))from.room=(int16_t)ref.room;
            if(tomb_dos_camera_los(w->level,&from,&to,0))c->flags=1;
            else if(c->flags>10){c->health=-16384;o->actor.flags=(o->actor.flags&~33u)|6;o->active=0;o->flags|=0x100;moved=0;}
        }
        if(o->object==28 && tomb_water_height(w->level,o->actor.x,o->actor.z,o->actor.room)!=-32512){c->health=-16384;o->actor.flags=(o->actor.flags&~33u)|6;o->active=0;o->flags|=0x100;moved=0;}
        if(o->object==18)o->actor.flags|=32;
        tomb_enemies_room_sync(w);
        w->level->items[index].state=o->actor.current;
        if(!moved){
            if(o->object==34){TombSectorRef r;TombHeights h;if(!tomb_find_sector(w->level,o->actor.x,o->actor.y,o->actor.z,o->actor.room,&r) || !tomb_item_heights(w->level,r,o->actor.x,o->actor.y,o->actor.z,0,w->level->items,w->count,&h) || !tomb_objects_trigger_heavy(w,h.trigger_index))return 0;}
            if(o->object==23 || o->object==34){if(!tomb_explode_meshes(w,o,c,UINT32_MAX,o->object==34?250:100,ctx->sine_quarter))return 0;if(ctx->event)ctx->event(ctx->user,5,171,&o->actor);o->actor.flags|=6;}
            tomb_navigation_free(n);e->slots[k]=-1;if(previous<0)e->head=next;else e->next[previous]=next;e->next[index]=-1;
        }
        else previous=index;
        index=next;
    }
    return 1;
}
