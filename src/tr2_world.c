#include "tr2_world.h"
#include "tr2_enemy.h"
#include <stdlib.h>
#include <string.h>

static int floor_word(const Tr2Level *l,unsigned p){return p<l->floors.count?tr2_u16(l->floors.data+p*2):-1;}
static int door_room(const Tr2Level *l,const uint8_t *s){unsigned p=tr2_u16(s);if(!p)return 255;int w=floor_word(l,p++);if(w==2){p++;w=floor_word(l,p++);}if(w==3){p++;w=floor_word(l,p++);}return (w&255)==1?floor_word(l,p):255;}
static void door_save(Tr2Level *l,Tr2DoorSector *d,int room,int x,int z){
    d->box=-1;if(room<0 || (uint32_t)room>=l->room_count)return;Tr2Room *r=l->rooms+room;int ix=(x-r->x)>>10,iz=(z-r->z)>>10;
    if(ix<0 || iz<0 || ix>=r->nx || iz>=r->nz)return;
    d->sector=(uint8_t *)r->sectors.data+(ix*r->nz+iz)*8;memcpy(d->saved,d->sector,8);
    int box=tr2_u16(d->saved+2),portal=door_room(l,d->saved);
    /* InitialiseDoor 43523b: the navigation box is across a portal,
       although the sector being sealed remains in this room. */
    if((uint32_t)portal<l->room_count && portal!=255){
        const Tr2Room *other=l->rooms+portal;int ox=(x-other->x)>>10,oz=(z-other->z)>>10;
        if(ox>=0 && oz>=0 && ox<other->nx && oz<other->nz)box=tr2_u16(other->sectors.data+(ox*other->nz+oz)*8+2);
    }
    if((uint32_t)box<l->boxes.count && (l->boxes.data[box*8+7]&128))d->box=box;
}
static void door_set(const Tr2Level *l,Tr2Object *o,int open){
    static const uint8_t closed[8]={0,0,255,255,255,129,255,129};
    for(int i=0;i<4;i++){Tr2DoorSector *d=o->door+i;if(!d->sector)continue;memcpy(d->sector,open?d->saved:closed,8);
        if(d->box>=0){uint8_t *b=(uint8_t *)l->boxes.data+d->box*8+7;if(open)*b&=(uint8_t)~64u;else *b|=64;}}
}
int tr2_world_init(Tr2Level *l,Tr2World *w){
    if(l->room_count>1024)return 0;
    l->object_floors=1;
    memset(w,0,sizeof *w);w->count=l->items.count;
    for(uint32_t i=0;i<l->items.count;i++)if(tr2_i16(l->items.data+i*24)==52)w->count++;
    w->objects=calloc(w->count,sizeof *w->objects);w->cd_pending=w->cd_play=w->cd_last=-1;w->flip_effect=w->flip_sound=-1;w->effect=w->assault_best=-1;if(!w->objects)return 0;
    w->camera_index=w->camera_last=w->camera_target=w->camera_previous_target=-1;w->camera_speed=10;
    w->level=l;w->vehicle=-1;w->hit_direction=-1;
    for(int i=0;i<1024;i++)w->room_head[i]=-1;
    for(int i=0;i<5;i++)w->creature_slots[i]=-1;
    w->fragments=calloc(TR2_MAX_FRAGMENTS,sizeof *w->fragments);
    if(!w->fragments){free(w->objects);memset(w,0,sizeof *w);return 0;}
    w->creatures=calloc(w->count,sizeof *w->creatures);
    if(!w->creatures){free(w->fragments);free(w->objects);memset(w,0,sizeof *w);return 0;}
    w->inventory=calloc(1,sizeof *w->inventory);
    if(!w->inventory){free(w->creatures);free(w->fragments);free(w->objects);memset(w,0,sizeof *w);return 0;}
    w->inventory->selected=-1;
    w->hazards=calloc(1,sizeof *w->hazards);if(!w->hazards){tr2_world_free(w);return 0;}
    w->combat=calloc(1,sizeof *w->combat);if(!w->combat){tr2_world_free(w);return 0;}w->combat->weapon=w->combat->requested=1;w->combat->target=-1;
    w->start_inventory=calloc(1,sizeof *w->start_inventory);w->start_combat=calloc(1,sizeof *w->start_combat);if(!w->start_inventory || !w->start_combat){tr2_world_free(w);return 0;}
    uint32_t next_vehicle=l->items.count;
    for(uint32_t i=0;i<l->items.count;i++){
        Tr2Object *o=w->objects+i;Tr2Actor *a=&o->pose;const uint8_t *p=l->items.data+i*24;o->mesh_mask=UINT64_MAX;o->id=tr2_u16(p);o->flags=tr2_u16(p+22);a->room=tr2_i16(p+2);a->x=tr2_i32(p+4);a->y=tr2_i32(p+8);a->z=tr2_i32(p+12);a->yaw=a->move_angle=tr2_i16(p+16);a->water_distance=TR2_NO_HEIGHT;
        /* InitialiseItem (426f23) enables contact before the object initializer. */
        a->control.flags|=32;
        o->listed_room=a->room;o->next_room=w->room_head[a->room];w->room_head[a->room]=(int)i;
        /* InitialiseItem 0x426fd8 caches the placement sector's raw floor. */
        if(o->id==13)o->snow.momentum=a->yaw;
        if(o->id==13 || o->id==60 || o->id==83 || o->id==101){const Tr2Room *room=l->rooms+a->room;int ix=tr2_asr(a->x-room->x,10),iz=tr2_asr(a->z-room->z,10);
            if(ix>=0 && ix<room->nx && iz>=0 && iz<room->nz){unsigned height=room->sectors.data[(ix*room->nz+iz)*8+5];o->floor=(height<128?(int)height:(int)height-256)*256;}}
        if(o->id==65)o->lift_origin=a->y; /* InitialiseLift 0x4357b0. */
        const uint8_t *m=tr2_model(l,(uint32_t)o->id);if(m){tr2_set_animation(l,a,tr2_u16(m+16),0);a->control.current=a->control.goal=tr2_i16(l->animations.data+a->control.animation*32+6);}
        if(o->id==50){const uint8_t *base=tr2_model(l,49);if(base){tr2_set_animation(l,a,tr2_u16(base+16),0);a->control.current=a->control.goal=tr2_i16(l->animations.data+a->control.animation*32+6);}}
        if(o->id>=16 && o->id<=18){w->random_state=w->random_state*0x41c64e6du+12345;uint32_t mask=UINT32_MAX;if(((w->random_state>>10)&32767)<16384)mask-=48;if(o->id==18)mask-=0x1f8000;o->mesh_mask=mask;}
        if(m && (o->id==17 || o->id==18)){const uint8_t *base=tr2_model(l,16);if(base){tr2_set_animation(l,a,tr2_u16(base+16),0);a->control.current=a->control.goal=tr2_i16(l->animations.data+a->control.animation*32+6);}}
        if(m && (o->id==80 || o->id==81 || o->id==82)){tr2_set_animation(l,a,tr2_u16(m+16)+(o->id==81?2:3),0);a->control.current=1;}
        /* InitialiseItem 0x426f44: the file's invisible bit becomes item
           status, leaving flags available for subsequent trigger activation. */
        if(o->flags&256){o->status=3;o->flags&=(uint16_t)~256u;}
        if((o->id>=15 && o->id<=18) || o->id==20 || o->id==21 || o->id==25 || o->id==28 || o->id==29 || o->id==30 || o->id==31 || o->id==34 || (o->id==32 || o->id==33) || o->id==36 || o->id==38 || tr2_xian_enemy(o->id) || tr2_tibetan_enemy(o->id) || o->id==47 || o->id==49 || o->id==50 || o->id==51 || o->id==39 || o->id==214){
            o->status=3;a->control.health=(int16_t)(o->id==19?60:o->id==37?40:o->id==41?100:o->id==43?80:o->id==46?200:o->id==48?45:tr2_tibetan_enemy(o->id)?30:o->id==20?150:o->id==25?30:o->id==28?12:o->id==30?25:(o->id==29 || o->id==31 || o->id==34)?20:(o->id==32 || o->id==33)?27:o->id>=16 && o->id<=18?25:o->id==21?4:o->id==15?10:o->id==36?5:o->id==47?20:(o->id==49 || o->id==50)?50:o->id==38?15:o->id==39?20:100);
        }
        if(o->id==40){a->x-=512;a->z-=512;}
        if(m && (o->id==41 || o->id==43)){
            o->status=0;o->mesh_mask=0;
            if(o->id==41){tr2_set_animation(l,a,tr2_u16(m+16)+48,0);a->control.current=a->control.goal=18;}
        }
        if(o->id==52){
            a->control.health=1;
            const uint8_t *model=tr2_model(l,51);
            if(!model){tr2_world_free(w);return 0;}
            Tr2Object *sled=w->objects+next_vehicle;
            o->linked[0]=(int)next_vehicle++;
            sled->id=51;sled->pose=*a;sled->pose.control.health=100;
            sled->listed_room=-1;sled->mesh_mask=UINT64_MAX;sled->status=3;
            tr2_set_animation(l,&sled->pose,tr2_u16(model+16),0);
            sled->pose.control.current=sled->pose.control.goal=tr2_i16(l->animations.data+sled->pose.control.animation*32+6);
            tr2_world_room(w,o->linked[0],a->room);
        }
        if(o->id==26 || o->id==27)a->control.health=o->id==26?5:20;
        if(m && o->id==20)tr2_cultist3_init(l,w,o);
        if(m && o->id==47){w->random_state=w->random_state*0x41c64e6du+12345;
            a->yaw=tr2_word(a->yaw+tr2_asr((int)((w->random_state>>10)&32767)-16384,1));a->control.flags|=32;tr2_set_animation(l,a,tr2_u16(m+16)+5,0);a->control.current=a->control.goal=2;}
        if(m && o->id==38){tr2_set_animation(l,a,tr2_u16(m+16)+14,0);a->control.current=a->control.goal=7;}
        if((o->flags&0x3e00)==0x3e00){o->flags=(o->flags&~0x3e00)|0x4000;o->active=1;o->status=1;}
        if(o->id>=106 && o->id<=113){
            int dx=a->yaw==16384?-1024:a->yaw==-16384?1024:0,dz=a->yaw==0?-1024:a->yaw==-32768?1024:0;
            door_save(l,&o->door[0],a->room,a->x+dx,a->z+dz);door_save(l,&o->door[1],l->rooms[a->room].alternate,a->x+dx,a->z+dz);
            int next=o->door[0].sector?door_room(l,o->door[0].sector):255;
            if(next>=0 && next!=255 && (uint32_t)next<l->room_count){door_save(l,&o->door[2],next,a->x,a->z);door_save(l,&o->door[3],l->rooms[next].alternate,a->x,a->z);}
        }
    }
    for(uint32_t i=0;i<w->count;i++)if(w->objects[i].id>=106 && w->objects[i].id<=113)door_set(l,w->objects+i,0);
    tr2_home_objects_init(l,w);
    for(uint32_t i=0;i<w->count;i++)if(w->creatures[i].lot.nodes){for(int j=0;j<5;j++)if(w->creature_slots[j]<0){w->creature_slots[j]=(int)i;break;}}
    if((uint32_t)l->lara<w->count){Tr2Actor *a=&w->objects[l->lara].pose;w->ai_camera[0]=a->x;w->ai_camera[1]=a->y;w->ai_camera[2]=a->z;}
    for(uint32_t i=0;i<w->count;i++)if(w->objects[i].id>=67 && w->objects[i].id<=70 && w->objects[i].status!=3)tr2_block_floor(l,w->objects+i,-1024);
    for(uint32_t i=0;i<w->count;i++)if(w->objects[i].id==260 && !w->creatures[i].lot.nodes){tr2_world_free(w);return 0;}
    /* InitialiseFinalLevel 0x4347a0 records the two intelligent room items. */
    for(uint32_t i=0;i<w->count;i++)if(w->objects[i].id==256){Tr2Object *o=w->objects+i;int found=0;o->linked[0]=o->linked[1]=-1;
        for(int j=w->room_head[o->pose.room],guard=0;j>=0 && (uint32_t)j<w->count && guard++<(int)w->count;j=w->objects[j].next_room)
            if(tr2_object_intelligent(w->objects[j].id) && found<2)o->linked[found++]=j;
    }
    tr2_carried_init(w);l->world=w;return 1;
}
/* FinalLevelCounter 0x434820, used by the Diving Area exit. */
void tr2_finish_guarded(Tr2World *w,const Tr2Object *o,const Tr2Actor *a){
    for(int i=0;i<2;i++)if((uint32_t)o->linked[i]>=w->count || w->objects[o->linked[i]].pose.control.health>0)return;
    if(abs(a->x-o->pose.x)<1024 && abs(a->z-o->pose.z)<1024 && !(a->control.flags&8) && a->y==o->pose.y)w->finished=1;
}
void tr2_world_room(Tr2World *w,int index,int room){
    if((uint32_t)index>=w->count || room< -1 || room>=1024)return;Tr2Object *o=w->objects+index;if(room==o->listed_room)return;
    if(o->listed_room>=0 && o->listed_room<1024){int *link=w->room_head+o->listed_room;for(uint32_t n=0;n<w->count && *link>=0 && (uint32_t)*link<w->count;n++){if(*link==index){*link=o->next_room;break;}link=&w->objects[*link].next_room;}}
    o->next_room=room>=0?w->room_head[room]:-1;o->listed_room=room;if(room>=0)w->room_head[room]=index;
}
/* KillItem 0x426d30 also clears Lara's target when removing an item. */
void tr2_world_kill(Tr2World *w,int index){
    if((uint32_t)index>=w->count)return;
    Tr2Object *o=w->objects+index;o->active=0;o->status=3;o->flags|=0x8000;
    tr2_world_room(w,index,-1);
    if(w->combat && w->combat->target==index)w->combat->target=-1;
}
/* Mini helicopter controller, root 0x4346c0. */
void tr2_helicopter_tick(const Tr2Level *l,Tr2World *w,int index,const Tr2Actor *lara){
    Tr2Object *o=w->objects+index;Tr2Actor *a=&o->pose;
    a->z=tr2_long((int64_t)a->z+100);
    if(abs(a->z-lara->z)>30720)tr2_world_kill(w,index);
    tr2_object_animate(l,o);
    if(a->event_count<16){int n=a->event_count++;a->event_kind[n]=5;a->event_value[n]=297;}
    Tr2Sector ref;if(tr2_find_sector(l,a->x,a->y,a->z,a->room,&ref) && ref.room!=a->room){a->room=(int16_t)ref.room;if(!(o->flags&0x8000))tr2_world_room(w,index,ref.room);}
}
void tr2_world_free(Tr2World *w){
    if(w->level && w->objects)for(uint32_t i=0;i<w->count;i++)if(w->objects[i].id>=67 && w->objects[i].id<=70 && w->objects[i].status==0)tr2_block_floor(w->level,w->objects+i,1024);
    if(w->level && w->level->world==w)w->level->world=NULL;
    if(w->objects && w->level)tr2_home_objects_free(w->level,w);
    if(w->objects)for(uint32_t i=0;i<w->count;i++)for(int j=0;j<4;j++){Tr2DoorSector *d=&w->objects[i].door[j];if(d->sector)memcpy(d->sector,d->saved,8);}
    if(w->creatures)for(uint32_t i=0;i<w->count;i++)free(w->creatures[i].lot.nodes);
    if(w->level && w->flip_status)tr2_flip_rooms(w->level);
    free(w->start_inventory);free(w->start_combat);free(w->combat);free(w->hazards);free(w->inventory);free(w->creatures);free(w->fragments);free(w->objects);memset(w,0,sizeof *w);
}
/* Object228 control435c30. Its sound uses the local interpolated bounds center,
   blended toward Lara without adding the item position (as in this EXE). */
void tr2_object228_sound(const Tr2Level *l,const Tr2Object *o,const Tr2Actor *lara,Tr2Position *p){
    int b[6]={0};tr2_actor_bounds(l,&o->pose,b);
    p->x=lara->x+tr2_asr(tr2_asr(b[0]+b[1],1)-lara->x,2);
    p->y=lara->y+tr2_asr(tr2_asr(b[2]+b[3],1)-lara->y,2);
    p->z=lara->z+tr2_asr(tr2_asr(b[4]+b[5],1)-lara->z,2);p->room=o->pose.room;p->pad=0;
}
void tr2_object228_tick(const Tr2Level *l,Tr2World *w,int index){
    Tr2Object *o=w->objects+index;Tr2Actor *a=&o->pose;
    if(a->control.current==1 && (o->flags&256))a->control.goal=2;
    tr2_object_animate(l,o);
    Tr2Sector sector;if(tr2_find_sector(l,a->x,a->y,a->z,a->room,&sector)){a->room=(int16_t)sector.room;tr2_world_room(w,index,sector.room);}
    if(a->event_count<16){int n=a->event_count++;a->event_kind[n]=5;a->event_value[n]=297;}
    if(o->status==2)tr2_world_kill(w,index);
}
int tr2_trigger_active(uint16_t flags,int16_t *timer){
    int active=!(flags&0x4000);if((flags&0x3e00)!=0x3e00)return !active;
    if(*timer==-1)return !active;if(*timer && --*timer==0)*timer=-1;return active;
}
void tr2_cd_trigger(Tr2World *w,int track,int flags,int type){
    if(track<=1 || track>=64)return;
    if(type!=2){if(w->cd_flags[track]&(flags&0x3e00))return;if(flags&0x100)w->cd_flags[track]|=(uint16_t)(flags&0x3e00);}
    if(track!=w->cd_pending){if(flags&255){w->cd_pending=track;w->cd_flags[track]=(uint16_t)((w->cd_flags[track]&0xff00)|(((flags&255)*30)&255));}else w->cd_play=track;}
    else if(w->cd_flags[track]&255){int timer=(w->cd_flags[track]&255)-1;if(!timer){w->cd_pending=-1;w->cd_play=track;}w->cd_flags[track]=(uint16_t)((w->cd_flags[track]&0xff00)|timer);}
}
int tr2_cd_track(int cue){
    /* Supplied executable 0x43f430, omitted cue table at 0x466278. */
    static const int omitted[]={2,19,20,26};int track=cue<2?2:cue;
    for(int i=0;i<4;i++)if(cue>omitted[i])track--;return track;
}
void tr2_object_animate(const Tr2Level *l,Tr2Object *o){
    Tr2Actor *a=&o->pose;int old=a->control.current;const uint8_t *p=l->animations.data+a->control.animation*32;
    int ending=a->control.frame+1>tr2_i16(p+18);
    /* The shared step implements frame ranges, translations and sound commands. */
    /* AnimateItem 414717/4147e4 consumes required_state only at a range
       transition or end-of-animation link, including same-state loops. */
    int16_t *required=NULL;Tr2World *w=l->world;
    if(w && w->creatures && (uintptr_t)o>=(uintptr_t)w->objects &&
       (uintptr_t)o<(uintptr_t)(w->objects+w->count))required=&w->creatures[o-w->objects].required;
    tr2_animate_required(l,a,required);
    if(ending){if(a->control.current!=old)a->control.goal=a->control.current;
        unsigned at=tr2_u16(p+30);static const int lengths[]={0,3,2,0,0,2,2};
        for(int n=0;n<tr2_i16(p+28) && at<l->commands.count;n++){int op=tr2_i16(l->commands.data+at++*2);if(op<1 || op>6)break;if(op==4)o->status=2;at+=(unsigned)lengths[op];}}
}
/* Underwater SwitchCollision2 0x4385b0; alignment speed 16, turn step 364. */
static int swim_switch_use(const Tr2Level *l,Tr2Object *o,Tr2Actor *a,uint32_t input){
    if(!(input&TR2_ACTION) || o->status || a->control.water_status!=1 || a->gun_status || a->control.current!=13)return 0;
    if(o->pose.control.current!=0 && o->pose.control.current!=1)return 0;
    int delta[3]={a->x-o->pose.x,a->y-o->pose.y,a->z-o->pose.z};
    for(int axis=0;axis<3;axis++){int basis[3]={0,0,0},v[3];basis[axis]=16384;tr2_rotate_offset(o->pose.yaw,o->pose.pitch,basis,v);
        int local=tr2_asr((int64_t)delta[0]*v[0]+(int64_t)delta[1]*v[1]+(int64_t)delta[2]*v[2],14);
        if(local< -1024 || local>(axis==2?512:1024))return 0;
    }
    if(abs(tr2_word(a->yaw-o->pose.yaw))>14560 || abs(tr2_word(a->pitch-o->pose.pitch))>14560 || abs(tr2_word(a->control.roll-o->pose.control.roll))>14560)return 0;
    const int offset[3]={0,0,108};int v[3];tr2_rotate_offset(o->pose.yaw,o->pose.pitch,offset,v);
    int target[3]={o->pose.x+v[0],o->pose.y+v[1],o->pose.z+v[2]};delta[0]=target[0]-a->x;delta[1]=target[1]-a->y;delta[2]=target[2]-a->z;
    unsigned square=(unsigned)(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]),root=0,bit=1u<<30;
    while(bit>square)bit>>=2;while(bit){if(square>=root+bit){square-=root+bit;root=(root>>1)+bit;}else root>>=1;bit>>=2;}
    if(root<=16){a->x=target[0];a->y=target[1];a->z=target[2];}
    else{a->x+=delta[0]*16/(int)root;a->y+=delta[1]*16/(int)root;a->z+=delta[2]*16/(int)root;}
    int16_t *angle[3]={&a->yaw,&a->pitch,&a->control.roll};int desired[3]={o->pose.yaw,o->pose.pitch,o->pose.control.roll};
    for(int i=0;i<3;i++){int d=tr2_word(desired[i]-*angle[i]);*angle[i]=tr2_word(*angle[i]+(d>364?364:d< -364?-364:d));}
    if(a->x!=target[0] || a->y!=target[1] || a->z!=target[2] || a->yaw!=desired[0] || a->pitch!=desired[1] || a->control.roll!=desired[2])return 0;
    a->fall_speed=0;a->control.goal=40;for(int n=0;n<256 && a->control.current!=40;n++)tr2_animate(l,a);
    if(a->control.current!=40)return 0;a->control.goal=13;a->gun_status=1;
    o->pose.control.goal=o->pose.control.current==1?0:1;o->status=o->active=1;tr2_object_animate(l,o);return 1;
}
int tr2_switch_use(const Tr2Level *l,Tr2Object *o,Tr2Actor *a,uint32_t input){
    if(o->id==105)return swim_switch_use(l,o,a,input);
    if((o->id!=92 && o->id!=93 && o->id!=103 && o->id!=104) || !(input&TR2_ACTION) || o->status || a->gun_status || (a->control.flags&8) || a->control.current!=2)return 0;
    int dx=a->x-o->pose.x,dz=a->z-o->pose.z,s=tr2_sin(o->pose.yaw),c=tr2_cos(o->pose.yaw);
    int x=tr2_asr((int64_t)dx*c-(int64_t)dz*s,14),z=tr2_asr((int64_t)dx*s+(int64_t)dz*c,14);
    if(x< -220 || x>220 || z<292 || z>512 || a->y!=o->pose.y || abs(tr2_word(a->yaw-o->pose.yaw))>5460 || abs(a->pitch)>1820 || abs(a->control.roll)>1820)return 0;
    a->yaw=o->pose.yaw;a->pitch=o->pose.pitch;a->control.roll=o->pose.control.roll;
    if(o->id==92 && o->pose.control.current==1)return 0;
    int offset=o->id==92?212:o->id==93?362:292;int align_x=o->pose.x+tr2_asr(offset*s,14),align_z=o->pose.z+tr2_asr(offset*c,14);Tr2Height height;
    if((o->id==92 || o->id==93 || o->id==103) && tr2_query(l,align_x,o->pose.y,align_z,a->room,&height,NULL) && abs(height.floor-a->y)<=256 && abs(height.ceiling-a->y)>=762){a->x=align_x;a->y=o->pose.y;a->z=align_z;}
    if(o->id==92){
        const uint8_t *m=tr2_model(l,12);if(!m)return 0;
        tr2_set_animation(l,a,tr2_u16(m+16),0);a->control.current=0;a->control.goal=4;
        Tr2Object pose={0};pose.pose=*a;tr2_object_animate(l,&pose);*a=pose.pose;a->extra_animation=1;
        a->gun_status=1;o->pose.control.goal=1;o->active=o->status=1;tr2_object_animate(l,o);return 1;
    }
    a->control.current=o->pose.control.current==1?40:41;a->control.goal=2;tr2_set_animation(l,a,o->id==93?(o->pose.control.current==1?195:196):o->id==103?197:o->pose.control.current==1?63:64,0);a->gun_status=1;
    o->pose.control.goal=o->pose.control.current==1?0:1;o->active=1;o->status=1;tr2_object_animate(l,o);return 1;
}
static void flip_pair(Tr2Level *l,unsigned i){
    int alternate=l->rooms[i].alternate;if(alternate<0 || (uint32_t)alternate>=l->room_count)return;
    Tr2Room old=l->rooms[i];l->rooms[i]=l->rooms[alternate];l->rooms[alternate]=old;
    l->rooms[i].alternate=(int16_t)alternate;l->rooms[alternate].alternate=-1;
}
/* Geometry-only swap for snapshot restoration; runtime flip side effects
   must not execute again when loading already-serialized item/sector state. */
void tr2_flip_rooms(Tr2Level *l){for(uint32_t i=0;i<l->room_count;i++)if(l->rooms[i].alternate>=0)flip_pair(l,i);}
void tr2_world_flip(Tr2Level *l,Tr2World *w){
    for(uint32_t room=0;room<l->room_count;room++)if(l->rooms[room].alternate>=0){
        for(int i=w->room_head[room],n=0;i>=0 && (uint32_t)i<w->count && (uint32_t)n<w->count;n++){
            Tr2Object *o=w->objects+i;int next=o->next_room;
            if(o->id>=67 && o->id<=70)tr2_block_floor(l,o,1024);
            else if((o->flags&256) && tr2_object_intelligent(o->id) && o->pose.control.health<=0){tr2_world_room(w,i,-1);o->flags|=0x8000;}
            i=next;
        }
        w->room_head[l->rooms[room].alternate]=w->room_head[room];
        flip_pair(l,room);
        for(int i=w->room_head[room],n=0;i>=0 && (uint32_t)i<w->count && (uint32_t)n<w->count;n++){
            Tr2Object *o=w->objects+i;if(o->id>=67 && o->id<=70)tr2_block_floor(l,o,-1024);i=o->next_room;
        }
    }
    w->flip_status=!w->flip_status;
}
/* TestTriggers flip command branches, 0x4156a9..0x4157bc. */
int tr2_flip_command(Tr2World *w,int kind,int index,int type,int flags){
    if(index<0 || index>=10)return 0;uint32_t *f=w->flip_flags+index;
    if(kind==3){
        if(*f&256)return 0;
        if(type==2)*f^=(unsigned)flags&0x3e00;else *f|=(unsigned)flags&0x3e00;
        if((*f&0x3e00)==0x3e00){if(flags&256)*f|=256;return !w->flip_status;}
        return w->flip_status!=0;
    }
    if((*f&0x3e00)!=0x3e00)return 0;
    return kind==4?!w->flip_status:kind==5?w->flip_status!=0:0;
}
void tr2_world_trigger(const Tr2Level *l,Tr2World *w,Tr2Actor *a,uint32_t at,int heavy){
    if(!at)return;int header=floor_word(l,at++);
    if((header&255)==5){if(!heavy && (!(a->control.flags&8) || a->control.water_status))a->control.health=-1;if(header&32768)return;header=floor_word(l,at++);}
    if((header&255)==6){if(header&32768)return;header=floor_word(l,at++);}
    if((header&255)!=4)return;int type=(header>>8)&63,flags=floor_word(l,at++);if(flags<0)return;
    /* Refresh the camera before checking the switch state (original 0x4150d0). */
    unsigned scan=at+(type==2 || type==3 || type==4);
    int flip=0,flip_command=0,pending_effect=-1,pending_camera_target=-1,switch_off=0,refresh=2;
    for(unsigned limit=0;w->camera_active!=5 && limit<l->floors.count;limit++){
        int cmd=floor_word(l,scan++);if(cmd<0)break;int kind=(cmd>>10)&15,n=cmd&1023;
        if(kind==1){scan++;refresh=0;if(n==w->camera_last){w->camera_index=n;if(w->camera_timer>=0){w->camera_active=1;refresh=1;}}}
        else if(kind==6 && (uint32_t)n<w->count)w->camera_target=n;
        if(cmd&32768)break;
    }
    if((uint32_t)w->camera_target<w->count && (!refresh || (refresh==2 &&
       (w->objects[w->camera_target].pose.control.flags&64) && w->camera_target!=w->camera_previous_target)))w->camera_target=-1;
    if(heavy?type!=5:type==5 || type==8)return;
    if(type==1 || type==6){Tr2Height h;if(!tr2_query(l,a->x,a->y,a->z,a->room,&h,NULL) || h.floor!=a->y)return;}
    if(type==7 && a->gun_status!=4)return;
    if(type==3 || type==4){int n=floor_word(l,at++)&1023;if((uint32_t)n>=w->count)return;
        Tr2Object *o=w->objects+n;
        if(type==3){if(o->status!=1 || a->gun_status==1)return;}
        else if(o->status!=3)return;
        o->status=2;
    }
    if(type==2){int n=floor_word(l,at++)&1023;if((uint32_t)n>=w->count)return;Tr2Object *o=w->objects+n;if(o->status!=2)return;
        switch_off=o->pose.control.current==1; /* Root 41532d: timed switch reset. */
        if(o->pose.control.current==0 && (flags&255)>0){o->timer=(int16_t)((flags&255)==1?1:(flags&255)*30);o->status=1;}
        else{o->active=0;o->status=0;}}
    for(unsigned limit=0;limit<l->floors.count;limit++){
        int cmd=floor_word(l,at++);if(cmd<0)return;int n=cmd&1023,kind=(cmd>>10)&15;
        if(kind==0 && (uint32_t)n<w->count){Tr2Object *o=w->objects+n;
            if(!(o->flags&256)){o->timer=(int16_t)((flags&255)==1?1:(flags&255)*30);
                if(type==2)o->flags^=(uint16_t)(flags&0x3e00);else if(type==6 || type==9){o->flags&=(uint16_t)~(flags&0x3e00);if(flags&256)o->flags|=256;}else o->flags|=(uint16_t)(flags&0x3e00);
                if((o->flags&0x3e00)==0x3e00){if(flags&256)o->flags|=256;if(!o->active){o->active=1;
                    if((o->id>=15 && o->id<=18) || o->id==20 || o->id==21 || o->id==25 || o->id==28 || o->id==29 || o->id==30 || o->id==31 || o->id==34 || (o->id==32 || o->id==33) || o->id==36 || o->id==38 || tr2_xian_enemy(o->id) || tr2_tibetan_enemy(o->id) || o->id==47 || o->id==49 || o->id==50 || o->id==51 || o->id==39 || o->id==214)o->status=tr2_enemy_enable(l,w,n)?1:3;else o->status=1;}}}
        }else if(kind==1){int camera=floor_word(l,at++);
            if((uint32_t)n<l->cameras.count && n<1024 && !w->camera_used[n])w->camera_index=n;
            if((uint32_t)n<l->cameras.count && n<1024 && !w->camera_used[n] && type!=7 && (n!=w->camera_last || type==2) &&
               !(type==2 && (flags&255) && switch_off)){ /* Root 4155be..4155d2. */
                w->camera_index=n;w->camera_timer=(camera&255)*30;w->camera_speed=((camera>>6)&248)+1;w->camera_active=heavy?5:1;
                if(camera&256)w->camera_used[n]=1;
            }
            if(camera&32768)cmd|=32768;
        }
        else if(kind==2)tr2_current_trigger(l,w,a,n);
        else if(kind==6 && (uint32_t)n<w->count)pending_camera_target=n;
        else if(kind==7)w->finished=1;
        else if(kind==8)tr2_cd_trigger(w,n,flags,type);
        else if(kind>=3 && kind<=5){flip_command=1;if(tr2_flip_command(w,kind,n,type,flags))flip=1;}
        else if(kind==9)pending_effect=n;
        if(cmd&32768)break;
    }
    /* Root 415636/4157fc defers the target until the command list ends;
       target-before-camera triggers must aim correctly on their first frame. */
    if(w->camera_active && pending_camera_target>=0)w->camera_target=pending_camera_target;
    if(flip)tr2_world_flip((Tr2Level *)l,w);
    if(pending_effect>=0 && (flip || !flip_command)){w->effect=pending_effect;w->flip_effect=pending_effect;w->flip_timer=0;}
}
/* SoundEffects 0x41c540: fixed sound sources use separate normal/flipped
   flags, and are submitted every simulation tick (including loop refresh). */
int tr2_sound_source(const Tr2Level *l,unsigned index,int flipped,Tr2Position *p){
    if(index>=l->sounds.count)return -1;
    const uint8_t *s=l->sounds.data+index*16;
    if(!(tr2_u16(s+14)&(flipped?64:128)))return -1;
    *p=(Tr2Position){tr2_i32(s),tr2_i32(s+4),tr2_i32(s+8),0,0};
    return tr2_i16(s+12);
}
/* Flood effect 0x41d050, dispatched by SoundEffects 0x41c540. */
void tr2_flip_effect_tick(Tr2World *w,const Tr2Actor *a,int camera_target_y){
    w->flip_sound=-1;
    /* One-shot sound 25 -> 41d1b0 and explosion 11 -> 41d140. */
    if(w->flip_effect==25){w->flip_sound=331;w->flip_effect=-1;return;}
    if(w->flip_effect==11){w->flip_sound=105;w->camera_bounce=-75;w->flip_effect=-1;return;}
    /* Effect table slot8 -> 0x41d170: unpositioned sound190, once. */
    if(w->flip_effect==8 || w->flip_effect==9){w->flip_sound=w->flip_effect==8?190:191;w->flip_effect=-1;return;}
    /* Effect table slot10 -> 0x41d1d0: unpositioned sound330, once. */
    if(w->flip_effect==27){w->flip_sound=338;w->flip_effect=-1;return;}
    if(w->flip_effect==10){w->flip_sound=330;w->flip_effect=-1;return;}
    if(w->flip_effect==7){w->flip_sound=24;w->camera_bounce=-350;w->flip_effect=-1;return;}
    if(w->flip_effect==6){w->flip_sound=278;w->flip_timer++;if(w->flip_timer>30)w->flip_effect=-1;return;}
    if(w->flip_effect!=5)return;
    if(w->flip_timer>120)w->flip_effect=-1;
    else{
        w->flip_sound=79;
        w->flip_sound_position=(Tr2Position){a->x,camera_target_y+abs(w->flip_timer-30)*100,a->z,a->room,0};
    }
    w->flip_timer++;
}
/* Alarm controller 0x41d270: the item data word is a persistent pulse counter. */
void tr2_alarm_tick(Tr2World *w,Tr2Object *o){
    if((o->flags&0x3e00)!=0x3e00)return;
    Tr2Actor *a=&o->pose;
    if(a->event_count<16){int n=a->event_count++;a->event_kind[n]=5;a->event_value[n]=332;}
    o->effect_counter=tr2_long((int64_t)o->effect_counter+1);
    if(o->effect_counter>6){
        tr2_dynamic_light(w,a->x,a->y,a->z,12,11);
        if(o->effect_counter>12)o->effect_counter=0;
    }
}
uint32_t tr2_world_look_input(Tr2World *w,const Tr2Actor *a,Tr2Look *look,uint32_t input){
    if((uint32_t)w->vehicle>=w->count)return tr2_combat_look_input(w->combat,a,look,input);
    /* LaraAboveWater 0x427560 calls horizontal Look before the vehicle return.
       LookLeftRight 0x427770 suppresses torso yaw while mounted. */
    Tr2Actor rider=*a;rider.control.current=-1;int yaw=look->torso_yaw,pitch=look->torso_pitch;
    uint32_t result=tr2_look_input(look,&rider,input);
    if(look->active){look->torso_yaw=(int16_t)yaw;look->torso_pitch=(int16_t)pitch;}
    return result;
}
void tr2_world_view_tick(Tr2Level *l,Tr2World *w,Tr2Actor *a,uint32_t input,Tr2Look *look){
    w->lara=a;
    if(w->hazards){w->hazards->light_count=0;memset(w->hazards->lights,0,sizeof w->hazards->lights);}
    for(uint32_t i=0;i<w->count;i++){w->objects[i].pose.event_count=0;if(w->objects[i].listed_room>=0)tr2_world_room(w,(int)i,w->objects[i].pose.room);}

    if(w->inventory && !w->finished)w->inventory->ticks++;
    w->cd_play=w->cd_pending< -1?-w->cd_pending:-1;w->effect=-1;
    w->camera_previous_target=w->camera_target;
    /* Heavy cameras survive without repeated floor triggers (411ec2).
       Mines emit their trigger once, so resetting it loses the whole shot. */
    if(w->camera_active!=5 || w->camera_timer==-1){w->camera_index=w->camera_target=-1;w->camera_active=0;}
    if((uint32_t)w->vehicle<w->count && w->objects[w->vehicle].id==13 &&
       (!w->player_contacts_done || (w->objects[w->vehicle].flags&256)))tr2_snow_view_tick(l,w,w->vehicle,a,input,look);
    for(uint32_t i=0;i<w->count;i++){
        Tr2Object *o=w->objects+i;
        if(o->active){
            if(o->id>=106 && o->id<=113){int open=tr2_trigger_active(o->flags,&o->timer);
                if(open){if(o->pose.control.current==0)o->pose.control.goal=1;else door_set(l,o,1);}
                else{if(o->pose.control.current==1)o->pose.control.goal=0;else door_set(l,o,0);}tr2_object_animate(l,o);
            }else if(o->id==92 || o->id==93 || o->id==103 || o->id==104 || o->id==105){o->flags|=0x3e00;if(!tr2_trigger_active(o->flags,&o->timer)){o->pose.control.goal=1;o->timer=0;}tr2_object_animate(l,o);}
            else if(o->id==40 || o->id==87 || (o->id>=262 && o->id<=264))tr2_final_object(l,w,o,a);
            else if(o->id==14)tr2_boat_view_tick(l,w,(int)i,a,input,look);
            else if(o->id==64 || o->id==71 || o->id==86 || o->id==88)tr2_tibet_trap_tick(l,w,o,a);
            else if(o->id==65)tr2_lift_tick(l,w,o);
            else if(o->id==63 || o->id==66)tr2_object63_tick(l,w,o);
            else if(o->id==24)tr2_gondola_tick(l,w,o);
            else if(o->id==227)tr2_detonator_tick(l,w,o);
            else if(o->id==222)tr2_mine_tick(l,w,o,a);
            else if(o->id==213 || o->id==215 || o->id==216)tr2_venice_sound_tick(w,o);
            else if(o->id==212)tr2_alarm_tick(w,o);
            else if(o->id==257 || o->id==258)tr2_home_sound_tick(o);
            else if(o->id==102)tr2_zipline_tick(l,o,a);
            else if(o->id==259)tr2_helicopter_tick(l,w,(int)i,a);
            else if(o->id==26 || o->id==27){int radius=a->control.water_status==1?300:100;uint32_t touch=tr2_object_overlap(l,&o->pose,a,radius)?tr2_touch(l,&o->pose,o->id,a,0):0;tr2_eel_tick(l,w,(int)i,a,touch);}
            else if(o->id==228)tr2_object228_tick(l,w,(int)i);
            else if(o->id>=55 && o->id<=57)tr2_falling_block_tick(l,o,a);
            else if(o->id==256)tr2_finish_guarded(w,o,a);
            else if(o->id==60 || o->id==83 || o->id==101)tr2_boulder_tick(l,w,o,a);
            else if(o->id>=67 && o->id<=70)tr2_block_tick(l,w,o,a);
            else if(o->id==62 || o->id==253)tr2_emitter_tick(l,w,o,(int)i);
            else if(o->id==251)tr2_ember_emit(w,o);
            else if(o->id==78){uint32_t touch=tr2_object_overlap(l,&o->pose,a,100)?tr2_touch(l,&o->pose,o->id,a,0):0;tr2_blade78_tick(l,w,o,a,touch);}
            else if(o->id==76 || o->id==77 || o->id==94 || o->id==95){uint32_t touch=tr2_object_overlap(l,&o->pose,a,100)?tr2_touch(l,&o->pose,o->id,a,0):0;tr2_propeller_tick(l,w,o,a,touch);}
            else if(o->id==84){
                int radius=a->control.water_status==1?300:100;
                uint32_t touch=tr2_object_overlap(l,&o->pose,a,radius)?tr2_touch(l,&o->pose,o->id,a,0):0;
                tr2_icicle_tick(l,w,(int)i,a,touch);
            }
            else if(o->id==58 || o->id==96 || o->id==79){
                uint32_t touch=tr2_object_overlap(l,&o->pose,a,100)?tr2_touch(l,&o->pose,o->id,a,0):0;
                if(o->id==79)tr2_falling_trap_tick(l,w,o,a,touch);else tr2_swinging_trap_tick(l,w,o,a,touch);
            }
            else if(o->id==80 || o->id==81 || o->id==82 || o->id==85){
                uint32_t touch=tr2_object_overlap(l,&o->pose,a,100)?tr2_touch(l,&o->pose,o->id,a,0):0;
                if(o->id==82)tr2_pendulum_tick(l,w,o,a,touch);else tr2_wall_trap_tick(l,o,a,touch);
            }
            else if(o->id==114 || o->id==115){
                int open=tr2_trigger_active(o->flags,&o->timer);
                if(open && o->pose.control.current==0)o->pose.control.goal=1;
                else if(!open && o->pose.control.current==1)o->pose.control.goal=0;
                tr2_object_animate(l,o);
            }
        }
        if(o->id==14 && !w->player_contacts_done)tr2_boat_use(l,w,(int)i,a,input);
        if(w->vehicle<0){
        if(tr2_switch_use(l,o,a,input) && o->id!=105 && (l->custom.options&TR2_AUTOSAVE))l->custom.switch_pending=1;
        tr2_zipline_use(l,o,a,input);
        tr2_pickup_use(l,w,o,a,input);
        tr2_key_use(l,w,o,a,input);
        tr2_puzzle_use(l,w,o,a,input);
        if((o->id==226 || o->id==227) && !w->player_contacts_done){Tr2Contact c={0};c.radius=100;c.flags=24;c.old[0]=a->x;c.old[1]=a->y;c.old[2]=a->z;tr2_detonator_contact(l,w,o,a,input,&c);}
        tr2_block_use(l,o,a,input);
        }
        if(o->id==59 && tr2_object_overlap(l,&o->pose,a,100) && tr2_touch(l,&o->pose,o->id,a,0))tr2_spikes_contact(l,o,a);
        if((o->id==60 || o->id==83 || o->id==101) && !w->player_contacts_done && a->control.health>0){Tr2Contact c={0};c.radius=100;c.flags=24;c.old[0]=a->x;c.old[1]=a->y;c.old[2]=a->z;tr2_boulder_contact(l,w,o,a,&c);}
    }
    Tr2Height h;if(w->vehicle<0 && tr2_query(l,a->x,a->y,a->z,a->room,&h,NULL))tr2_world_trigger(l,w,a,h.trigger,0);
    tr2_home_objects(l,w,a);
    for(int i=0;i<TR2_MAX_DARTS;i++){Tr2Object *o=w->hazards->darts+i;if(o->active && o->id==152)tr2_pickup_use(l,w,o,a,input);}
    tr2_hazards_tick(l,w,a);
    tr2_inventory_reward(w,a);
    /* AboveWater 4275d4/4276ed still runs LaraGun for the boat rider. */
    if(w->vehicle<0 || ((uint32_t)w->vehicle<w->count && w->objects[w->vehicle].id==14))tr2_combat_view_tick(l,w,a,input,look);
    for(uint32_t i=0;i<w->count;i++)tr2_enemy_tick(l,w,(int)i,a);
    if(w->assault_active)w->assault_ticks++;
    if(w->effect==28){w->assault_active=w->assault_visible=0;}
    else if(w->effect==29){w->assault_active=0;w->assault_visible=1;}
    else if(w->effect==30){w->assault_ticks=0;w->assault_active=w->assault_visible=1;}
    else if(w->effect==31 && w->assault_active){
        tr2_assault_record(&w->assault_scores,(uint32_t)w->assault_ticks);
        if(w->assault_best<0){w->cd_play=w->assault_ticks<3000?22:24;w->assault_best=w->assault_ticks<3000?w->assault_ticks:3000;}
        else if(w->assault_ticks<w->assault_best){w->cd_play=22;w->assault_best=w->assault_ticks;}
        else w->cd_play=w->assault_ticks<w->assault_best+150?23:21;
        w->assault_active=0;
    }
    w->player_contacts_done=0;
    if(w->cd_play>1)w->cd_pending=w->cd_play;
}
void tr2_falling_block_tick(const Tr2Level *l,Tr2Object *o,const Tr2Actor *lara){
    Tr2Actor *a=&o->pose;
    if(a->control.current==0){
        if(lara->y!=a->y-(o->id==57?1024:512)){o->status=o->active=0;return;}
        a->control.goal=1;
    }else if(a->control.current==1)a->control.goal=2;
    else if(a->control.current==2 && a->control.goal!=3)a->control.flags|=8;
    tr2_object_animate(l,o);if(o->status==2){o->active=0;return;}
    Tr2Height h;Tr2Sector ref;if(!tr2_query(l,a->x,a->y,a->z,a->room,&h,&ref))return;
    a->room=(int16_t)ref.room;
    if(a->control.current==2 && a->y>=h.floor){a->control.flags&=(uint16_t)~8u;a->control.goal=3;a->y=h.floor;a->fall_speed=0;}
}
void tr2_world_camera(const Tr2Level *l,Tr2World *w,const Tr2Actor *a,Tr2Camera *c,Tr2Look *look){
    w->lara=a;
    if(w->cinematic_active){
        if(!c->initialized)c->position.room=a->room;
        tr2_cinematic_camera(l,&w->cinematic_origin,w->cinematic_yaw,&w->cinematic_frame,c,&w->cinematic_fov,&w->cinematic_roll);
        w->ai_camera[0]=c->position.x;w->ai_camera[1]=c->position.y;w->ai_camera[2]=c->position.z;return;
    }
    /* RefreshCamera 0x415140: Look cancels the current timed camera; its
       remaining time must not resume when Look is released. */
    if(look && look->active){if(w->camera_active){w->camera_timer=-1;w->camera_active=0;}w->camera_target=-1;}
    int scripted=0;
    if(!w->camera_active && look && !look->active && (uint32_t)w->camera_target<w->count){
        Tr2Actor *target=&w->objects[w->camera_target].pose;
        scripted=tr2_scripted_look(l,a,target,look);
        if(scripted){target->control.flags|=64;look->active=1;}
    }
    if(w->camera_active && (!look || !look->active) && (uint32_t)w->camera_index<l->cameras.count){
        const Tr2Actor *target=(uint32_t)w->camera_target<w->count?&w->objects[w->camera_target].pose:a;
        int explicit_target=target!=a;
        int b[6],y=target->y-762;
        if(tr2_actor_bounds(l,target,b))y=target->y+(explicit_target?(b[2]+b[3])/2:b[3]+tr2_asr(3*(b[2]-b[3]),2));
        /* CalculateCamera 411b6c/411d87: Lara uses the three-quarter
           bounds height, with quarter-step tracking. Only a transition
           to/from an explicit camera target snaps the target height. */
        int previous_explicit=w->camera_fixed_target;
        w->camera_fixed_target=explicit_target;
        if(c->initialized && explicit_target==previous_explicit)y=c->target.y+(y-c->target.y)/4;
        else if(explicit_target!=previous_explicit)w->camera_speed=1; /* 411d9c */
        c->target=(Tr2Position){target->x,y,target->z,target->room,0};c->initialized=1;
        const uint8_t *p=l->cameras.data+w->camera_index*16;
        Tr2Position ideal={tr2_i32(p),tr2_i32(p+4),tr2_i32(p+8),tr2_i16(p+12),0};
        if(!tr2_camera_los(l,&c->target,&ideal,0))tr2_camera_clamp(l,&ideal,256);
        tr2_camera_move(l,c,ideal,w->camera_speed);
        if(w->camera_timer && --w->camera_timer==0)w->camera_timer=-1;
    }else{
        tr2_camera_weapon_tick(l,a,c,look,w->combat);
        w->camera_fixed_target=0;
    }
    if(scripted)look->active=0; /* Not a held player Look input next tick. */
    w->camera_last=w->camera_index;
    if(w->camera_active!=5 || w->camera_timer==-1)w->camera_speed=10;
    w->ai_camera[0]=c->position.x;w->ai_camera[1]=c->position.y;w->ai_camera[2]=c->position.z;
}
int tr2_zipline_use(const Tr2Level *l,Tr2Object *o,Tr2Actor *a,uint32_t input){
    if(o->id!=102 || !(input&TR2_ACTION) || o->status || a->gun_status || (a->control.flags&8) || a->control.current!=2)return 0;
    int dx=a->x-o->pose.x,dz=a->z-o->pose.z,s=tr2_sin(o->pose.yaw),c=tr2_cos(o->pose.yaw);
    int x=tr2_asr((int64_t)dx*c-(int64_t)dz*s,14),z=tr2_asr((int64_t)dx*s+(int64_t)dz*c,14);
    if(x< -256 || x>256 || z<256 || z>512 || abs(a->y-o->pose.y)>100 || abs(tr2_word(a->yaw-o->pose.yaw))>4550 || a->pitch || a->control.roll)return 0;
    a->yaw=o->pose.yaw;a->pitch=o->pose.pitch;a->control.roll=o->pose.control.roll;
    a->x=o->pose.x+tr2_asr(371*s,14);a->y=o->pose.y;a->z=o->pose.z+tr2_asr(371*c,14);
    a->gun_status=1;a->control.goal=70;
    for(int n=0;n<256 && a->control.current!=19;n++)tr2_animate(l,a);
    o->active=o->status=1;o->flags|=256;return 1;
}
void tr2_zipline_tick(const Tr2Level *l,Tr2Object *o,Tr2Actor *a){
    if(o->status!=1)return;
    Tr2Actor *p=&o->pose;
    if(!(o->flags&256)){
        /* Reset to the level's recorded origin after reactivation. */
        for(uint32_t i=0;i<l->items.count;i++){const uint8_t *it=l->items.data+i*24;if(tr2_u16(it)==102){p->x=tr2_i32(it+4);p->y=tr2_i32(it+8);p->z=tr2_i32(it+12);p->room=tr2_i16(it+2);break;}}
        const uint8_t *m=tr2_model(l,102);if(m)tr2_set_animation(l,p,tr2_u16(m+16),0);p->control.current=p->control.goal=1;o->active=o->status=0;return;
    }
    int starting=p->control.current==1;tr2_object_animate(l,o);if(starting)return;
    if(p->fall_speed<100)p->fall_speed+=5;
    int s=tr2_sin(p->yaw),c=tr2_cos(p->yaw);p->x+=tr2_asr(p->fall_speed*s,14);p->z+=tr2_asr(p->fall_speed*c,14);p->y+=p->fall_speed>>2;
    Tr2Sector sector;if(tr2_find_sector(l,p->x,p->y,p->z,p->room,&sector))p->room=(int16_t)sector.room;
    int riding=a->control.current==70;if(riding){a->x=p->x;a->y=p->y;a->z=p->z;}
    Tr2Height h;int x=p->x+tr2_asr(s*1024,14),z=p->z+tr2_asr(c*1024,14),y=p->y+64;
    int clear=tr2_query(l,x,y,z,p->room,&h,NULL) && h.floor>y+256 && h.ceiling<y-256;
    if(!clear){if(riding){a->control.goal=3;tr2_animate(l,a);a->control.flags|=8;a->speed=p->fall_speed;a->fall_speed=p->fall_speed>>2;}o->active=o->status=0;o->flags=(uint16_t)(o->flags-256);}
    if(p->event_count<16){int n=p->event_count++;p->event_kind[n]=5;p->event_value[n]=clear?280:281;}
}

void tr2_world_tick(Tr2Level *l,Tr2World *w,Tr2Actor *a,uint32_t input){tr2_world_view_tick(l,w,a,input,NULL);}








