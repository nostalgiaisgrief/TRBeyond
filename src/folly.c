/* Folly controllers reconstructed from DOS: lightning 2c5f4, hammer 2c97c,
   Damocles sword 3aa48. Per-item auxiliary state uses the persisted creature
   slots, also used by the existing rolling-ball/falling-ceiling controllers. */
#include "folly.h"
#include "playtest.h"
#include "enemies.h"
#include "hazards.h"
#include "peru.h"
#include "city.h"
#include "fixed.h"
#include "object_contact.h"
#include "inventory.h"
#include <stdlib.h>
/* DOS Lara_State_MidasDeath 25d14: accumulated mesh replacements. */
uint32_t tomb_midas_gold(int frame){
    static const int frames[]={120,90,70,5,120,100,5,135,186,195,218,150,163,174,225};
    uint32_t mask=0;for(int j=0;j<15;j++)if(frame>=frames[j])mask|=1u<<j;return mask;
}
int tomb_midas_teeth(TombObject *o,uint32_t touch){int active=tomb_trigger_active(o);o->actor.goal=(int16_t)active;return active && o->actor.current==1 && touch?400:0;}
int tomb_midas_contact(TombPlaytest *p,uint32_t input){
    TombObjects *w=p->objects;TombActor *a=&p->lara.actor;
    if(a->current==50 || a->current==51)return 1;
    for(size_t j=0;j<w->count;j++){
        TombObject *o=w->items+j;if(o->object!=128 || o->actor.room!=a->room)continue;
        if(a->current!=2 || (a->flags&8))continue;
        int dx=a->x-o->actor.x,dz=a->z-o->actor.z;
        int death=dx> -512 && dx<512 && dz> -512 && dz<512;
        if(death && (p->gameplay_options&TOMB_INVINCIBLE))continue;
        if(!death){
            if(p->animation.weapon_status || (!(input&64) && p->inventory.chosen<0))continue;
            o->actor.yaw=tomb_word(((uint16_t)(a->yaw+8192)/16384)*16384);
            int dy=a->y-o->actor.y;int yaw=tomb_word(a->yaw-o->actor.yaw);
            if(dx< -700 || dx>700 || dz< -700 || dz>700 || dy<284 || dy>996 || abs(p->lara.pitch)>1820 || abs(p->movement.lean)>1820 || abs(yaw)>5460)continue;
            if(p->inventory.chosen<0){if(!p->quest_latch){p->quest_latch=1;if(p->inventory.counts[8])p->quest_request=1;else if(p->animation.event)p->animation.event(p,5,2,a);}continue;}
            if(p->inventory.chosen!=127 || !p->inventory.counts[8])continue;
            --p->inventory.counts[8];++p->inventory.quest[0];p->inventory.chosen=-1;
        }
        TombModel m;if(!tomb_visual_model(w->visual,5,&m))return 0;
        a->animation=(int16_t)(m.animation+death);a->frame=p->level->animations[a->animation].first_frame;
        a->current=a->goal=(int16_t)(death?51:50);a->flags&=(uint8_t)~8u;p->animation.weapon_status=1;
        if(death){p->lara.health=p->lara.air=-1;w->peru->scion_item=(int)j;w->peru->scion_frame=0;w->peru->scion_origin=*a;}
        return 1;
    }
    return 1;
}
/* FlameEmitter / FlameControl 3afb8 / 3b060. Effect counter is the
   original burn-joint/cooldown field; emission ownership is stored as index+1. */
int tomb_midas_tick(TombPlaytest *p){
    TombObjects *w=p->objects;TombActor *a=&p->lara.actor;TombAnimContext *ctx=&p->animation;
    unsigned char flames[256];
    for(int j=0;j<256;j++)flames[j]=(unsigned char)(w->hazards->effects[j].active && w->hazards->effects[j].id==178);
    for(int j=0;j<256;j++){
        TombHazardEffect *e=w->hazards->effects+j;if(!flames[j])continue;
        int frames;if(tomb_hazard_sprite(w->visual,178,0,&frames)<0)return 0;e->frame=(int16_t)((e->frame+1)%frames);
        if(e->counter<0){
            if(p->gameplay_options&TOMB_INVINCIBLE){e->active=0;continue;}
            int32_t pos[3]={0,0,e->counter==-1?-100:0};
            if(!tomb_object_joint(w->visual,0,a,p->lara.pitch,p->movement.lean,0,(-e->counter-1)>14?14:-e->counter-1,pos,ctx->sine_quarter))return 0;
            e->x=pos[0];e->y=pos[1];e->z=pos[2];
            int water=tomb_water_height(w->level,e->x,e->z,e->room);
            if(water!=-32512 && water<e->y){e->active=0;if(ctx->event)ctx->event(p,7,150,a);continue;}
            p->lara.health=tomb_word(p->lara.health-5);a->flags|=16;
        }else if(e->counter)--e->counter;
        else{
            int dx=e->x-a->x,dy=e->y-a->y,dz=e->z-a->z;int16_t bounds[6];
            if(!tomb_visual_bounds(w->visual,a,bounds))return 0;
            if(!(p->gameplay_options&TOMB_INVINCIBLE) && abs(dx)<=600 && abs(dz)<=600 && abs(dy)<=3072 && (int64_t)dx*dx+(int64_t)dz*dz<=360000 && dy>=bounds[2] && dy<=bounds[3]+100){
                p->lara.health=tomb_word(p->lara.health-3);a->flags|=16;
                if((int64_t)dx*dx+(int64_t)dz*dz<90000){e->counter=100;int fx=tomb_effect_spawn(w,178,a,0,0);if(fx>=0)w->hazards->effects[fx].counter=-1;}
            }
        }
        if(ctx->event){TombActor sound=*a;sound.x=e->x;sound.y=e->y;sound.z=e->z;ctx->event(p,5,150,&sound);}
    }
    for(size_t j=0;j<w->count;j++){
        TombObject *o=w->items+j;if(!o->active)continue;
        TombCreature *c=w->enemies->items+j;
        if(o->object==179){
            if(tomb_trigger_active(o)){if(!c->flags){int fx=tomb_effect_spawn(w,178,&o->actor,0,0);c->flags=(int16_t)(fx+1);}}
            else if(c->flags){w->hazards->effects[c->flags-1].active=0;c->flags=0;if(ctx->event)ctx->event(p,7,150,&o->actor);}
        }else if(o->object==42){
            int damage=tomb_midas_teeth(o,c->touch);
            if(damage && !w->invincible){
                p->lara.health=tomb_word(p->lara.health-damage);a->flags|=16;
                for(int k=0;k<6;k++){int32_t pos[3]={k%2?71:-23,k/2==1?10:k/2==2?-10:0,-1718};if(!tomb_object_joint(w->visual,42,&o->actor,0,0,0,k%2,pos,ctx->sine_quarter))return 0;TombActor blood=o->actor;blood.x=pos[0];blood.y=pos[1];blood.z=pos[2];tomb_effect_spawn(w,158,&blood,o->actor.speed,o->actor.yaw);}
            }
            c->touch=0;if(!tomb_object_animate(o,ctx))return 0;
        }
    }
    return 1;
}
/* DOS Midas sparkle 1d8c0, called before Lara's animation advances. */
int tomb_midas_sparkles(TombPlaytest *p){
    TombActor *a=&p->lara.actor;if(a->current!=50 && a->current!=51)return 1;
    TombObjects *w=p->objects;TombSphere spheres[64];
    int count=tomb_object_spheres(w->visual,0,a,p->lara.pitch,p->movement.lean,p->animation.sine_quarter,spheres);
    if(count<0)return 0;
    uint32_t mask=a->current==50?0x2400:tomb_midas_gold(a->frame-p->level->animations[a->animation].first_frame);
    for(int j=0;j<count && j<15;j++)if(mask&(1u<<j)){
        TombActor sparkle=*a;TombSphere *s=spheres+j;
        sparkle.x=s->x+((int)tomb_control_random(&w->hazards->random)-16384)*s->radius/16384;
        sparkle.y=s->y+((int)tomb_control_random(&w->hazards->random)-16384)*s->radius/16384;
        sparkle.z=s->z+((int)tomb_control_random(&w->hazards->random)-16384)*s->radius/16384;
        tomb_effect_spawn(w,165,&sparkle,0,0);
    }
    return 1;
}
int tomb_folly_object(int id){return id>=43 && id<=47;}
static int folly_floor(TombObjects *w,TombActor *a,TombHeights *h){
    TombSectorRef r;if(!tomb_find_sector(w->level,a->x,a->y,a->z,a->room,&r) || !tomb_item_heights(w->level,r,a->x,a->y,a->z,0,w->level->items,w->count,h))return 0;
    a->room=(int16_t)r.room;return 1;
}
void tomb_folly_init(TombObjects *w,size_t i){
        TombObject *o=w->items+i;TombCreature *c=w->enemies->items+i;
        if(o->object==43){
            o->actor.yaw=(int16_t)tomb_control_random(&w->enemies->random);
            c->required=(int16_t)(((int)tomb_control_random(&w->enemies->random)-16384)/16);
            o->actor.fall_speed=50;TombHeights h;if(folly_floor(w,&o->actor,&h))c->floor=h.floor;
        }else if(o->object==46){c->required=1;c->flags=0;}
}
int tomb_folly_head(const TombObjects *w,const TombObject *handle,TombObject *head){
    TombModel a,b;if(handle->object!=44 || !tomb_visual_model(w->visual,44,&a) || !tomb_visual_model(w->visual,45,&b))return 0;
    *head=*handle;head->object=45;
    int animation=b.animation+handle->actor.animation-a.animation;
    if(animation<0 || (size_t)animation>=w->level->animation_count)return 0;
    head->actor.animation=(int16_t)animation;
    head->actor.frame=(int16_t)(w->level->animations[animation].first_frame+handle->actor.frame-w->level->animations[handle->actor.animation].first_frame);
    return 1;
}
static void folly_sound(TombAnimContext *c,int sound,TombActor *a){if(c->event)c->event(c->user,5,sound,a);}
int tomb_lava_touch(TombPlaytest *p){
    if(p->lara.health<0 || (p->gameplay_options&TOMB_INVINCIBLE))return 1;
    TombActor *a=&p->lara.actor,probe=*a;TombHeights current,bottom;
    if(!folly_floor(p->objects,&probe,&current))return 0;probe.y=32000;
    if(!folly_floor(p->objects,&probe,&bottom))return 0;if(current.floor!=bottom.floor)return 1;
    p->lara.health=-1;a->flags|=16;
    int frames;if(tomb_hazard_sprite(p->visual,178,0,&frames)<0)return 1;
    for(int j=0;j<10;j++){int fx=tomb_effect_spawn(p->objects,178,a,0,0);if(fx<0)continue;
        TombHazardEffect *e=p->objects->hazards->effects+fx;e->frame=(int16_t)(frames*tomb_control_random(&p->objects->enemies->random)/32767);
        e->counter=(int16_t)(-(int)tomb_control_random(&p->objects->enemies->random)*24/32767-1);
    }return 1;
}
/* Doppelganger 291b4, waterfall 1dd4c, Scion 30184, earthquake 30310. */
int tomb_late_world_tick(TombPlaytest *p){
    TombObjects *w=p->objects;TombActor *lara=&p->lara.actor;TombAnimContext *ctx=&p->animation;
    for(size_t j=0;j<w->count;j++){
        TombObject *o=w->items+j;TombActor *a=&o->actor;TombCreature *c=w->enemies->items+j;
        if(!o->active)continue;
        if(o->object==6){
            if(c->health<1000){if(!w->invincible)p->lara.health=tomb_word(p->lara.health-10*(1000-c->health));c->health=1000;}
            if(!c->flags){
                TombActor mirror=*lara;mirror.x=0x12000-lara->x;mirror.z=0x1e000-lara->z;mirror.room=a->room;
                TombHeights mh,lh;if(!folly_floor(w,&mirror,&mh))return 0;
                TombActor probe=*lara;if(!folly_floor(w,&probe,&lh))return 0;
                a->x=mirror.x;a->y=mirror.y;a->z=mirror.z;a->room=lara->room;a->animation=lara->animation;a->frame=lara->frame;a->yaw=tomb_word(lara->yaw-32768);
                c->pitch=p->lara.pitch;c->roll=p->movement.lean;c->floor=mh.floor;
                if(mh.floor>=lh.floor+1024 && !(lara->flags&8)){a->goal=a->current=9;a->animation=32;a->frame=481;a->fall_speed=a->speed=0;a->flags|=8;a->y+=50;c->flags=-1;}
            }
            if(c->flags){
                if(!tomb_object_animate(o,ctx))return 0;
                TombHeights h;if(!folly_floor(w,a,&h))return 0;c->floor=h.floor;
                if(!tomb_objects_trigger_heavy(w,h.trigger_index))return 0;
                if(a->y>=h.floor){a->y=h.floor;if(!folly_floor(w,a,&h) || !tomb_objects_trigger_heavy(w,h.trigger_index))return 0;a->fall_speed=0;a->goal=c->required=8;a->flags&=(uint8_t)~8u;}
            }
        }else if(o->object==145){
            if(c->health>0){c->flags=0;if(!tomb_object_animate(o,ctx))return 0;}
            else{
                if(!c->flags){c->health=-16384;a->flags|=6;TombHeights h;if(!folly_floor(w,a,&h) || !tomb_objects_trigger_heavy(w,h.trigger_index))return 0;}
                if(c->flags%10==0){
                    TombActor point=*a;point.x+=((int)tomb_control_random(&w->enemies->random)-16384)/32;point.y+=((int)tomb_control_random(&w->enemies->random)-16384)/256-500;point.z+=((int)tomb_control_random(&w->enemies->random)-16384)/32;
                    if(tomb_effect_spawn(w,151,&point,0,0)>=0){folly_sound(ctx,104,&point);if(ctx->event)ctx->event(ctx->user,10,-200,a);}
                }
                if(++c->flags==90)o->active=0;
            }
        }else if(o->object==170){
            if((o->flags&0x3e00)==0x3e00 && abs(a->x-lara->x)<=10240 && abs(a->y-lara->y)<=10240 && abs(a->z-lara->z)<=10240){
                TombActor point=*a;point.x+=((int)tomb_control_random(&w->hazards->random)-16384)*1024/32767;point.z+=((int)tomb_control_random(&w->hazards->random)-16384)*1024/32767;tomb_effect_spawn(w,153,&point,0,0);
            }
        }else if(o->object==180){
            TombHeights h;if(!folly_floor(w,a,&h))return 0;
            if((a->flags&6)!=4){TombActor probe=*a;int dx=a->yaw==16384?1:a->yaw==0 || a->yaw==-32768?0:-1,dz=a->yaw==0?1:a->yaw==-32768?-1:0;
                a->x+=25*dx;a->z+=25*dz;probe.x+=2048*dx;probe.z+=2048*dz;
                if(!folly_floor(w,&probe,&h))return 0;if(h.floor!=a->y)a->flags=(a->flags&~6u)|4;
            }
            if(c->touch && !w->invincible){if(p->lara.health>0 && !tomb_lava_touch(p))return 0;p->camera_request=(TombCameraRequest){3072,1,3,-32768,0,0,-1,(int16_t)j};}
            c->touch=0;
        }else if(o->object==183 && tomb_trigger_active(o)){
            int sound=-1,bounce=0;if(tomb_control_random(&w->hazards->random)<256){sound=147;bounce=-150;}else if(tomb_control_random(&w->enemies->random)<1024){sound=70;bounce=50;}
            if(sound>=0 && ctx->event){ctx->event(ctx->user,9,(int16_t)sound,a);ctx->event(ctx->user,10,(int16_t)bounce,a);}
        }
    }
    return 1;
}
void tomb_folly_sword(TombObject *o,TombCreature *c,const TombActor *lara,TombAnimContext *ctx){
    TombActor *a=&o->actor;
    if(a->flags&8){
        a->yaw=tomb_word(a->yaw+c->required);a->fall_speed=tomb_word(a->fall_speed+(a->fall_speed<128?6:1));
        a->y+=a->fall_speed;a->x+=a->current;a->z+=a->goal;
        if(a->y>c->floor){folly_sound(ctx,103,a);a->y=c->floor+10;a->flags=(a->flags&~15u)|4;o->active=0;}
    }else if(a->y!=c->floor){
        a->yaw=tomb_word(a->yaw+c->required);int dx=lara->x-a->x,dy=lara->y-a->y,dz=lara->z-a->z;
        if(abs(dx)<=1536 && abs(dz)<=1536 && dy>0 && dy<3072){a->current=(int16_t)(dx/32);a->goal=(int16_t)(dz/32);a->flags|=8;}
    }
}
int tomb_folly_tick(TombPlaytest *p){
    TombObjects *w=p->objects;TombAnimContext *ctx=&p->animation;TombActor *lara=&p->lara.actor;
    for(size_t i=0;i<w->count;i++){
        TombObject *o=w->items+i;TombActor *a=&o->actor;TombCreature *c=w->enemies->items+i;
        if(!o->active)continue;
        if(o->object==43){
            tomb_folly_sword(o,c,lara,ctx);
        }else if(o->object==44){
            if(a->current==0){if(tomb_trigger_active(o))a->goal=1;else {o->active=0;a->flags&=(uint8_t)~7u;}}
            else if(a->current==1)a->goal=tomb_trigger_active(o)?2:0;
            else if(a->current==2 && a->frame-ctx->animations[a->animation].first_frame>30){
                int q=(uint16_t)a->yaw/16384;const int dx[]={0,3072,0,-3072},dz[]={3072,0,-3072,0};
                if(!(p->gameplay_options&TOMB_INVINCIBLE) && p->lara.health>=0 && abs(lara->x-a->x-dx[q])<520 && abs(lara->z-a->z-dz[q])<520){
                    p->lara.health=-1;lara->animation=139;lara->frame=3561;lara->current=lara->goal=46;lara->y=a->y;lara->flags&=(uint8_t)~8u;
                }
            }else if(a->current==3){
                TombHeights h;if(!folly_floor(w,a,&h) || !tomb_objects_trigger_heavy(w,h.trigger_index))return 0;
                int q=(uint16_t)a->yaw/16384;const int dx[]={0,3072,0,-3072},dz[]={3072,0,-3072,0};
                int x=a->x,z=a->z;a->x+=dx[q];a->z+=dz[q];
                if(p->lara.health>=0 && !tomb_block_floor(w,i,-2048))return 0;
                a->x=x;a->z=z;o->active=0;a->flags=(a->flags&~7u)|4;
            }
            if(!tomb_object_animate(o,ctx))return 0;
        }else if(o->object==46){
            if(!tomb_trigger_active(o)){
                c->required=1;c->flags=0;if(w->peru->flipped && !tomb_flip_map(w,1))return 0;o->active=0;a->flags&=(uint8_t)~7u;continue;
            }
            if(--c->required>0)continue;
            if(c->flags&1){c->flags=0;c->head=0;c->required=(int16_t)(35+tomb_control_random(&w->enemies->random)*45/32768);if(w->peru->flipped && !tomb_flip_map(w,1))return 0;}
            else{
                c->flags=1;c->required=20;TombModel model;if(!tomb_visual_model(w->visual,46,&model))return 0;
                int simple=model.mesh_count<=1,radius=simple?1024:2560;
                int dx=lara->x-a->x,dy=lara->y-a->y,dz=lara->z-a->z;
                /* NearLara 1d4f0 uses a horizontal circular range and Lara's vertical bounds. */
                int16_t bounds[6];if(!tomb_visual_bounds(w->visual,lara,bounds))return 0;
                int near=abs(dx)<=radius && abs(dz)<=radius && dx*dx+dz*dz<=radius*radius && abs(dy)<=3072 && -dy>=bounds[2] && -dy<=bounds[3]+100;
                if(near){c->target_x=lara->x;c->target_y=lara->y;c->target_z=lara->z;p->lara.health=tomb_word(p->lara.health-400);lara->flags|=16;c->flags|=2;}
                else if(simple){TombHeights h;if(!folly_floor(w,a,&h))return 0;c->target_x=a->x;c->target_y=h.floor;c->target_z=a->z;}
                else{int joint=1+tomb_control_random(&w->enemies->random)*5/32767;int32_t pos[3]={0,0,0};if(joint>=model.mesh_count)joint=model.mesh_count-1;if(!tomb_object_joint(w->visual,46,a,0,0,0,joint,pos,ctx->sine_quarter))return 0;c->target_x=pos[0];c->target_y=pos[1];c->target_z=pos[2];}
                c->touch=w->enemies->random; /* Branch seed, also persisted in saves. */
                for(int j=0;j<6;j++)tomb_control_random(&w->enemies->random);
                if(!w->peru->flipped && !tomb_flip_map(w,1))return 0;
            }
            folly_sound(ctx,98,a);
        }
    }
    return 1;
}
int tomb_folly_contact(TombPlaytest *p){
    TombObjects *w=p->objects;
    for(size_t i=0;i<w->count;i++)if(w->items[i].object==46 && (w->enemies->items[i].flags&2)){
        const TombActor *a=&w->items[i].actor,*lara=&p->lara.actor;
        if(abs(a->x-lara->x)>=4096 || abs(a->y-lara->y)>=4096 || abs(a->z-lara->z)>=4096)continue;
        p->door_hit_direction=tomb_control_random(&w->enemies->random)*4/32767;
        TombCreature *c=w->enemies->items+i;if(c->head<34)++c->head;p->door_hit_ticks=c->head;
    }
    for(size_t i=0;i<w->count;i++)if(w->items[i].object==43 && (w->items[i].actor.flags&8) && w->enemies->items[i].touch){
        if(w->invincible){w->enemies->items[i].touch=0;continue;}
        p->lara.health=tomb_word(p->lara.health-100);TombActor blood=p->lara.actor;
        blood.x+=((int)tomb_control_random(&w->enemies->random)-16384)/256;
        blood.z+=((int)tomb_control_random(&w->enemies->random)-16384)/256;
        blood.y-=tomb_control_random(&w->enemies->random)/44;
        int yaw=p->lara.actor.yaw+((int)tomb_control_random(&w->enemies->random)-16384)/8;
        tomb_effect_spawn(w,158,&blood,p->lara.actor.speed,tomb_word(yaw));w->enemies->items[i].touch=0;
    }
    return 1;
}
static int folly_clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
TombCreatureDecision tomb_folly_control(TombObject *o,TombCreature *c,const TombCreatureInfo *i,int16_t turn,uint32_t *random,const TombAnimContext *ctx,int base,int targetable){
    TombActor *a=&o->actor;TombCreatureDecision d={0,turn,0,0};int head=c->health>0 && i->ahead?i->angle:0;
    if(o->object==16 || o->object==17){
        int water=o->object==17;
        if(c->health<=0){d.turn=0;if(a->current!=(water?3:5)){a->animation=(int16_t)(base+(water?2:8));a->frame=ctx->animations[a->animation].first_frame;a->current=(int16_t)(water?3:5);}}
        else if(water){
            if(a->current==1 && i->ahead && (c->touch&0x300018f))a->goal=2;
            else if(a->current==2){if(!c->required && i->ahead && (c->touch&0x300018f)){d.damage=20;d.blood=1;c->required=1;}a->goal=0;}
        }else switch(a->current){
            case 1:a->goal=c->required?c->required:i->bite && i->distance<0x1c639?4:3;break;
            case 3:if(i->ahead && (c->touch&0x300018f))a->goal=1;else if(i->bite && i->distance<0x240000)a->goal=2;else if(i->ahead && tomb_control_random(random)<256){c->required=6;a->goal=1;}break;
            case 2:case 4:if(!c->required && i->ahead && (c->touch&0x300018f)){d.damage=20;d.blood=1;c->required=a->current==2?3:1;}break;
            case 6:if(c->mood || tomb_control_random(random)<256)a->goal=1;break;
            default:break;
        }
    }else if(o->object==12 || o->object==13 || o->object==14){
        if(c->health<=0){d.turn=0;if(a->current!=5){a->animation=(int16_t)(base+(o->object==14?4:7)+tomb_control_random(random)/16384);a->frame=ctx->animations[a->animation].first_frame;a->current=5;}}
        else switch(a->current){
        case 1:if(c->required)a->goal=c->required;else if(!c->mood)a->goal=2;else if(i->ahead && (c->touch&0x380066))a->goal=7;else if(i->ahead && i->distance<0x100000)a->goal=4;else a->goal=3;break;
        case 2:c->maximum_turn=364;if(c->mood)a->goal=1;else if(tomb_control_random(random)<128){c->required=6;a->goal=1;}break;
        case 3:c->maximum_turn=910;d.tilt=turn;if(!c->mood || (i->ahead && (i->distance<0x100000 || (c->touch&0x380066))))a->goal=1;else if(c->mood!=2 && tomb_control_random(random)<128){c->required=6;a->goal=1;}break;
        case 4:case 7:if(!c->required && (c->touch&0x380066)){d.damage=a->current==4?150:250;d.blood=a->current==7;c->required=1;}break;
        default:break;
        }
    }else if(o->object==15){
        if(c->health<=0){d.turn=0;if(a->current!=5){a->animation=(int16_t)(base+(o->object==14?4:7)+tomb_control_random(random)/16384);a->frame=ctx->animations[a->animation].first_frame;a->current=5;}}
        else{
            if((a->flags&16) || i->distance<0x400000)c->flags|=1;
            switch(a->current){
            case 1:
                if(c->flags&2){a->yaw=tomb_word(a->yaw-16384);c->flags-=2;}else if(o->flags&4){a->yaw=tomb_word(a->yaw+16384);c->flags-=4;}
                if(c->required)a->goal=c->required;
                else if(i->bite && i->distance<0x2d244)a->goal=4;
                else if(!(c->flags&1) && i->zone==i->enemy_zone && i->ahead){int r=tomb_control_random(random)/32;a->goal=r<160?10:r<320?6:r<480?7:r<752?8:9;if(a->goal>=8 && a->goal<=9)c->maximum_turn=0;}
                else a->goal=3;break;
            case 3:c->maximum_turn=910;
                if(!c->flags && i->angle> -8190 && i->angle<8190)a->goal=1;
                else if(i->ahead && (c->touch&0xff00)){c->required=4;a->goal=1;}
                else if(c->mood!=2){int r=tomb_control_random(random);if(r<480){c->required=r<160?10:r<320?6:7;a->goal=1;}}break;
            case 8:if(!(c->flags&4)){a->yaw=tomb_word(a->yaw-16384);c->flags|=4;}a->goal=1;break;
            case 9:if(!(c->flags&2)){a->yaw=tomb_word(a->yaw+16384);c->flags|=2;}a->goal=1;break;
            case 4:if(!c->required && (c->touch&0xff00)){d.damage=200;d.blood=1;c->required=1;}break;
            default:break;
            }
        }
    }else if(o->object==10){
        if(c->health<=0){d.turn=0;if(a->current!=7){a->animation=(int16_t)(base+11);a->frame=ctx->animations[a->animation].first_frame;a->current=7;}}
        else{
            if(a->current==4){a->yaw=tomb_word(a->yaw+1092);d.turn=0;}
            switch(a->current){
            case 1:if(i->bite && i->distance<0x2e329)a->goal=5;else if(c->mood==2)a->goal=2;else if(c->mood==1)a->goal=(i->angle< -16384 || i->angle>16384) && i->distance>0x900000?4:2;else if(c->mood==3)a->goal=3;break;
            case 2:if(i->ahead && (c->touch&0x3fc))a->goal=1;else if(c->mood==3)a->goal=3;else if(!c->mood)a->goal=1;else if(c->mood==1 && i->distance>0x900000 && (i->angle< -16384 || i->angle>16384))a->goal=1;break;
            case 3:if(i->ahead && (c->touch&0x3fc))a->goal=1;else if(c->mood==1 || c->mood==2)a->goal=2;else if(!c->mood)a->goal=1;break;
            case 4:if(i->angle> -16384 && i->angle<16384)a->goal=3;break;
            case 5:if(!c->required){d.damage=100;d.blood=1;c->required=1;}break;
            default:break;
            }
        }
    }else if(o->object==11){
        d.turn=turn;
        if(c->health<=0){d.turn=0;if(a->current!=3){a->animation=(int16_t)(base+4);a->frame=ctx->animations[a->animation].first_frame;a->current=3;c->health=-16384;}return d;}
        else if(a->current==1){if(i->bite && c->touch)a->goal=2;}
        else if(a->current==2){
            if(a->frame==ctx->animations[a->animation].first_frame)c->required=0;
            if(i->bite && c->touch){if(!c->required){d.damage=100;d.blood=1;c->required=1;}}else a->goal=1;
        }
    }else if(o->object==28){
        if(c->health<=0){d.turn=0;if(a->current!=5){a->animation=(int16_t)(base+12);a->frame=ctx->animations[a->animation].first_frame;a->current=5;}}
        else switch(a->current){
        case 1:if(c->required)a->goal=c->required;else if(!c->mood)a->goal=tomb_control_random(random)<96?6:2;else a->goal=c->mood==2?3:2;break;
        case 6:if(c->mood)a->goal=1;else if(tomb_control_random(random)<96){c->required=2;a->goal=1;}break;
        case 2:c->maximum_turn=546;
            if(!c->mood && tomb_control_random(random)<96){c->required=6;a->goal=1;}
            else if(c->mood==2){c->required=3;a->goal=1;}
            else if(targetable){c->required=4;a->goal=1;}
            else if(!i->ahead || i->distance>0x900000){c->required=3;a->goal=1;}break;
        case 3:c->maximum_turn=1092;d.tilt=turn/2;
            if(!c->mood && tomb_control_random(random)<96){c->required=6;a->goal=1;}
            else if(targetable){c->required=4;a->goal=1;}
            else if(i->ahead && i->distance<0x900000){c->required=2;a->goal=1;}break;
        case 4:if(c->required)a->goal=c->required;else if(targetable)a->goal=7;else a->goal=1;break;
        case 7:if(!c->required){for(int j=0;j<2;j++)if(i->distance<=0x3100000 && (int)tomb_control_random(random)<(0x3100000-i->distance)/0x620-0x2000)d.damage+=25;d.blood=2;c->required=4;}
            if(c->mood==2 && tomb_control_random(random)>8192)c->required=1;break;
        default:break;
        }
    }
    if(o->object==12 || o->object==13 || o->object==14 || o->object==28)c->roll=tomb_word(c->roll+folly_clamp(tomb_word(4*d.tilt-c->roll),-546,546));
    c->head=tomb_word(c->head+folly_clamp(tomb_word(head-c->head),-910,910));c->head=(int16_t)folly_clamp(c->head,-16384,16384);
    return d;
}
