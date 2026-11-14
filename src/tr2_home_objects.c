#include "tr2_world.h"
#include <math.h>
#include <stdlib.h>

static void sound(Tr2Actor *a,int id){if(a->event_count<16){int n=a->event_count++;a->event_kind[n]=5;a->event_value[n]=id;}}
/* State and sound decisions from WinstonControl, 0x437da0. Navigation is
   separate so its corridor choices can be compared independently. */
void tr2_winston_decide(Tr2Object *o,int distance,int ahead,int touch,int random){
    if(o->pose.control.current==1){
        if((distance>0x240000 || !ahead) && o->pose.control.goal!=2){o->pose.control.goal=2;sound(&o->pose,345);}
    }else if(distance<0x240000){
        if(ahead){o->pose.control.goal=1;o->timer&=(int16_t)~1;}
        else if(!(o->timer&1)){sound(&o->pose,344);sound(&o->pose,347);o->timer|=1;}
    }
    if(touch){if(!(o->timer&2)){sound(&o->pose,346);sound(&o->pose,347);o->timer|=2;}}
    else o->timer&=(int16_t)~2;
    if(random<256)sound(&o->pose,347);
}
static int item_box(const Tr2Level *l,const Tr2Actor *a){
    Tr2Sector s;if(!tr2_find_sector(l,a->x,a->y,a->z,a->room,&s))return -1;
    int id=tr2_u16(l->rooms[s.room].sectors.data+s.index*8+2);return (uint32_t)id<l->boxes.count?id:-1;
}
static int passable(const Tr2Level *l,int from,int to){
    if((uint32_t)to>=l->boxes.count)return 0;
    const uint8_t *a=l->boxes.data+from*8,*b=l->boxes.data+to*8;
    /* Search expands backwards from the destination, as SearchLOT does.
       Winston uses the one-click ground zone and one-click step/drop. */
    int delta=tr2_i16(b+4)-tr2_i16(a+4);const Tr2World *w=l->world;const uint8_t *zones=l->zones.data+(w && w->flip_status?l->boxes.count*10:0);
    return !(tr2_u16(b+6)&0x4000) && delta<=256 && delta>=-256 &&
        tr2_u16(zones+from*2)==tr2_u16(zones+to*2);
}
int tr2_navigation_next(const Tr2Level *l,int start,int goal){
    if((uint32_t)start>=l->boxes.count || (uint32_t)goal>=l->boxes.count)return -1;
    if((tr2_u16(l->boxes.data+start*8+6)|tr2_u16(l->boxes.data+goal*8+6))&0x4000)return -1;
    if(start==goal)return goal;
    int *queue=malloc(l->boxes.count*sizeof *queue),*parent=malloc(l->boxes.count*sizeof *parent);if(!queue || !parent){free(queue);free(parent);return -1;}
    for(uint32_t i=0;i<l->boxes.count;i++)parent[i]=-1;
    unsigned head=0,tail=0;queue[tail++]=goal;parent[goal]=goal;
    while(head<tail && parent[start]<0){int b=queue[head++];unsigned at=tr2_u16(l->boxes.data+b*8+6)&0x3fff;
        for(unsigned n=0;at<l->overlaps.count && n<l->overlaps.count;n++,at++){
            int link=tr2_u16(l->overlaps.data+at*2),id=link&0x7fff;
            if((uint32_t)id<l->boxes.count && parent[id]<0 && passable(l,b,id)){parent[id]=b;queue[tail++]=id;}
            if(link&0x8000)break;
        }
    }
    int result=parent[start];
    free(queue);free(parent);return result;
}
int tr2_creature_turn(Tr2Actor *a,int target_x,int target_z,int maximum){
    if(!a->speed || !maximum)return 0;
    int dx=target_x-a->x,dz=target_z-a->z,turn=tr2_word(tr2_atan(dz,dx)-a->yaw);
    int radius=(a->speed*16384)/maximum;
    if((turn>16384 || turn< -16384) && tr2_long((int64_t)dx*dx+(int64_t)dz*dz)<tr2_long((int64_t)radius*radius))maximum=tr2_asr(maximum,1);
    if(turn>maximum)turn=maximum;else if(turn< -maximum)turn=-maximum;
    a->yaw=tr2_word(a->yaw+turn);return turn;
}
static void winston(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *lara){
    Tr2Actor *a=&o->pose;int dx=lara->x-a->x,dz=lara->z-a->z;
    int64_t square=(int64_t)dx*dx+(int64_t)dz*dz;int distance=tr2_long(square);
    int relative=tr2_word(tr2_atan(dz,dx)-a->yaw);
    int start=item_box(l,a),goal=item_box(l,lara);
    Tr2Creature *c=w->creatures+(o-w->objects);if(!c->lot.nodes)return;
    tr2_winston_target(l,c,a,lara,start,goal,&w->random_state);
    int turn=tr2_creature_turn(a,c->target[0],c->target[2],c->maximum_turn);a->move_angle=a->yaw;
    c->random_draw=c->random_draw*0x41c64e6d+12345;
    int touching=tr2_object_overlap(l,a,lara,100) && tr2_touch(l,a,260,lara,0);
    a->event_count=0;tr2_winston_decide(o,distance,abs(relative)<16384,touching,(c->random_draw>>10)&32767);
    int count=a->event_count,sounds[16];for(int i=0;i<count;i++)sounds[i]=a->event_value[i];
    tr2_winston_move(l,o,&c->lot,start,turn);for(int i=0;i<count;i++)sound(a,sounds[i]);
    if(tr2_object_overlap(l,a,lara,100) && tr2_touch(l,a,260,lara,0)){
        Tr2Contact contact={0};contact.radius=100;contact.old[0]=lara->x;contact.old[1]=lara->y;contact.old[2]=lara->z;
        tr2_object_push(l,a,lara,&contact);
    }
}
int tr2_breakable_should_break(int object_yaw,int lara_yaw,int speed){
    return abs(tr2_asr(speed*tr2_cos(tr2_word(lara_yaw-object_yaw)),14))>=50;
}
void tr2_home_objects_init(Tr2Level *l,Tr2World *w){
    for(uint32_t i=0;i<w->count;i++)if(w->objects[i].id==260){
        Tr2Creature *c=w->creatures+i;Tr2Lot *lot=&c->lot;
        lot->nodes=calloc(l->boxes.count,sizeof *lot->nodes);if(!lot->nodes)continue;
        c->maximum_turn=182;c->random_draw=0xd371f947;lot->head=lot->tail=lot->target_box=lot->required_box=-1;
        lot->step=256;lot->drop=-512;lot->block=0x4000;
        int box=item_box(l,&w->objects[i].pose);
        for(uint32_t j=0;j<l->boxes.count;j++)lot->nodes[j].exit=lot->nodes[j].next=-1;
        if(box>=0)for(uint32_t j=0;j<l->boxes.count;j++){
            const uint8_t *normal=l->zones.data,*flip=normal+l->boxes.count*10;
            if(tr2_i16(normal+j*2)==tr2_i16(normal+box*2) || tr2_i16(flip+j*2)==tr2_i16(flip+box*2))lot->nodes[lot->zone_count++].box=(int16_t)j;
        }
    }
    for(uint32_t i=0;i<w->count;i++){Tr2Object *o=w->objects+i;if(o->id!=72 && o->id!=73)continue;
        o->flags=0;o->door[0].box=item_box(l,&o->pose);
        if(o->door[0].box>=0){uint8_t *p=(uint8_t *)l->boxes.data+o->door[0].box*8+6;
            o->door[0].saved[0]=p[0];o->door[0].saved[1]=p[1];if(p[1]&128)p[1]|=64;}
    }
}
void tr2_home_objects_free(Tr2Level *l,Tr2World *w){
    for(uint32_t i=0;i<w->count;i++){Tr2Object *o=w->objects+i;
        if((o->id==72 || o->id==73) && o->door[0].box>=0){uint8_t *p=(uint8_t *)l->boxes.data+o->door[0].box*8+6;p[0]=o->door[0].saved[0];p[1]=o->door[0].saved[1];}}
}
void tr2_fragment_tick(const Tr2Level *l,Tr2Fragment *f){
    if(!f->active)return;
    /* BodyPartControl 0x4337a0, zero-damage glass fragments. */
    f->pitch=tr2_word(f->pitch+910);f->roll=tr2_word(f->roll+1820);
    f->z+=tr2_asr(f->speed*tr2_cos(f->yaw),14);f->x+=tr2_asr(f->speed*tr2_sin(f->yaw),14);
    f->fall_speed=tr2_word(f->fall_speed+6);f->y+=f->fall_speed;
    Tr2Height h;Tr2Sector sector;
    if(!tr2_query(l,f->x,f->y,f->z,f->room,&h,&sector)){f->active=0;return;}
    if(f->y<h.ceiling){f->y=h.ceiling;f->fall_speed=tr2_word(-f->fall_speed);}
    if(f->y>=h.floor){f->active=0;return;}f->room=(int16_t)sector.room;
}
static int random_control(Tr2World *w){w->random_state=w->random_state*0x41c64e6d+12345;return (w->random_state>>10)&32767;}
uint64_t tr2_object_mesh_mask(const Tr2Object *o){
    /* InitialiseWindow 0x434e4c: intact mesh 0 only. SmashWindow sets
       0xfffe then ExplodingDeath removes 0xfefe, leaving broken mesh 8. */
    if(o->id==41 || o->id==43)return o->mesh_mask;
    return (o->id==72 || o->id==73)?((o->flags&256)?UINT64_C(0x100):UINT64_C(1)):(o->mesh_mask?o->mesh_mask:UINT64_MAX);
}
void tr2_object_fragments(const Tr2Level *l,Tr2World *w,const Tr2Object *o,uint64_t mask){
    const uint8_t *model=tr2_model(l,(uint32_t)o->id);if(!model || !w->fragments)return;
    if(o->mesh_mask)mask&=o->mesh_mask;
    Tr2Light light;tr2_room_light(l,o->pose.room,o->pose.x,o->pose.y,o->pose.z,0,0,&light);
    for(int mesh=0;mesh<tr2_i16(model+4) && mesh<64;mesh++){
        if(!(mask&(UINT64_C(1)<<mesh)))continue;
        Tr2Fragment *f=NULL;for(int i=0;i<TR2_MAX_FRAGMENTS;i++)if(!w->fragments[i].active){f=w->fragments+i;break;}
        if(!f)break;int point[3]={0,0,0};if(!tr2_joint(l,&o->pose,o->id,mesh,point))continue;
        *f=(Tr2Fragment){0};f->active=1;f->x=point[0];f->y=point[1];f->z=point[2];f->room=o->pose.room;
        f->yaw=tr2_word(2*random_control(w)-32768);f->speed=(int16_t)(random_control(w)>>8);
        f->fall_speed=(int16_t)tr2_asr(-random_control(w),8);f->mesh=(int16_t)(tr2_u16(model+6)+mesh);f->object=242;f->shade=(int16_t)(light.shade-768);
    }
}
void tr2_smash_window(const Tr2Level *l,Tr2World *w,Tr2Object *o){
    if(o->flags&256)return;o->flags|=256;o->status=2;o->active=0;sound(&o->pose,o->id==73?348:58);
    tr2_object_fragments(l,w,o,0xfefeu);
    if(o->door[0].box>=0){uint8_t *p=(uint8_t *)l->boxes.data+o->door[0].box*8+7;if(*p&128)*p&=(uint8_t)~64u;}
}
static void breakable(Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *a){
    if(o->flags&256)return;
    if(w->vehicle>=0){
        int dx=o->pose.x-a->x,dy=o->pose.y-a->y,dz=o->pose.z-a->z,b[6];
        if(abs(dx)<=512 && abs(dz)<=512 && dx*dx+dz*dz<=512*512 &&
           abs(dy)<=3072 && tr2_best_bounds(l,a,b) && dy>=b[2] && dy<=b[3]+100)
            tr2_smash_window(l,w,o);
        return;
    }
    if(!tr2_object_overlap(l,&o->pose,a,100) || !tr2_touch(l,&o->pose,72,a,0))return;
    if(tr2_breakable_should_break(o->pose.yaw,a->yaw,a->speed)){
        tr2_smash_window(l,w,o);
        return;
    }
    Tr2Contact contact={0};contact.radius=100;contact.old[0]=a->x;contact.old[1]=a->y;contact.old[2]=a->z;
    tr2_object_push(l,&o->pose,a,&contact);
}
void tr2_home_objects(Tr2Level *l,Tr2World *w,Tr2Actor *a){
    if(w->fragments)for(int i=0;i<TR2_MAX_FRAGMENTS;i++)tr2_fragment_tick(l,w->fragments+i);
    for(uint32_t i=0;i<w->count;i++){
        if(w->objects[i].id==260)winston(l,w,w->objects+i,a);
        else if(w->objects[i].id==73 && w->objects[i].active)tr2_smash_window(l,w,w->objects+i);
        else if(w->objects[i].id==72)breakable(l,w,w->objects+i,a);
    }
}
