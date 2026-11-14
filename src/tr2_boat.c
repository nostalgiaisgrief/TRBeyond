#include "tr2_boat.h"
#include "tr2_world.h"
/* BoatControl packs pitch in SoundEffect flags; 0x43f635 extracts bits 8..31. */
int tr2_boat_sound_pitch(const Tr2Boat *b){return (int)(((uint32_t)b->pitch*25600u+0xdcd804u)>>8);}

static void boat_sound(Tr2Actor *a,int id){if(a->event_count<16){int n=a->event_count++;a->event_kind[n]=5;a->event_value[n]=id;}}
static void rider_animate(const Tr2Level *l,Tr2Actor *a){Tr2Object item={0};item.pose=*a;tr2_object_animate(l,&item);*a=item.pose;}
int tr2_boat_use(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *a,uint32_t input){Tr2Contact c={0};c.radius=a->control.water_status==1?300:100;c.old[0]=a->x;c.old[1]=a->y;c.old[2]=a->z;c.facing=a->yaw;return tr2_boat_contact(l,w,index,a,input,&c);}
int tr2_boat_contact(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *a,uint32_t input,Tr2Contact *contact){
    if(a->control.health<0 || w->vehicle>=0)return 0;
    Tr2Object *o=w->objects+index;Tr2Actor *p=&o->pose;if(o->flags&0x8000)return 0;
    int radius=contact->radius;
    if(!tr2_object_overlap(l,p,a,radius) || !tr2_touch(l,p,14,a,0))return 0;
    int kind=tr2_boat_mount_kind(p,a,input);
    if(!kind){contact->flags|=8;tr2_object_push(l,p,a,contact);return 0;}
    const uint8_t *m=tr2_model(l,11);if(!m)return 0;
    int offset=kind==1?8:kind==2?0:kind==3?6:1;
    tr2_set_animation(l,a,tr2_u16(m+16)+offset,0);a->control.current=a->control.goal=0;
    a->control.water_status=0;a->control.flags&=(uint16_t)~8u;a->x=p->x;a->y=p->y-5;a->z=p->z;a->yaw=p->yaw;
    a->pitch=a->control.roll=a->speed=a->fall_speed=0;a->room=p->room;
    w->vehicle=index;rider_animate(l,a);o->active=o->status=1;return 1;
}
int tr2_player_object_contacts(const Tr2Level *l,Tr2Actor *a,uint32_t input,Tr2Contact *contact){
    Tr2World *w=l->world;if(!w)return 0;w->player_contacts_done=1;
    w->hit_direction=-1;
    a->control.flags&=(uint16_t)~16u;
    int alive=a->control.health>0;
    for(uint32_t i=0;i<w->count;i++){
        /* LaraBaddieCollision (4136db) skips creatures once CreatureDie
           has cleared contact at the end of their death animation. */
        if(tr2_object_intelligent(w->objects[i].id) && !(w->objects[i].pose.control.flags&32))continue;
        if(w->objects[i].id==13)tr2_snow_contact(l,w,(int)i,a,input,contact);
        else if(w->objects[i].id==14)tr2_boat_contact(l,w,(int)i,a,input,contact);
        /* DoorCollision 413920 remains active in the open pose too. */
        else if(w->objects[i].id>=106 && w->objects[i].id<=113 && w->objects[i].status!=3 && alive){
            const Tr2Object *door=w->objects+i;
            if((contact->flags&8) && tr2_object_overlap(l,&door->pose,a,contact->radius) && tr2_touch(l,&door->pose,door->id,a,0))
                tr2_object_push_bounds(l,&door->pose,a,contact,(door->pose.control.current!=door->pose.control.goal) && (contact->flags&16),1);
        }
        else if(w->objects[i].id==51 && w->objects[i].status!=3 && alive){
            Tr2Actor *sled=&w->objects[i].pose;
            if(tr2_object_overlap(l,sled,a,contact->radius) && tr2_touch(l,sled,51,a,0)){
                if(contact->flags&8)tr2_object_push_bounds(l,sled,a,contact,sled->speed>0 && (contact->flags&16),0);
                if(w->vehicle<0 && sled->speed>0){a->control.flags|=16;a->control.health-=100;}
            }
        }
        else if((w->objects[i].id==226 || w->objects[i].id==227))tr2_detonator_contact(l,w,w->objects+i,a,input,contact);
        else if(w->objects[i].id==22 && alive && w->objects[i].active)tr2_dragon_contact(l,w,w->objects+i,a,input,contact);
        else if(w->objects[i].id==63 && alive && w->objects[i].status!=3)tr2_object63_contact(l,w->objects+i,a,contact);
        else if((w->objects[i].id==87 || w->objects[i].id==64 || w->objects[i].id==94 || w->objects[i].id==77 || w->objects[i].id==66) && w->objects[i].status!=3 && alive)tr2_propeller_contact(l,w,w->objects+i,a,contact);
        else if((w->objects[i].id==73 || w->objects[i].id==88) && !(w->objects[i].flags&256) && (contact->flags&8) && alive){
            const Tr2Actor *p=&w->objects[i].pose;
            if(tr2_object_overlap(l,p,a,contact->radius) && tr2_touch(l,p,w->objects[i].id,a,0))tr2_object_push(l,p,a,contact);
        }
        else if((w->objects[i].id==60 || w->objects[i].id==83 || w->objects[i].id==101) && alive)tr2_boulder_contact(l,w,w->objects+i,a,contact);
        /* Tomb2.exe assigns CreatureCollision (413840) to these object types. */
        else if(w->objects[i].status!=3 && alive){
            switch(w->objects[i].id){
            case 15: case 16: case 17: case 18: case 20: case 21:
            case 25: case 26: case 27: case 28: case 29: case 30:
            case 31: case 32: case 33: case 34: case 39:
            case 19: case 36: case 37: case 41: case 43:
            case 38: case 45: case 46: case 48: case 53: case 54:
            case 47: case 49: case 50: case 78:
                tr2_blade_contact(l,w,w->objects+i,a,contact);
                break;
            }
        }
    }
    if(w->hit_direction==-1 && alive)w->hit_frame=0;
    return (uint32_t)w->vehicle<w->count &&
           !(w->objects[w->vehicle].id==13 && (w->objects[w->vehicle].flags&256));
}

typedef struct {Tr2World *world;int index;} BoatContext;
static void boat_room_obstacles(void *context){
    BoatContext *c=context;Tr2World *w=c->world;Tr2Object *boat=w->objects+c->index;
    int room=boat->pose.room;if(room<0 || room>=1024)return;
    for(int i=w->room_head[room],n=0;i>=0 && (uint32_t)i<w->count && (uint32_t)n<w->count;n++){
        Tr2Object *o=w->objects+i;
        if(o->id==14 && i!=c->index && i!=w->vehicle){tr2_boat_obstacle(&boat->pose,&o->pose,14);return;}
        if(o->id==24 && tr2_boat_obstacle(&boat->pose,&o->pose,24))boat_sound(&o->pose,337);
        i=o->next_room;
    }
}
void tr2_gondola_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o){
    Tr2Actor *a=&o->pose;
    if(a->control.current==3){a->y+=50;Tr2Height h;if(tr2_query(l,a->x,a->y,a->z,a->room,&h,NULL) && a->y>=h.floor){a->y=h.floor;a->control.goal=4;}}
    else if(a->control.current==1 && a->control.goal==2){o->mesh_mask=255;tr2_object_fragments(l,w,o,240);o->mesh_mask=15;}
    tr2_object_animate(l,o);if(o->status==2)o->active=0;
}
static int boat_float(const Tr2Level *l,Tr2Actor *a,Tr2Boat *b,const Tr2Actor *rider,uint32_t input,Tr2BoatSurface *surface,Tr2World *w,Tr2Actor *trigger_actor,Tr2Look *look);
void tr2_boat_tick(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *a,uint32_t input){tr2_boat_view_tick(l,w,index,a,input,NULL);}
void tr2_boat_view_tick(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *a,uint32_t input,Tr2Look *look){
    Tr2Object *o=w->objects+index;Tr2Actor *p=&o->pose;Tr2Boat *b=&o->boat;int occupied=w->vehicle==index;
    BoatContext context={w,index};int hit=tr2_boat_dynamics(l,p,b,occupied?a:NULL,boat_room_obstacles,&context);
    Tr2BoatSurface surface;if(!boat_float(l,p,b,occupied?a:NULL,input,&surface,occupied?w:NULL,a,look))return;
    const uint8_t *rider=tr2_model(l,11),*boat=tr2_model(l,14);if(!rider || !boat)return;
    int rb=tr2_u16(rider+16),bb=tr2_u16(boat+16);
    if(occupied)tr2_boat_rider(l,p,b,a,hit,input,rb);
    p->room=(int16_t)surface.room;tr2_world_room(w,index,surface.room);p->control.roll=tr2_word(p->control.roll+b->tilt);
    if(occupied){
        a->room=p->room;a->x=p->x;a->y=p->y;a->z=p->z;a->pitch=p->pitch;a->yaw=p->yaw;a->control.roll=p->control.roll;
        rider_animate(l,a);
        if(a->control.health>0){int animation=a->control.animation+bb-rb;
            if((uint32_t)animation<l->animations.count){p->control.animation=animation;p->control.frame=tr2_word(a->control.frame+tr2_i16(l->animations.data+animation*32+16)-tr2_i16(l->animations.data+a->control.animation*32+16));}
        }
    }
    int pitch=surface.water-surface.ceiling;
    pitch=pitch<5120?tr2_long((int64_t)pitch*p->speed)/5120:p->speed;b->pitch+=tr2_asr(pitch-b->pitch,2);
    if(p->speed && p->y!=surface.water-5)boat_sound(p,336);
    else if(p->speed>20)boat_sound(p,197);
    else if(surface.driving)boat_sound(p,195);
    if(p->speed && p->y==surface.water-5){Tr2Light light;tr2_room_light(l,p->room,p->x,p->y,p->z,0,0,&light);tr2_boat_wake(l,w,p,light.shade);}
    if(occupied && (a->control.current==3 || a->control.current==4) && a->control.frame==tr2_i16(l->animations.data+a->control.animation*32+18)){
        a->yaw=tr2_word(a->yaw+(a->control.current==4?-16384:16384));tr2_set_animation(l,a,77,0);a->control.current=a->control.goal=3;
        a->control.flags|=8;a->fall_speed=-40;a->speed=20;a->pitch=a->control.roll=0;w->vehicle=-1;
        int x=a->x+tr2_asr(360*tr2_sin(a->yaw),14),y=a->y-90,z=a->z+tr2_asr(360*tr2_cos(a->yaw),14);
        Tr2Height h;Tr2Sector sector;if(tr2_query(l,x,y,z,a->room,&h,&sector) && h.floor>=y-256){a->x=x;a->z=z;a->room=(int16_t)sector.room;}
        a->y=y;tr2_set_animation(l,p,bb,0);
    }
}
/* Buoyancy and input phase of BoatControl 0x40daa0, through 0x40dd33.
   Called after dynamics has removed the steering tilt from the hull roll. */
int tr2_boat_float(const Tr2Level *l,Tr2Actor *a,Tr2Boat *b,const Tr2Actor *rider,uint32_t input,Tr2BoatSurface *surface){return boat_float(l,a,b,rider,input,surface,NULL,NULL,NULL);}
static int boat_float(const Tr2Level *l,Tr2Actor *a,Tr2Boat *b,const Tr2Actor *rider,uint32_t input,Tr2BoatSurface *surface,Tr2World *w,Tr2Actor *trigger_actor,Tr2Look *look){
    int left[3],right[3],lh=tr2_boat_sample(l,a,750,-300,left),rh=tr2_boat_sample(l,a,750,300,right);
    Tr2Sector sector;Tr2Height h;
    if(!tr2_find_sector(l,a->x,a->y,a->z,a->room,&sector) || !tr2_query(l,a->x,a->y,a->z,sector.room,&h,NULL))return 0;
    surface->room=sector.room;surface->floor=h.floor;surface->ceiling=h.ceiling;surface->trigger=h.trigger;
    if(w){tr2_world_trigger(l,w,trigger_actor,h.trigger,0);tr2_world_trigger(l,w,trigger_actor,h.trigger,1);}
    surface->water=b->water=tr2_water_height(l,a->x,a->z,sector.room,NULL);surface->driving=surface->look=0;
    int damp=1;
    if(rider && rider->control.health>0){int state=rider->control.current;
        if(state!=0 && (state<=2 || state>4)){surface->driving=1;damp=tr2_boat_input(a,b,input,&surface->look);}
    }else if(a->speed>1)a->speed--;else a->speed=0;
    if(surface->look && look && rider){
        /* LookUpDown 0x427700, called only by stopped BoatUserControl. */
        look->active=1;
        if(input&TR2_FORWARD){if(look->pitch>-7644)look->pitch-=364;}
        else if(input&TR2_BACK){if(look->pitch<4004)look->pitch+=364;}
        if(rider->gun_status!=1)look->torso_pitch=look->pitch;
    }
    if(damp){if(b->turn< -45)b->turn+=45;else if(b->turn>45)b->turn-=45;else b->turn=0;}
    b->water=b->water==TR2_NO_HEIGHT?h.floor:b->water-5;
    b->left_fall=tr2_boat_vertical(lh,b->left_fall,&left[1]);b->right_fall=tr2_boat_vertical(rh,b->right_fall,&right[1]);
    a->fall_speed=tr2_word(tr2_boat_vertical(b->water,a->fall_speed,&a->y));
    int average=tr2_asr(left[1]+right[1],1),pitch=tr2_word(tr2_atan(750,a->y-average)),roll=tr2_word(tr2_atan(300,average-left[1]));
    a->pitch=tr2_word(a->pitch+tr2_asr(pitch-a->pitch,1));a->control.roll=tr2_word(a->control.roll+tr2_asr(roll-a->control.roll,1));
    if(!pitch && a->pitch> -4 && a->pitch<4)a->pitch=0;
    if(!roll && a->control.roll> -4 && a->control.roll<4)a->control.roll=0;
    return 1;
}
/* Pair response from 0x40cf20. Return 1 when a gondola receives its smash
   request; the caller owns room-list iteration and sound dispatch. */
int tr2_boat_obstacle(Tr2Actor *a,Tr2Actor *other,int id){
    int x=other->x,z=other->z;
    if(id==24){if(other->control.current!=1)return 0;
        x-=tr2_asr(512*tr2_sin(other->yaw),14);z-=tr2_asr(512*tr2_cos(other->yaw),14);
    }else if(id!=14)return 0;
    int dx=x-a->x,dz=z-a->z;
    int distance=tr2_long((int64_t)dx*dx+(int64_t)dz*dz);
    if(distance>=1000000)return 0;
    if(id==24 && a->speed>=80){if(other->y-a->y<2048){other->control.goal=2;return 1;}return 0;}
    /* The original divides by zero for coincident anchors. Leave the pose
       unchanged for that invalid input instead of faulting the process. */
    if(!distance)return 0;
    a->x=x-tr2_long((int64_t)dx*1000000)/distance;
    a->z=z-tr2_long((int64_t)dz*1000000)/distance;return 0;
}
/* BoatDynamics 0x40d2c0. Room-item collisions run between movement and the
   second hull sample, through the supplied world callback. */
int tr2_boat_dynamics(const Tr2Level *l,Tr2Actor *a,Tr2Boat *b,Tr2Actor *rider,void (*obstacles)(void *),void *context){
    static const int forward[5]={750,750,-750,-750,1000},side[5]={-300,300,-300,300,0};
    int old[5][3],origin[3]={a->x,a->y,a->z};
    a->control.roll=tr2_word(a->control.roll-b->tilt);
    for(int i=0;i<5;i++){int height=tr2_boat_sample(l,a,forward[i],side[i],old[i]);if(old[i][1]>height)old[i][1]=height;}
    a->yaw=tr2_word(a->yaw+b->turn+b->extra_rotation);b->tilt=tr2_word(b->turn*6);
    int s=tr2_sin(a->yaw),c=tr2_cos(a->yaw);
    a->x+=tr2_asr(a->speed*s,14);a->z+=tr2_asr(a->speed*c,14);
    int slip=tr2_asr(30*tr2_sin(a->control.roll),14);
    if(!slip && a->control.roll)slip=a->control.roll>0?1:-1;
    a->x+=tr2_asr(slip*c,14);a->z-=tr2_asr(slip*s,14);
    slip=tr2_asr(10*tr2_sin(a->pitch),14);
    if(!slip && a->pitch)slip=a->pitch>0?1:-1;
    a->x-=tr2_asr(slip*s,14);a->z-=tr2_asr(slip*c,14);
    int moved[3]={a->x,a->y,a->z};if(obstacles)obstacles(context);
    int rotation=0;static const int order[4]={2,3,0,1};
    for(int j=0;j<4;j++){int i=order[j],p[3];int height=tr2_boat_sample(l,a,forward[i],side[i],p);
        if(height<old[i][1]-128)rotation+=tr2_vehicle_shift(l,a,p,old[i]);
    }
    if(!slip){int p[3];if(tr2_boat_sample(l,a,1000,0,p)<old[4][1]-128)tr2_vehicle_shift(l,a,p,old[4]);}
    /* The centre probe has no five-unit hull offset. */
    Tr2Sector sector;int height=TR2_NO_HEIGHT;
    if(tr2_find_sector(l,a->x,a->y,a->z,a->room,&sector)){
        height=tr2_water_height(l,a->x,a->z,sector.room,NULL);
        if(height==TR2_NO_HEIGHT){Tr2Height h;if(tr2_query(l,a->x,a->y,a->z,sector.room,&h,NULL))height=h.floor;}
    }
    if(height<a->y-128){int p[3]={a->x,a->y,a->z};tr2_vehicle_shift(l,a,p,origin);}
    b->extra_rotation=tr2_word(rotation);int hit=tr2_vehicle_hit(a,moved);
    if(slip || hit){
        int speed=tr2_asr(tr2_long((int64_t)(a->x-origin[0])*s+(int64_t)(a->z-origin[2])*c),14);
        if(rider && a->speed>95 && speed<a->speed-10){
            rider->control.health=tr2_word(rider->control.health-tr2_asr(a->speed-speed,1));rider->control.flags|=16;
            if(rider->event_count<16){int n=rider->event_count++;rider->event_kind[n]=5;rider->event_value[n]=31;}
        }
        if(slip){if(a->speed<=100)a->speed=tr2_word(speed);}
        else if((a->speed>0 && speed<a->speed) || (a->speed<0 && speed>a->speed))a->speed=tr2_word(speed);
        if(a->speed< -20)a->speed=-20;
    }
    return hit;
}
/* Shared vehicle wall response, 0x43d3d0. Diagonal crossings test the two
   adjacent sectors separately, retaining the original 256-unit threshold. */
int tr2_vehicle_shift(const Tr2Level *l,Tr2Actor *a,const int p[3],const int old[3]){
    int px=tr2_asr(p[0],10),pz=tr2_asr(p[2],10),ox=tr2_asr(old[0],10),oz=tr2_asr(old[2],10);
    int x=p[0]&1023,z=p[2]&1023;
    if(px==ox){
        if(pz==oz){a->x+=old[0]-p[0];a->z+=old[2]-p[2];return 0;}
        if(pz>oz){a->z-=z+1;return p[0]-a->x;}
        a->z+=1024-z;return a->x-p[0];
    }
    if(pz==oz){
        if(px>ox){a->x-=x+1;return a->z-p[2];}
        a->x+=1024-x;return p[2]-a->z;
    }
    int dx=0,dz=0;Tr2Height h;
    if(tr2_query(l,old[0],p[1],p[2],a->room,&h,NULL) && h.floor<old[1]-256)dz=(p[2]>old[2]?-1:1024)-z;
    if(tr2_query(l,p[0],p[1],old[2],a->room,&h,NULL) && h.floor<old[1]-256)dx=(p[0]>old[0]?-1:1024)-x;
    if(dx && dz){a->x+=dx;a->z+=dz;return 0;}
    if(dz){a->z+=dz;return dz>0?a->x-p[0]:p[0]-a->x;}
    if(dx){a->x+=dx;return dx>0?p[2]-a->z:a->z-p[2];}
    a->x+=old[0]-p[0];a->z+=old[2]-p[2];return 0;
}
/* GetCollisionAnim 0x43d6b0 mutates its old-position argument to the
   displacement caused by collision resolution. */
int tr2_vehicle_hit(const Tr2Actor *a,int p[3]){
    p[0]=a->x-p[0];p[2]=a->z-p[2];if(!p[0] && !p[2])return 0;
    int c=tr2_cos(a->yaw),s=tr2_sin(a->yaw);
    int forward=tr2_asr(tr2_long((int64_t)c*p[2]+(int64_t)s*p[0]),14);
    int side=tr2_asr(tr2_long((int64_t)c*p[0]-(int64_t)s*p[2]),14);
    if((forward<0?-forward:forward)>(side<0?-side:side))return forward>0?14:13;
    return side<=0?12:11;
}
/* BoatAnimation 0x40d930. The idle state's speed check deliberately overrides
   a dismount request while moving; the moving state tests Jump first. */
void tr2_boat_rider(const Tr2Level *l,const Tr2Actor *b,const Tr2Boat *s,Tr2Actor *a,int collision,uint32_t input,int base){
    int state=a->control.current,next=-1,animation=-1;
    if(a->control.health<=0){if(state!=8){next=8;animation=base+18;}}
    else if(b->y<s->water-128 && b->fall_speed>0){if(state!=6){next=6;animation=base+15;}}
    else if(collision){if(state!=5){next=5;animation=base+collision;}}
    else if(state==6)a->control.goal=2;
    else if(state==2){
        if(input&TR2_JUMP){if(input&TR2_RIGHT)a->control.goal=3;else if(input&TR2_LEFT)a->control.goal=4;}
        else if(b->speed<=0)a->control.goal=1;
    }else if(state==1){
        if(input&TR2_JUMP){if(input&TR2_RIGHT)a->control.goal=3;else if(input&TR2_LEFT)a->control.goal=4;}
        if(b->speed>0)a->control.goal=2;
    }
    if(animation>=0 && (uint32_t)animation<l->animations.count){
        const uint8_t *p=l->animations.data+animation*32;
        a->control.animation=animation;a->control.frame=tr2_i16(p+16);
        a->control.current=a->control.goal=next;
    }
}
/* DoBoatDynamics 0x40d270. */
int tr2_boat_vertical(int height,int fall,int *y){
    if(height>*y){*y+=fall;if(*y>height){*y=height;return 0;}return fall+6;}
    int kick=tr2_asr(height-fall-*y,3);fall+=kick;if(fall< -20)fall=-20;
    if(*y>height)*y=height;return fall;
}
/* BoatUserControl 0x40d7a0: return whether turn damping should run. */
int tr2_boat_input(Tr2Actor *a,Tr2Boat *b,uint32_t input,int *look){
    int damp=1;*look=0;
    if(a->y<b->water-128 || b->water==TR2_NO_HEIGHT)return damp;
    if(input&(TR2_JUMP|TR2_LOOK)){if((input&TR2_LOOK) && !a->speed)*look=1;return damp;}
    int back=(input&TR2_BACK)!=0;
    if(((input&TR2_LEFT) && !back) || ((input&TR2_RIGHT) && back)){
        if(b->turn>0)b->turn-=45;else{b->turn-=22;if(b->turn< -728)b->turn=-728;}damp=0;
    }else if(((input&TR2_RIGHT) && !back) || ((input&TR2_LEFT) && back)){
        if(b->turn<0)b->turn+=45;else{b->turn+=22;if(b->turn>728)b->turn=728;}damp=0;
    }
    if(back){if(a->speed> -20)a->speed=tr2_word(a->speed-5);}
    else if(input&TR2_FORWARD){int maximum=input&TR2_ACTION?140:input&TR2_WALK?30:90;
        if(a->speed<maximum)a->speed=tr2_word(a->speed+2+a->speed*5/(maximum*2));
        else if(a->speed>maximum+1)a->speed--;
    }else if(a->speed>=0 && a->speed<20 && (input&(TR2_LEFT|TR2_RIGHT)))a->speed=20;
    else if(a->speed>1)a->speed--;else a->speed=0;
    return damp;
}

/* GetOnBoat 0x40cb50 eligibility; caller additionally tests mesh collision. */
int tr2_boat_mount_kind(const Tr2Actor *b,const Tr2Actor *a,uint32_t input){
    if(a->gun_status)return 0;
    int z=tr2_asr(tr2_long((int64_t)(a->z-b->z)*tr2_cos(-b->yaw)-(int64_t)(a->x-b->x)*tr2_sin(-b->yaw)),14);
    if(z>200)return 0;int angle=tr2_word(b->yaw-a->yaw);
    if(a->control.water_status==2 || a->control.water_status==4){
        if(!(input&TR2_ACTION) || (a->control.flags&8) || b->speed)return 0;
        if(angle>8192 && angle<24576)return 1;
        if(angle> -24576 && angle< -8192)return 2;
    }else if(!a->control.water_status && angle> -24576 && angle<24576){
        if(a->fall_speed>0 && a->y>b->y)return 3;
        if(!a->fall_speed)return a->x==b->x && a->y==b->y && a->z==b->z?4:3;
    }return 0;
}
/* TestWaterHeight 0x40ce20 uses separate pitch/roll projections, not a
   general rotation matrix. Hull corner offsets are measured in world units. */
int tr2_boat_sample(const Tr2Level *l,const Tr2Actor *a,int forward,int side,int p[3]){
    p[1]=a->y-tr2_asr(forward*tr2_sin(a->pitch),14)+tr2_asr(side*tr2_sin(a->control.roll),14);
    int c=tr2_cos(a->yaw),s=tr2_sin(a->yaw);
    p[0]=a->x+tr2_asr((int64_t)forward*s+(int64_t)side*c,14);
    p[2]=a->z+tr2_asr((int64_t)forward*c-(int64_t)side*s,14);
    Tr2Sector sector;if(!tr2_find_sector(l,p[0],p[1],p[2],a->room,&sector))return TR2_NO_HEIGHT;
    int height=tr2_water_height(l,p[0],p[2],sector.room,NULL);
    if(height==TR2_NO_HEIGHT){Tr2Height h;if(!tr2_query(l,p[0],p[1],p[2],sector.room,&h,NULL))return TR2_NO_HEIGHT;height=h.floor;}
    return height==TR2_NO_HEIGHT?height:height-5;
}





