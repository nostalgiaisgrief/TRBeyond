#include "tr2_world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* Versioned native snapshots. Process addresses are never written. Geometry
   is included because doors and movable objects alter collision sectors. */
typedef struct {uint32_t magic,version,size,checksum,level,objects,rooms,boxes,index;} Header;
enum { SAVE_VERSION=6 };
typedef struct {Tr2World world;Tr2Actor actor;Tr2Water water;Tr2Inventory inventory;Tr2Hazards hazards;Tr2Combat combat,start_combat;Tr2Inventory start_inventory;} State;
static uint32_t hash(const void *data,size_t n){const unsigned char *p=data;uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
static uint32_t identity(const Tr2Level *l){return hash(l->items.data,l->items.count*24)^hash(l->frames.data,l->frames.count*2);}
static size_t length(const Tr2Level *l,const Tr2World *w){
    size_t n=sizeof(State)+w->count*(sizeof(Tr2Object)+sizeof(Tr2Creature))+sizeof(Tr2Fragment)*TR2_MAX_FRAGMENTS+l->boxes.count*8;
    for(uint32_t i=0;i<l->room_count;i++)n+=l->rooms[i].sectors.count*8;
    /* Reserve LOT storage for every item so load size does not depend on the
       currently active creatures. A saved presence flag controls allocation. */
    return n+w->count*(1+l->boxes.count*sizeof(Tr2LotNode));
}
static void transfer(unsigned char **p,void *data,size_t n,int load){if(load)memcpy(data,*p,n);else memcpy(*p,data,n);*p+=n;}
int tr2_slot_write(const char *path,int index,const Tr2Level *l,const Tr2World *w,const Tr2Actor *a,const Tr2Water *water,const Tr2CampaignTotals *totals){
    size_t n=length(l,w)+sizeof *totals+sizeof l->custom;if(n>UINT32_MAX)return 0;unsigned char *data=calloc(1,n),*p=data;if(!data)return 0;
    State s={0};s.world=*w;s.actor=*a;s.water=*water;s.inventory=*w->inventory;s.hazards=*w->hazards;s.combat=*w->combat;s.start_combat=*w->start_combat;s.start_inventory=*w->start_inventory;
    s.world.lara=NULL;s.world.objects=NULL;s.world.level=NULL;s.world.fragments=NULL;s.world.creatures=NULL;s.world.inventory=NULL;s.world.hazards=NULL;s.world.combat=NULL;s.world.start_inventory=NULL;s.world.start_combat=NULL;
    transfer(&p,&s,sizeof s,0);
    for(uint32_t i=0;i<w->count;i++){Tr2Object o=w->objects[i];for(int j=0;j<4;j++)o.door[j].sector=NULL;transfer(&p,&o,sizeof o,0);}
    for(uint32_t i=0;i<w->count;i++){Tr2Creature c=w->creatures[i];c.lot.nodes=NULL;transfer(&p,&c,sizeof c,0);}
    for(uint32_t i=0;i<w->count;i++){*p++=w->creatures[i].lot.nodes!=NULL;size_t bytes=l->boxes.count*sizeof(Tr2LotNode);if(w->creatures[i].lot.nodes)memcpy(p,w->creatures[i].lot.nodes,bytes);p+=bytes;}
    transfer(&p,w->fragments,sizeof(Tr2Fragment)*TR2_MAX_FRAGMENTS,0);
    for(uint32_t i=0;i<l->room_count;i++)transfer(&p,(void *)l->rooms[i].sectors.data,l->rooms[i].sectors.count*8,0);
    transfer(&p,(void *)l->boxes.data,l->boxes.count*8,0);
    memcpy(p,totals,sizeof *totals);
    memcpy(p+sizeof *totals,&l->custom,sizeof l->custom);
    Header h={0x32534254,SAVE_VERSION,(uint32_t)n,hash(data,n),identity(l),w->count,l->room_count,l->boxes.count,(unsigned)index};
    char tmp[MAX_PATH];char directory[MAX_PATH];snprintf(directory,sizeof directory,"%s",path);
    char *slash=strrchr(directory,'/'),*backslash=strrchr(directory,'\\');
    if(backslash && (!slash || backslash>slash))slash=backslash;
    if(slash)*slash=0;else strcpy(directory,".");
    if(!GetTempFileNameA(directory,"tr2",0,tmp)){free(data);return 0;}
    FILE *f=fopen(tmp,"wb");int ok=0;if(f){ok=fwrite(&h,sizeof h,1,f)==1 && fwrite(data,n,1,f)==1;if(fclose(f))ok=0;}
    free(data);if(ok && MoveFileExA(tmp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return 1;
    DeleteFileA(tmp);return 0;
}
int tr2_load_save(const char *path,Tr2Level *l,Tr2World *w,Tr2Actor *a,Tr2Water *water,Tr2CampaignTotals *totals){
    FILE *f=fopen(path,"rb");if(!f)return 0;Header h;size_t n=length(l,w);
    if(fread(&h,sizeof h,1,f)!=1 || h.magic!=0x32534254 || (h.version<1 || h.version>SAVE_VERSION) || h.size!=n+(h.version>=2?sizeof *totals:0)+(h.version>=5?sizeof l->custom:0) || h.level!=identity(l) || h.objects!=w->count || h.rooms!=l->room_count || h.boxes!=l->boxes.count){fclose(f);return 0;}
    unsigned char *data=malloc(h.size),*p=data;if(!data){fclose(f);return 0;}
    int ok=fread(data,h.size,1,f)==1 && fgetc(f)==EOF && hash(data,h.size)==h.checksum;fclose(f);if(!ok){free(data);return 0;}
    State s;transfer(&p,&s,sizeof s,1);
    if(s.world.count!=w->count || (s.world.flip_status!=0 && s.world.flip_status!=1) || s.world.vehicle< -1 || (s.world.vehicle>=0 && ((uint32_t)s.world.vehicle>=w->count || (w->objects[s.world.vehicle].id!=14 && w->objects[s.world.vehicle].id!=13 && w->objects[s.world.vehicle].id!=51))) || s.actor.room<0 || (uint32_t)s.actor.room>=l->room_count || (uint32_t)s.actor.control.animation>=l->animations.count){free(data);return 0;}
    if(s.world.vehicle>=0){Tr2Object mounted;memcpy(&mounted,p+s.world.vehicle*sizeof mounted,sizeof mounted);if(mounted.id!=13 && mounted.id!=14){free(data);return 0;}}
    Tr2LotNode **nodes=calloc(w->count,sizeof *nodes);if(!nodes){free(data);return 0;}
    unsigned char *lot=p+w->count*(sizeof(Tr2Object)+sizeof(Tr2Creature));
    for(uint32_t i=0;i<w->count;i++){size_t bytes=l->boxes.count*sizeof(Tr2LotNode);if(*lot){nodes[i]=malloc(bytes);if(!nodes[i]){for(uint32_t j=0;j<i;j++)free(nodes[j]);free(nodes);free(data);return 0;}memcpy(nodes[i],lot+1,bytes);}lot+=1+bytes;}
    /* All validation and allocation finishes before changing the live world. */
    if(s.world.flip_status!=w->flip_status)tr2_flip_rooms(l);
    for(uint32_t i=0;i<w->count;i++){Tr2Object o;transfer(&p,&o,sizeof o,1);for(int j=0;j<4;j++)o.door[j].sector=w->objects[i].door[j].sector;/* Earlier builds stored these unsupported enemies with zero health. */
        if(((h.version<3 && tr2_tibetan_enemy(o.id)) || (h.version<4 && tr2_xian_enemy(o.id))) && o.pose.control.health==0){
            o.pose.control.health=w->objects[i].pose.control.health;
            o.status=o.active?1:w->objects[i].status;
            if(o.id==41 || o.id==43){o.pose.control=w->objects[i].pose.control;o.mesh_mask=w->objects[i].mesh_mask;}
        }
        if(h.version<4 && o.id==40){o.pose.x=w->objects[i].pose.x;o.pose.z=w->objects[i].pose.z;}
        w->objects[i]=o;}
    for(uint32_t i=0;i<w->count;i++){free(w->creatures[i].lot.nodes);transfer(&p,w->creatures+i,sizeof(Tr2Creature),1);w->creatures[i].lot.nodes=nodes[i];}free(nodes);p=lot;
    transfer(&p,w->fragments,sizeof(Tr2Fragment)*TR2_MAX_FRAGMENTS,1);
    for(uint32_t i=0;i<l->room_count;i++)transfer(&p,(void *)l->rooms[i].sectors.data,l->rooms[i].sectors.count*8,1);
    transfer(&p,(void *)l->boxes.data,l->boxes.count*8,1);
    s.world.lara=a;s.world.objects=w->objects;s.world.level=l;s.world.fragments=w->fragments;s.world.creatures=w->creatures;s.world.inventory=w->inventory;s.world.hazards=w->hazards;s.world.combat=w->combat;s.world.start_inventory=w->start_inventory;s.world.start_combat=w->start_combat;
    *w=s.world;*w->inventory=s.inventory;*w->hazards=s.hazards;*w->combat=s.combat;*w->start_inventory=s.start_inventory;*w->start_combat=s.start_combat;*a=s.actor;*water=s.water;l->world=w;
    /* Normalize terminal corpses after restoring state, while preserving
       enemies whose death animations have not yet finished. */
    for(uint32_t i=0;i<w->count;i++){Tr2Object *o=w->objects+i;
        if(o->status==2 && o->pose.control.health<=0 && ((o->id>=15 && o->id<=18) || o->id==20 || o->id==21 || o->id==30 || o->id==31 || o->id==34 || (o->id==32 || o->id==33) || o->id==36 || o->id==38 || tr2_xian_enemy(o->id) || tr2_tibetan_enemy(o->id) || o->id==47 || o->id==49 || o->id==50 || o->id==51 || o->id==39 || o->id==214))tr2_enemy_finish(w,(int)i);}
    if((uint32_t)w->combat->target<w->count){Tr2Object *target=w->objects+w->combat->target;
        if((target->flags&0x8000) || (target->status==3 && target->pose.control.health<=0))w->combat->target=-1;
    }
    memset(totals,0,sizeof *totals);if(h.version>=2)memcpy(totals,p,sizeof *totals);
    if(h.version>=5)memcpy(&l->custom,p+sizeof *totals,sizeof l->custom);
    if(h.version<5 || (h.version==5 && !(l->custom.options&TR2_CONSISTENT_WEAPONS))){
        int gun=w->combat->weapon==8?w->combat->last_weapon:w->combat->weapon;
        l->custom.sidearm=w->inventory->count[135]?1:0;
        if((gun==2 && w->inventory->count[137]) || (gun==3 && w->inventory->count[138]))l->custom.sidearm=gun;
    }
    free(data);return 1;
}








int tr2_slot_level(const char *path){
    FILE *f=fopen(path,"rb");if(!f)return -1;Header h;
    int ok=fread(&h,sizeof h,1,f)==1 && h.magic==0x32534254 && (h.version>=1 && h.version<=SAVE_VERSION) && (h.index>=1 || h.version>=5) && h.index<256;
    if(ok){ok=!fseek(f,0,SEEK_END) && ftell(f)==(long)(sizeof h+h.size);}
    fclose(f);return ok?(int)h.index:-1;
}
