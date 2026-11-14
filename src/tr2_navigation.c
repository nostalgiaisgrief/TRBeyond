#include "tr2_world.h"
#include <stdlib.h>

static int random15(uint32_t *seed){*seed=*seed*0x41c64e6d+12345;return (*seed>>10)&32767;}
static int zone(const Tr2Level *l,const Tr2Lot *lot,int box){int index=lot->fly?4:(lot->step>>8)-1;if(index<0 || index>4)index=0;const Tr2World *w=l->world;if(w && w->flip_status)index+=5;return tr2_i16(l->zones.data+((unsigned)index*l->boxes.count+box)*2);}
static int min(int a,int b){return a<b?a:b;}
static int max(int a,int b){return a>b?a:b;}
static int clamp(int a,int low,int high){return a<low?low:a>high?high:a;}
/* SearchLOT/UpdateLOT, 0x40e470/0x40e670. Nodes and queue survive ticks;
   changed destinations prepend a search rather than discarding older work. */
int tr2_lot_update(const Tr2Level *l,Tr2Lot *lot,int budget){
    if(lot->required_box!=-1 && lot->required_box!=lot->target_box){
        int id=lot->target_box=lot->required_box;Tr2LotNode *n=lot->nodes+id;
        if(n->next==-1 && lot->tail!=id){n->next=lot->head;if(lot->head==-1)lot->tail=(int16_t)id;lot->head=(int16_t)id;}
        n->search=++lot->search;n->exit=-1;
    }
    int z=lot->head>=0?zone(l,lot,lot->head):-1;
    for(int i=0;i<budget;i++){
        int head=lot->head;if(head==-1){lot->tail=-1;return 0;}
        Tr2LotNode *current=lot->nodes+head;const uint8_t *b=l->boxes.data+head*8;
        unsigned at=tr2_u16(b+6)&0x3fff;
        for(;at<l->overlaps.count;at++){
            int link=tr2_u16(l->overlaps.data+at*2),id=link&0x7fff;
            if((uint32_t)id<l->boxes.count && zone(l,lot,id)==z){
                const uint8_t *other=l->boxes.data+id*8;Tr2LotNode *n=lot->nodes+id;
                int delta=tr2_i16(other+4)-tr2_i16(b+4);
                unsigned search=current->search,old=n->search;
                if(delta<=lot->step && delta>=lot->drop && (search&0x7fff)>=(old&0x7fff)){
                    int visit=0;
                    if(search&0x8000){if((search^old)&0x7fff){n->search=(uint16_t)search;visit=1;}}
                    else if(((search^old)&0x7fff) || (old&0x8000)){
                        n->search=(uint16_t)search;
                        if(tr2_u16(other+6)&lot->block)n->search|=0x8000;
                        else n->exit=(int16_t)head;
                        visit=1;
                    }
                    if(visit && n->next==-1 && id!=lot->tail){lot->nodes[lot->tail].next=(int16_t)id;lot->tail=(int16_t)id;}
                }
            }
            if(link&0x8000)break;
        }
        lot->head=current->next;current->next=-1;
    }
    return 1;
}
/* CalculateTarget, 0x40ee50: clip the whole exit chain, stopping at its
   first obstructed turn. The original uses inclusive box maxima. */
int tr2_creature_target(const Tr2Level *l,Tr2Lot *lot,const Tr2Actor *a,int box,int t[3],uint32_t *random){
    tr2_lot_update(l,lot,5);t[0]=a->x;t[1]=a->y;t[2]=a->z;
    int flags=15,left=0,right=0,top=0,bottom=0;
    if((uint32_t)box>=l->boxes.count)return 0;
    for(uint32_t safety=0;safety<l->boxes.count;safety++){
        const uint8_t *b=l->boxes.data+box*8;
        int zmin=b[0]*1024,zmax=b[1]*1024-1,xmin=b[2]*1024,xmax=b[3]*1024-1;
        t[1]=min(t[1],tr2_i16(b+4)-(lot->fly?1024:0));
        if(a->z>=zmin && a->z<=zmax && a->x>=xmin && a->x<=xmax){left=zmin;right=zmax;top=xmin;bottom=xmax;}
        else{
            if(a->z<zmin){
                if((flags&1) && a->x>=xmin && a->x<=xmax){t[2]=max(t[2],zmin+512);if(flags&16)return 2;top=max(top,xmin);bottom=min(bottom,xmax);flags=1;}
                else if(flags!=1){t[2]=right-512;if(flags!=15)return 2;flags=31;}
            }else if(a->z>zmax){
                if((flags&2) && a->x>=xmin && a->x<=xmax){t[2]=min(t[2],zmax-512);if(flags&16)return 2;top=max(top,xmin);bottom=min(bottom,xmax);flags=2;}
                else if(flags!=2){t[2]=left+512;if(flags!=15)return 2;flags=31;}
            }
            if(a->x<xmin){
                if((flags&4) && a->z>=zmin && a->z<=zmax){t[0]=max(t[0],xmin+512);if(flags&16)return 2;left=max(left,zmin);right=min(right,zmax);flags=4;}
                else if(flags!=4){t[0]=bottom-512;if(flags!=15)return 2;flags=31;}
            }else if(a->x>xmax){
                if((flags&8) && a->z>=zmin && a->z<=zmax){t[0]=min(t[0],xmax-512);if(flags&16)return 2;left=max(left,zmin);right=min(right,zmax);flags=8;}
                else if(flags!=8){t[0]=top+512;if(flags!=15)return 2;flags=31;}
            }
        }
        if(box==lot->target_box){
            if(flags&3)t[2]=lot->target[2];else if(!(flags&16))t[2]=clamp(t[2],zmin+512,zmax-512);
            if(flags&12)t[0]=lot->target[0];else if(!(flags&16))t[0]=clamp(t[0],xmin+512,xmax-512);
            t[1]=lot->target[1];return 1;
        }
        int next=lot->nodes[box].exit;
        if(next<0 || (tr2_u16(l->boxes.data+next*8+6)&lot->block)){
            if(flags&3)t[2]=zmin+512+tr2_asr(random15(random)*(zmax-zmin-1024),15);
            else if(!(flags&16))t[2]=clamp(t[2],zmin+512,zmax-512);
            if(flags&12)t[0]=xmin+512+tr2_asr(random15(random)*(xmax-xmin-1024),15);
            else if(!(flags&16))t[0]=clamp(t[0],xmin+512,xmax-512);
            t[1]=tr2_i16(b+4)-(lot->fly?384:0);return 0;
        }
        box=next;
    }
    return 0;
}
static int valid_box(const Tr2Level *l,const Tr2Lot *lot,const Tr2Actor *a,int z,int box){
    if((uint32_t)box>=l->boxes.count || zone(l,lot,box)!=z)return 0;
    const uint8_t *b=l->boxes.data+box*8;
    return !(tr2_u16(b+6)&lot->block) && !(a->z>b[0]*1024 && a->z<b[1]*1024 && a->x>b[2]*1024 && a->x<b[3]*1024);
}
static void target_box(const Tr2Level *l,Tr2Lot *lot,int box,uint32_t *random){
    const uint8_t *b=l->boxes.data+box*8;
    lot->target[2]=b[0]*1024+512+random15(random)*((b[1]-b[0]-1)>>5);
    lot->target[0]=b[2]*1024+512+random15(random)*((b[3]-b[2]-1)>>5);
    lot->target[1]=tr2_i16(b+4)-(lot->fly?384:0);lot->required_box=(int16_t)box;
}
static int stalk_box(const Tr2Level *l,const Tr2Actor *a,const Tr2Actor *enemy,int box){
    const uint8_t *b=l->boxes.data+box*8;
    int dz=(b[0]+b[1])*512-enemy->z,dx=(b[2]+b[3])*512-enemy->x;
    if(dx>(b[3]-b[2]+3)*1024 || dx<-(b[3]-b[2]+3)*1024 || dz>(b[1]-b[0]+3)*1024 || dz<-(b[1]-b[0]+3)*1024)return 0;
    int facing=tr2_asr(enemy->yaw,14)+2,q=dz>0?(dx>0?2:1):(dx<=0?0:3);
    if(facing==q)return 0;
    int aq=a->z>enemy->z?(a->x>enemy->x?2:1):(a->x<=enemy->x?0:3);
    return facing!=aq || abs(facing-q)!=2;
}
static int escape_box(const Tr2Level *l,const Tr2Actor *a,const Tr2Actor *enemy,int box){
    const uint8_t *b=l->boxes.data+box*8;int dx=(b[2]+b[3])*512-enemy->x,dz=(b[0]+b[1])*512-enemy->z;
    if(dx> -5120 && dx<5120 && dz> -5120 && dz<5120)return 0;
    return (a->z>enemy->z)==(dz>0) || (a->x>enemy->x)==(dx>0);
}
/* CreatureMood 0x40e9e0, ground creatures targeting Lara. */
void tr2_enemy_target(const Tr2Level *l,Tr2Creature *c,const Tr2Actor *a,const Tr2Actor *enemy,int box,int enemy_box,int violent,int distance,uint32_t *random){
    Tr2Lot *lot=&c->lot;if((uint32_t)box>=l->boxes.count)return;
    int z=zone(l,lot,box),ez=(uint32_t)enemy_box<l->boxes.count?zone(l,lot,enemy_box):-1;
    if((uint32_t)enemy_box<l->boxes.count && ((tr2_u16(l->boxes.data+enemy_box*8+6)&lot->block) || lot->nodes[box].search==(lot->search|0x8000)))ez|=0x4000;
    if(lot->nodes[box].search==(lot->search|0x8000))lot->required_box=-1;
    if(c->mood!=1 && lot->required_box!=-1 && !valid_box(l,lot,a,z,lot->target_box)){if(z==ez)c->mood=0;lot->required_box=-1;}
    int old=c->mood;
    if(enemy->control.health<=0)c->mood=0;
    else if(violent){
        if(c->mood==1){if(z!=ez)c->mood=0;}
        else if(c->mood==0 || c->mood==3){if(z==ez)c->mood=1;else if(a->control.flags&16)c->mood=2;}
        else if(c->mood==2 && z==ez)c->mood=1;
    }else if(c->mood==1){
        if((a->control.flags&16) && (random15(random)<2048 || z!=ez))c->mood=2;
        else if(z!=ez)c->mood=0;
    }else if(c->mood==0 || c->mood==3){
        if((a->control.flags&16) && (random15(random)<2048 || z!=ez))c->mood=2;
        else if(z==ez)c->mood=(distance<9437184 || (c->mood==3 && lot->required_box==-1))?1:3;
    }else if(c->mood==2 && z==ez && random15(random)<256)c->mood=3;
    if(old!=c->mood){if(old==1 && lot->target_box>=0)target_box(l,lot,lot->target_box,random);lot->required_box=-1;}
    if(c->mood==1){lot->target[0]=enemy->x;lot->target[1]=enemy->y;lot->target[2]=enemy->z;lot->required_box=(int16_t)enemy_box;
        if(lot->fly && enemy->control.water_status==0){int b[6];if(tr2_best_bounds(l,enemy,b))lot->target[1]+=b[2];}}
    else if(c->mood==0 || c->mood==3){
        int candidate=-1;
        if(c->mood==0 || lot->required_box==-1 || !stalk_box(l,a,enemy,lot->required_box)){
            int pick=tr2_asr(random15(random)*lot->zone_count,15);candidate=lot->nodes[pick].box;
            if(valid_box(l,lot,a,z,candidate)){
                if(stalk_box(l,a,enemy,candidate) && enemy->control.health>0){target_box(l,lot,candidate,random);c->mood=3;}
                else if(lot->required_box==-1){target_box(l,lot,candidate,random);if(c->mood==3 && z!=ez)c->mood=0;}
            }
        }
    }
    else if(c->mood==2 && lot->zone_count>0){
        int candidate=lot->nodes[tr2_asr(random15(random)*lot->zone_count,15)].box;
        if(valid_box(l,lot,a,z,candidate) && lot->required_box==-1){
            if(escape_box(l,a,enemy,candidate))target_box(l,lot,candidate,random);
            else if(z==ez && stalk_box(l,a,enemy,candidate)){target_box(l,lot,candidate,random);c->mood=3;}
        }
    }
    if(lot->target_box==-1)target_box(l,lot,box,random);
    tr2_creature_target(l,lot,a,box,c->target,random);
}
void tr2_winston_target(const Tr2Level *l,Tr2Creature *c,const Tr2Actor *a,const Tr2Actor *enemy,int box,int enemy_box,uint32_t *random){
    tr2_enemy_target(l,c,a,enemy,box,enemy_box,1,0,random);
}
static int sector_box(const Tr2Level *l,int x,int y,int z,int room,int *new_room){
    Tr2Sector s;if(!tr2_find_sector(l,x,y,z,room,&s))return -1;
    if(new_room)*new_room=s.room;int box=tr2_u16(l->rooms[s.room].sectors.data+s.index*8+2);
    return (uint32_t)box<l->boxes.count?box:-1;
}
static int bad_floor(const Tr2Level *l,const Tr2Lot *lot,int x,int y,int z,int height,int next_height,int room){
    int box=sector_box(l,x,y,z,room,NULL);if(box<0)return 1;
    const uint8_t *b=l->boxes.data+box*8;int h=tr2_i16(b+4),delta=height-h;
    return (tr2_u16(b+6)&lot->block) || delta>lot->step || delta<lot->drop || (delta< -lot->step && h>next_height) || (lot->fly && y>h+lot->fly);
}
/* CreatureBiff 0x40f2b0 stops at self in the original room item list. */
int tr2_creature_biff(const Tr2World *w,int index,int radius){
    if((uint32_t)index>=w->count)return 0;const Tr2Object *o=w->objects+index;int room=o->pose.room;if(room<0 || room>=1024)return 0;
    int next=w->room_head[room];for(uint32_t n=0;n<w->count && next>=0 && (uint32_t)next<w->count && next!=index;n++){
        const Tr2Object *p=w->objects+next;next=p->next_room;if(p->id==0 || p->status!=1 || !p->pose.speed)continue;
        int64_t dx=p->pose.x-o->pose.x,dy=p->pose.y-o->pose.y,dz=p->pose.z-o->pose.z;
        if(tr2_long(dx*dx+dy*dy+dz*dz)<radius*radius)return 1;
    }return 0;
}
/* CreatureAnimation 0x40f500 and BadFloor 0x40f3b0. */
void tr2_creature_move(const Tr2Level *l,Tr2Object *o,Tr2Lot *lot,int box,int turn,int radius,int target_y){
    Tr2Actor *a=&o->pose;int ox=a->x,oy=a->y,oz=a->z;
    tr2_object_animate(l,o);if(o->status==2){o->active=0;return;}if(box<0){a->x=ox;a->y=oy;a->z=oz;return;}
    int bounds[6]={0};tr2_actor_bounds(l,a,bounds);int y=a->y+bounds[2],room=a->room;
    sector_box(l,ox,y,oz,room,&room);
    int dest=sector_box(l,a->x,y,a->z,room,&room);
    int height=dest<0?0:tr2_i16(l->boxes.data+dest*8+4);
    int old_height=tr2_i16(l->boxes.data+box*8+4);
    if(dest<0 || zone(l,lot,box)!=zone(l,lot,dest) || old_height-height>lot->step || old_height-height<lot->drop){
        int tile=tr2_asr(a->x,10);
        if(tile<tr2_asr(ox,10))a->x=ox&~1023;else if(tile>tr2_asr(ox,10))a->x=ox|1023;
        /* The supplied executable compares the X tile here as well. */
        if(tile<tr2_asr(oz,10))a->z=oz&~1023;else if(tile>tr2_asr(oz,10))a->z=oz|1023;
        dest=sector_box(l,a->x,y,a->z,room,&room);
        if(dest<0){a->x=ox;a->y=oy;a->z=oz;return;}
        height=tr2_i16(l->boxes.data+dest*8+4);
    }
    int next=lot->nodes[dest].exit,next_height=next<0?height:tr2_i16(l->boxes.data+next*8+4);
    int x=a->x,z=a->z,xpos=x&1023,zpos=z&1023,sx=0,sz=0;
    if(zpos<radius){
        if(bad_floor(l,lot,x,y,z-radius,height,next_height,room))sz=radius-zpos;
        if(xpos<radius){
            if(bad_floor(l,lot,x-radius,y,z,height,next_height,room))sx=radius-xpos;
            else if(!sz && bad_floor(l,lot,x-radius,y,z-radius,height,next_height,room)){
                if(a->yaw> -24576 && a->yaw<8192)sz=radius-zpos;else sx=radius-xpos;
            }
        }else if(xpos>1024-radius){
            if(bad_floor(l,lot,x+radius,y,z,height,next_height,room))sx=1024-xpos-radius;
            else if(!sz && bad_floor(l,lot,x+radius,y,z-radius,height,next_height,room)){
                if(a->yaw> -8192 && a->yaw<24576)sz=radius-zpos;else sx=1024-xpos-radius;
            }
        }
    }else if(zpos>1024-radius){
        if(bad_floor(l,lot,x,y,z+radius,height,next_height,room))sz=1024-zpos-radius;
        if(xpos<radius){
            if(bad_floor(l,lot,x-radius,y,z,height,next_height,room))sx=radius-xpos;
            else if(!sz && bad_floor(l,lot,x-radius,y,z+radius,height,next_height,room)){
                if(a->yaw> -8192 && a->yaw<24576)sx=radius-xpos;else sz=1024-zpos-radius;
            }
        }else if(xpos>1024-radius){
            if(bad_floor(l,lot,x+radius,y,z,height,next_height,room))sx=1024-xpos-radius;
            else if(!sz && bad_floor(l,lot,x+radius,y,z+radius,height,next_height,room)){
                if(a->yaw> -24576 && a->yaw<8192)sx=1024-xpos-radius;else sz=1024-zpos-radius;
            }
        }
    }else if(xpos<radius){if(bad_floor(l,lot,x-radius,y,z,height,next_height,room))sx=radius-xpos;}
    else if(xpos>1024-radius){if(bad_floor(l,lot,x+radius,y,z,height,next_height,room))sx=1024-xpos-radius;}
    a->x+=sx;a->z+=sz;if(sx || sz)a->yaw=tr2_word(a->yaw+turn);
    Tr2World *w=l->world;int index=-1;
    if(w && (uintptr_t)o>=(uintptr_t)w->objects && (uintptr_t)o<(uintptr_t)(w->objects+w->count))index=(int)(o-w->objects);
    if(index>=0 && tr2_creature_biff(w,index,radius)){a->x=ox;a->y=oy;a->z=oz;return;}
    Tr2Height h;Tr2Sector ref;
    if(lot->fly){
        int dy=clamp(target_y-a->y,-lot->fly,lot->fly);
        if(tr2_query(l,a->x,y,a->z,room,&h,&ref)){
            if(a->y+dy>h.floor){
                if(a->y>h.floor){a->x=ox;a->z=oz;dy=-lot->fly;}
                else{dy=0;a->y=h.floor;}
            }else if(a->y+bounds[2]+dy<h.ceiling){
                if(a->y+bounds[2]<h.ceiling){a->x=ox;a->z=oz;dy=lot->fly;}else dy=0;
            }
            a->y+=dy;room=ref.room;
        }
        int pitch=a->speed?tr2_word(tr2_atan(a->speed,-dy)):0;pitch=clamp(pitch,-3640,3640);
        a->pitch=tr2_word(clamp(pitch,a->pitch-182,a->pitch+182));
    }else if(tr2_query(l,a->x,a->y,a->z,room,&h,&ref)){
        if(a->y>h.floor)a->y=h.floor;else if(h.floor-a->y>64)a->y+=64;else if(a->y<h.floor)a->y=h.floor;
        room=ref.room;
    }
    if(!lot->fly)a->pitch=0;if(o->id!=25 && o->id!=28 && o->id!=29)sector_box(l,a->x,a->y-512,a->z,room,&room);a->room=(int16_t)room;
    if(index>=0)tr2_world_room(w,index,room);
    if(o->id!=25 && o->id!=28 && o->id!=29 && (uint32_t)room<l->room_count && (l->rooms[room].flags&1))a->control.health=0;
}
void tr2_ground_creature_move(const Tr2Level *l,Tr2Object *o,Tr2Lot *lot,int box,int turn,int radius){tr2_creature_move(l,o,lot,box,turn,radius,lot->target[1]);}
void tr2_winston_move(const Tr2Level *l,Tr2Object *o,Tr2Lot *lot,int box,int turn){
    tr2_ground_creature_move(l,o,lot,box,turn,102);
}



