#include "tr2_world.h"
#include <stdlib.h>
#include <string.h>
/* Intelligent object flags from the root's 0x43a530 setup table. */
int tr2_xian_enemy(int id){return id==19 || id==22 || id==37 || id==41 || id==43;}
int tr2_tibetan_enemy(int id){return id==45 || id==46 || id==48 || id==53 || id==54;}
int tr2_object_intelligent(int id){
    static const int ids[]={15,16,17,18,19,20,21,22,25,28,29,30,31,32,33,34,35,36,37,38,39,41,43,45,46,47,48,49,50,51,53,54,214,260};
    for(unsigned i=0;i<sizeof ids/sizeof ids[0];i++)if(ids[i]==id)return 1;return 0;
}
/* GetCarriedItems 0x43c830 attaches exactly colocated pickups in room-list
   order, removing them from the world until CreatureDie releases them. */
void tr2_carried_init(Tr2World *w){
    for(uint32_t i=0;i<w->count;i++)w->objects[i].carried=-1;
    for(uint32_t i=0;i<w->count;i++){
        Tr2Object *o=w->objects+i;Tr2Actor *a=&o->pose;if(!tr2_object_intelligent(o->id) || a->room<0 || a->room>=1024)continue;
        for(int j=w->room_head[a->room],n=0;j>=0 && (uint32_t)j<w->count && (uint32_t)n<w->count;n++){
            Tr2Object *pickup=w->objects+j;int next=pickup->next_room;
            if(tr2_pickup_id(pickup->id) && pickup->pose.x==a->x && pickup->pose.y==a->y && pickup->pose.z==a->z){
                pickup->carried=o->carried;o->carried=j;tr2_world_room(w,j,-1);pickup->pose.room=255;
            }
            j=next;
        }
    }
}
/* CreatureDie 0x40f4b2. A restored terminal corpse must not move pickups
   already released or collected, hence the carried-room check. */
void tr2_carried_drop(Tr2World *w,int index){
    if((uint32_t)index>=w->count)return;Tr2Object *o=w->objects+index;
    for(int i=o->carried,n=0;i>=0 && (uint32_t)i<w->count && (uint32_t)n<w->count;n++){
        Tr2Object *p=w->objects+i;if(p->pose.room==255){p->pose.x=o->pose.x;p->pose.y=o->pose.y;p->pose.z=o->pose.z;p->pose.room=o->pose.room;tr2_world_room(w,i,o->pose.room);}
        i=p->carried;
    }
}

int tr2_inventory_weapon(int id){static const int ids[]={0,135,137,138,136,140,141,139};for(int i=1;i<8;i++)if(ids[i]==id)return i;return 0;}
/* UseItem 430d10: all seven guns, flares, and half/full medipacks. */
int tr2_inventory_use(Tr2World *w,Tr2Actor *a,int id){
    if(id<0 || id>=265 || w->inventory->count[id]<=0)return 0;
    int weapon=tr2_inventory_weapon(id);
    if(weapon || id==151){w->combat->requested=id==151?8:weapon;return 1;}
    if(id==149 || id==150){
        if(a->control.health<=0 || a->control.health>=1000)return 0;
        int health=a->control.health+(id==149?500:1000);
        a->control.health=(int16_t)(health>1000?1000:health);
        w->inventory->count[id]--;w->inventory->medipacks=(w->inventory->medipacks+(id==149?1:2))&255;
        return 116;
    }
    w->inventory->selected=id;return 1;
}
/* Inv_AddItem 0x4242d0. Ammunition is kept in the executable's units:
   shotgun shells contain six pellets; a pickup adds twelve pellets. */
void tr2_inventory_add(Tr2World *w,int id){
    static const int ids[]={0,135,137,138,136,140,141,139},packs[]={0,0,40,80,12,40,2,3};
    Tr2Inventory *v=w->inventory;Tr2Combat *g=w->combat;if(id<0 || id>=265)return;
    if(id==152){v->count[151]++;return;}
    int weapon=tr2_inventory_weapon(id);
    if(weapon){if(v->count[id]){v->count[id]++;return;}if(weapon>1){int ammo_id=id+7;g->ammo[weapon]+=packs[weapon]*(1+v->count[ammo_id]);v->count[ammo_id]=0;for(uint32_t i=0;i<w->count;i++)if(w->objects[i].id==id)w->objects[i].id=ammo_id;}v->count[id]=1;return;}
    weapon=tr2_inventory_weapon(id-7);
    if(weapon>1 && v->count[ids[weapon]]){g->ammo[weapon]+=packs[weapon];return;}
    v->count[id]+=id==151?6:1;
}
static void pickup_event(Tr2Actor *a,int id){
    if(!a || a->event_count>=16)return;
    int event=a->event_count++;
    a->event_kind[event]=8;
    a->event_value[event]=id;
}
void tr2_inventory_reward(Tr2World *w,Tr2Actor *a){
    static const int ids[27]={135,136,137,138,139,140,141,142,143,144,145,146,147,148,152,149,150,205,206,174,175,176,177,193,194,195,196};
    if(w->rewarded || (w->inventory->secrets&7)!=7)return;w->rewarded=1;
    for(int i=0;i<27;i++){
        int id=ids[i],count=w->rewards[i];
        if(i<7)count=count && !w->inventory->count[id];
        for(int n=0;n<count;n++){
            pickup_event(a,id);
            tr2_inventory_add(w,id);
        }
    }
}
void tr2_campaign_entry(Tr2World *w){
    /* InitialiseLaraMeshes 4316b2: choose the back mesh from owned guns
       after the level's equipment rules have run. */
    const int back_items[]={136,140,141,139}; /* shotgun, M16, grenade launcher, harpoon */
    w->combat->back_weapon=0;
    for(unsigned i=0;i<sizeof back_items/sizeof back_items[0];i++){
        if(w->inventory->count[back_items[i]]){
            w->combat->back_weapon=tr2_inventory_weapon(back_items[i]);
            break;
        }
    }
    /* InitialiseLaraInventory calls GlobalItemReplace for owned long guns. */
    for(uint32_t i=0;i<w->count;i++){int id=w->objects[i].id;if(tr2_inventory_weapon(id)>1 && w->inventory->count[id])w->objects[i].id=id+7;}
    *w->start_inventory=*w->inventory;*w->start_combat=*w->combat;
}
/* ModifyStartInfo 0x439200, normal game (bonus flag clear). */
void tr2_campaign_start(Tr2World *w,int level){
    memset(w->inventory,0,sizeof *w->inventory);w->inventory->selected=-1;memset(w->combat,0,sizeof *w->combat);w->combat->target=-1;
    w->combat->weapon=w->combat->requested=level?1:0;
    if(level){w->inventory->count[135]=1;w->combat->ammo[1]=1000;w->combat->last_weapon=1;} /* 439210..43922e */
    if(level==1){w->inventory->count[135]=w->inventory->count[136]=1;w->inventory->count[149]=w->inventory->count[150]=1;w->inventory->count[151]=2;w->combat->ammo[1]=1000;w->combat->ammo[4]=12;w->combat->back_weapon=4;w->combat->last_weapon=1;}
    tr2_campaign_entry(w);
}
void tr2_campaign_carry(Tr2World *next,const Tr2World *previous){
    *next->inventory=*previous->inventory;Tr2Inventory *v=next->inventory;v->selected=-1;v->pickups=v->secrets=v->kills=v->ticks=v->medipacks=v->distance=0;
    for(int id=174;id<=177;id++)v->count[id]=0;for(int id=190;id<=196;id++)v->count[id]=0;
    Tr2Combat *g=next->combat;
    memset(g,0,sizeof *g);
    memcpy(g->ammo,previous->combat->ammo,sizeof g->ammo);
    /* Normal level entry runs ModifyStartInfo (439210..439229) after
       completion records the selected weapon. Scripted confiscation follows. */
    v->count[135]=1;
    g->ammo[1]=1000;
    g->weapon=g->requested=g->last_weapon=1;
    g->target=-1;
    tr2_campaign_entry(next);
}
/* InitialiseLaraInventory 0x4312a0. Apply only to fresh level entry,
   before play; a restart or saved game already contains the entry result. */
void tr2_campaign_inventory_flags(Tr2World *w,int flags){
    Tr2Inventory *v=w->inventory;Tr2Combat *g=w->combat;
    static const int ids[]={0,135,137,138,136,140,141,139},packs[]={0,0,40,80,12,40,2,3};
    if(flags&1){
        for(int weapon=2;weapon<8;weapon++){
            int id=ids[weapon];
            if(v->count[id])v->count[id+7]=(uint16_t)g->ammo[weapon]/packs[weapon];
            v->count[id]=0;g->ammo[weapon]=0;
        }
        v->count[135]=0;g->weapon=g->requested=g->last_weapon=g->back_weapon=0;
        /* Carry has already replaced owned gun pickups with ammo. Restore
           fresh placement IDs now that none of those guns is owned. */
        for(uint32_t i=0;i<w->level->items.count;i++){
            int id=tr2_i16(w->level->items.data+i*24);
            if(tr2_inventory_weapon(id)>1 && w->objects[i].id==id+7)w->objects[i].id=id;
        }
    }
    if(flags&2){
        for(int id=142;id<=151;id++)v->count[id]=0;
        for(int weapon=2;weapon<8;weapon++)g->ammo[weapon]=0;
    }
    if(flags){g->ammo[1]=1000;tr2_campaign_entry(w);}
}

int tr2_pickup_id(int id){return (id>=135 && id<=152) || (id>=174 && id<=177) || (id>=190 && id<=196) || id==205 || id==206;}
static void local_position(const Tr2Actor *o,const Tr2Actor *a,int yaw,int *x,int *z){
    int dx=a->x-o->x,dz=a->z-o->z,s=tr2_sin(yaw),c=tr2_cos(yaw);
    *x=tr2_asr((int64_t)dx*c-(int64_t)dz*s,14);*z=tr2_asr((int64_t)dx*s+(int64_t)dz*c,14);
}
static int change_state(const Tr2Level *l,Tr2Actor *a,int goal){
    a->control.goal=(int16_t)goal;
    for(int n=0;n<256 && a->control.current!=goal;n++)tr2_animate(l,a);
    return a->control.current==goal;
}
static void align(const Tr2Level *l,Tr2Actor *a,const Tr2Actor *o,int yaw,int z){
    int x=o->x+tr2_asr(z*tr2_sin(yaw),14),nz=o->z+tr2_asr(z*tr2_cos(yaw),14);Tr2Height h;
    a->yaw=(int16_t)yaw;a->pitch=o->pitch;a->control.roll=o->control.roll;
    if(tr2_query(l,x,o->y,nz,a->room,&h,NULL) && abs(h.floor-a->y)<=256 && abs(h.ceiling-a->y)>=762){a->x=x;a->y=o->y;a->z=nz;}
}
static int swim_align(const Tr2Level *l,Tr2Actor *a,const Tr2Actor *o,int flare){
    const int offset[3]={0,-200,-350};int v[3];tr2_rotate_offset(a->yaw,-4550,offset,v);
    int target[3]={o->x+v[0],o->y+v[1],o->z+v[2]},delta[3]={target[0]-a->x,target[1]-a->y,target[2]-a->z};
    unsigned square=(unsigned)(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]),root=0,bit=1u<<30;
    while(bit>square)bit>>=2;
    while(bit){if(square>=root+bit){square-=root+bit;root=(root>>1)+bit;}else root>>=1;bit>>=2;}
    if(flare){Tr2Height h;if(!tr2_query(l,target[0],target[1],target[2],a->room,&h,NULL) || abs(h.floor-a->y)>512)return 0;if(root<128)return 1;}
    if(root<=16){a->x=target[0];a->y=target[1];a->z=target[2];}
    else{a->x+=delta[0]*16/(int)root;a->y+=delta[1]*16/(int)root;a->z+=delta[2]*16/(int)root;}
    int dp=tr2_word(-4550-a->pitch),dr=tr2_word(-a->control.roll);
    a->pitch=tr2_word(a->pitch+(dp>364?364:dp< -364?-364:dp));
    a->control.roll=tr2_word(a->control.roll+(dr>364?364:dr< -364?-364:dr));
    return a->x==target[0] && a->y==target[1] && a->z==target[2] && a->pitch==-4550 && !a->control.roll;
}
/* Ground/wading PickupCollision branch, 0x43818b. Pickup happens at the
   original contact frame, not when Action is first pressed. */
int tr2_pickup_use(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *a,uint32_t input){
    if(!w->inventory || !tr2_pickup_id(o->id) || o->status==2 || o->status==3 || (uint32_t)o->pose.room>=l->room_count)return 0;
    int swim=a->control.water_status==1;
    if(!swim && a->control.water_status!=0 && a->control.water_status!=4)return 0;
    int x,z;local_position(&o->pose,a,a->yaw,&x,&z);
    if(swim){
        int delta[3]={a->x-o->pose.x,a->y-o->pose.y,a->z-o->pose.z};
        for(int axis=0;axis<3;axis++){int basis[3]={0,0,0},v[3];basis[axis]=16384;tr2_rotate_offset(a->yaw,-4550,basis,v);
            if(abs(tr2_asr((int64_t)delta[0]*v[0]+(int64_t)delta[1]*v[1]+(int64_t)delta[2]*v[2],14))>512)return 0;}
        if(abs(tr2_word(a->pitch+4550))>8190 || abs(a->control.roll)>8190)return 0;
    }else if(abs(x)>256 || abs(z)>256 || abs(a->y-o->pose.y)>100 || abs(a->pitch)>1820 || a->control.roll)return 0;
    if(a->control.current==39){
        if(o->id==152)return 0;
        if(a->control.frame!=tr2_i16(l->animations.data+(swim?130:135)*32+16)+(swim?18:42))return 0;
        pickup_event(a,o->id);
        tr2_inventory_add(w,o->id);w->inventory->pickups++;
        if(o->id>=190 && o->id<=192)w->inventory->secrets|=1<<(o->id-190);
        o->status=3;o->active=0;return 2;
    }
    if(a->control.current==67){
        if(o->id!=152 || !w->combat || w->combat->weapon==8 || a->control.frame!=tr2_i16(l->animations.data+(swim?206:204)*32+16)+(swim?20:58))return 0;
        Tr2Combat *g=w->combat;if(g->weapon>0 && g->weapon<8)g->last_weapon=g->weapon;g->weapon=g->requested=8;g->flare_age=o->timer&32767;g->target=-1;g->meshes=1;g->rifle_active=g->flare_control=0;memset(g->arms,0,sizeof g->arms);a->gun_status=5;
        o->status=3;o->active=0;return 2;
    }
    if(!(input&TR2_ACTION) || a->control.current!=(swim?13:2) || a->gun_status || (!swim && (a->control.flags&8)))return 0;
    if(o->id==152){
        if(!w->combat || w->combat->weapon==8)return 0;
        if(swim){if(!swim_align(l,a,&o->pose,1))return 0;tr2_set_animation(l,a,206,0);a->fall_speed=0;a->control.current=67;a->control.goal=13;return 1;}
        if(!change_state(l,a,67))return 0;a->control.goal=2;a->gun_status=1;return 1;
    }
    if(swim){if(!swim_align(l,a,&o->pose,0) || !change_state(l,a,39))return 0;a->control.goal=13;return 1;}
    align(l,a,&o->pose,a->yaw,-100);
    if(!change_state(l,a,39))return 0;a->control.goal=2;a->gun_status=1;return 1;
}
/* KeyHoleCollision's use path, 0x438ab0; selected inventory IDs are world
   pickup IDs here, while the executable's UI uses option IDs four higher. */
static void puzzle_refuse(Tr2World *w,Tr2Actor *a);
static int has_ring_item(const Tr2Inventory *inventory){
    for(int i=174;i<=206;i++)if((i<=177 || (i>=193 && i<=196) || i>=205) && inventory->count[i]>0)return 1;
    return 0;
}
int tr2_key_use(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *a,uint32_t input){
    if(!w->inventory || o->id<201 || o->id>204 || a->control.current!=2 || a->gun_status || (a->control.flags&8))return 0;
    if(!(input&TR2_ACTION) && w->inventory->selected<0)return 0;
    int x,z;local_position(&o->pose,a,o->pose.yaw,&x,&z);
    if(abs(x)>200 || z<312 || z>512 || a->y!=o->pose.y || abs(a->pitch-o->pose.pitch)>1820 || abs(tr2_word(a->yaw-o->pose.yaw))>5460 || abs(a->control.roll-o->pose.control.roll)>1820)return 0;
    if(o->status){puzzle_refuse(w,a);return 0;}
    int key=o->id-8,selected=w->inventory->selected;
    if(selected<0){ /* Root 4389c4: DisplayInventory(INV_KEYS). */
        if(has_ring_item(w->inventory)){w->inventory_request=1;return 0;}
        puzzle_refuse(w,a);return 0;
    }
    w->refusal_position[1]=a->y-1;w->inventory->selected=-1;
    if(selected!=key || w->inventory->count[key]<=0){puzzle_refuse(w,a);return 0;}
    w->inventory->count[key]--;
    align(l,a,&o->pose,o->pose.yaw,362);
    if(!change_state(l,a,42))return 0;a->control.goal=2;a->gun_status=1;o->status=1;return 1;
}

static void puzzle_refuse(Tr2World *w,Tr2Actor *a){
    if(w->refusal_position[0]==a->x && w->refusal_position[1]==a->y && w->refusal_position[2]==a->z)return;
    w->refusal_position[0]=a->x;w->refusal_position[1]=a->y;w->refusal_position[2]=a->z;
    if(a->event_count<16){int n=a->event_count++;a->event_kind[n]=5;a->event_value[n]=2;}
}
/* PuzzleHoleCollision 0x438b30. The filled model is installed at frame80. */
int tr2_puzzle_use(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *a,uint32_t input){
    if(!w->inventory || o->id<182 || o->id>185)return 0;
    if(a->control.current!=43 && (a->control.current!=2 || a->gun_status || (a->control.flags&8) || (!(input&TR2_ACTION) && w->inventory->selected<0)))return 0;
    int x,z;local_position(&o->pose,a,o->pose.yaw,&x,&z);
    if(abs(x)>200 || z<312 || z>512 || a->y!=o->pose.y || abs(a->pitch-o->pose.pitch)>1820 || abs(tr2_word(a->yaw-o->pose.yaw))>5460 || abs(a->control.roll-o->pose.control.roll)>1820)return 0;
    if(a->control.current==43){
        if(a->control.frame==tr2_i16(l->animations.data+134*32+16)+80){o->id+=4;return 2;}return 0;
    }
    if(o->status){puzzle_refuse(w,a);return 0;}
    int selected=w->inventory->selected,key=o->id-8;
    if(selected<0){
        if(has_ring_item(w->inventory)){w->inventory_request=1;return 0;}
        puzzle_refuse(w,a);return 0;
    }
    w->refusal_position[1]=a->y-1;w->inventory->selected=-1;
    if(selected!=key || w->inventory->count[key]<=0){puzzle_refuse(w,a);return 0;}
    w->inventory->count[key]--;align(l,a,&o->pose,o->pose.yaw,327);
    if(!change_state(l,a,43))return 0;a->control.goal=2;a->gun_status=1;o->status=1;
    w->refusal_position[0]=a->x;w->refusal_position[1]=a->y;w->refusal_position[2]=a->z;return 1;
}

/* GongBongerCollision 0x4386b0, detonator227 branch. Inventory option198
   corresponds to placed key194. The object faces Lara during acceptance. */
int tr2_detonator_use(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *a,uint32_t input){
    int gong=o->id==226;
    if((!gong && o->id!=227) || a->extra_animation || o->status==2 || !(input&TR2_ACTION) || a->gun_status || (a->control.flags&8) || a->control.current!=2)return 0;
    int x,z;local_position(&o->pose,a,a->yaw,&x,&z);
    if(x<(gong?-512:-256) || x>(gong?1024:256) || z<(gong?-812:-256) || z>(gong?-412:256) || a->y-o->pose.y< -100 || a->y-o->pose.y>100 || abs(a->pitch)>(gong?5460:1820) || a->control.roll)return 0;
    if(w->inventory->selected<0 && has_ring_item(w->inventory)){
        /* Resume this contact after the key ring returns (438782). */
        w->inventory_request=2+(int)(o-w->objects);
        return 0;
    }
    if(w->inventory->selected!=194 || w->inventory->count[194]<=0)return 0;
    const uint8_t *m=tr2_model(l,12);if(!m)return 0;
    w->inventory->count[194]--;w->inventory->selected=-1;
    if(!gong){o->pose.yaw=a->yaw;o->pose.pitch=o->pose.control.roll=0;}
    align(l,a,&o->pose,gong?o->pose.yaw:a->yaw,0);tr2_set_animation(l,a,tr2_u16(m+16),0);a->control.current=0;a->control.goal=gong?5:1;
    if(gong)a->yaw=tr2_word(a->yaw+32768);
    Tr2Object pose={0};pose.pose=*a;tr2_object_animate(l,&pose);*a=pose.pose;
    a->extra_animation=1;a->gun_status=1;
    if(!gong)o->active=o->status=1;
    else for(int i=0;i<TR2_MAX_DARTS;i++){
        Tr2Object *d=w->hazards->darts+i;const uint8_t *striker=tr2_model(l,225);
        if(d->active || !striker)continue;
        *d=(Tr2Object){0};d->id=225;d->active=d->status=1;
        d->pose.x=a->x;d->pose.y=a->y;d->pose.z=a->z;d->pose.yaw=a->yaw;d->pose.room=a->room;
        tr2_set_animation(l,&d->pose,tr2_u16(striker+16),0);
        d->pose.control.current=d->pose.control.goal=tr2_i16(l->animations.data+d->pose.control.animation*32+6);
        break;
    }
    return 1;
}

/* Failed GongBongerCollision restores orientation then calls ObjectCollision. */
int tr2_detonator_contact(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *a,uint32_t input,Tr2Contact *contact){
    if(a->extra_animation)return 0;
    if(tr2_detonator_use(l,w,o,a,input) || w->inventory_request>=2)return 1;
    if((contact->flags&8) && tr2_object_overlap(l,&o->pose,a,contact->radius) && tr2_touch(l,&o->pose,o->id,a,0))tr2_object_push(l,&o->pose,a,contact);
    return 0;
}
