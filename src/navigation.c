/* DOS LOT search/target selection 0x118f0..0x1275b. */
#include "navigation.h"
#include "pistols.h"
#include "fixed.h"
#include "hazards.h"
#include "object_contact.h"
#include <stdlib.h>
#include <string.h>
static int min(int a,int b){return a<b?a:b;}
static int max(int a,int b){return a>b?a:b;}
static int clamp(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
static int valid(const TombLevel *l,int b){return b>=0 && (size_t)b<l->box_count;}
static const int16_t *zone(const TombNavigation *n,const TombLevel *l){return l->zones+l->box_count*(n->fly?2:n->step==256?0:1);}
int tomb_navigation_box(const TombLevel *l,const TombActor *a) {
    if(a->room<0 || (size_t)a->room>=l->room_count)return -1;
    const TombRoom *r=l->rooms+a->room;int x=tomb_asr(a->x-r->x,10),z=tomb_asr(a->z-r->z,10);
    if(x<0 || z<0 || x>=r->nx || z>=r->nz)return -1;
    int b=r->sectors[x*r->nz+z].box;return valid(l,b)?b:-1;
}
void tomb_navigation_free(TombNavigation *n){free(n->nodes);memset(n,0,sizeof *n);}
int tomb_navigation_init(TombNavigation *n,const TombLevel *l,const TombActor *a,int object) {
    /* The DOS random zone selector can reach zone_count when RNG=32767.
       Keep its zero-initialized spare entry without an out-of-bounds read. */
    memset(n,0,sizeof *n);n->nodes=calloc(l->box_count+1,sizeof *n->nodes);if(!n->nodes)return 0;
    n->head=n->tail=n->target_box=n->required_box=-1;n->block=0x4000;n->step=256;n->drop=object==7 || object==12 || object==13 || object==14?-1024:-256;
    if(object==9 || object==11){n->step=20480;n->drop=-20480;n->fly=16;}
    if(object==15){n->step=512;n->drop=-1024;}
    if(object==18 || object==20 || object==23)n->block=0x8000; /* DOS 2cfd3. */
    int box=tomb_navigation_box(l,a);if(box<0){tomb_navigation_free(n);return 0;}
    const int16_t *z=zone(n,l),*alt=z+3*l->box_count;
    for(size_t i=0;i<l->box_count;i++){n->nodes[i].exit=n->nodes[i].next=-1;if(z[i]==z[box] || alt[i]==alt[box])n->nodes[n->zone_count++].zone_box=(int16_t)i;}
    return 1;
}
int tomb_navigation_search(TombNavigation *n,const TombLevel *l,int count) {
    const int16_t *z=zone(n,l);if(!valid(l,n->head))return 0;int target_zone=z[n->head];
    while(count-->0) {
        if(!valid(l,n->head))return 0;
        int head=n->head;TombNavNode *cur=n->nodes+head;const TombCameraBox *b=l->boxes+head;
        size_t at=b->overlap&0x3fff;
        for(;;) {
            if(at>=l->overlap_count)return 0;
            unsigned word=l->overlaps[at++];int id=word&0x7fff;
            if(valid(l,id) && z[id]==target_zone) {
                const TombCameraBox *nb=l->boxes+id;int dy=nb->height-b->height;
                TombNavNode *next=n->nodes+id;
                if(dy<=n->step && dy>=n->drop && (cur->search&0x7fff)>=(next->search&0x7fff)) {
                    int visit=0;
                    if(cur->search&0x8000){if((cur->search&0x7fff)!=(next->search&0x7fff)){next->search=cur->search;visit=1;}}
                    else if((cur->search&0x7fff)!=(next->search&0x7fff) || (next->search&0x8000)) {
                        next->search=cur->search;
                        if(nb->overlap&n->block)next->search|=0x8000;else next->exit=n->head;
                        visit=1;
                    }
                    if(visit && next->next==-1 && id!=n->tail){if(!valid(l,n->tail))return 0;n->nodes[n->tail].next=(int16_t)id;n->tail=(int16_t)id;}
                }
            }
            if(word&0x8000)break;
        }
        n->head=cur->next;cur->next=-1;
    }
    return 1;
}
int tomb_navigation_update(TombNavigation *n,const TombLevel *l,int count) {
    if(n->required_box!=-1 && n->required_box!=n->target_box) {
        if(!valid(l,n->required_box))return 0;n->target_box=n->required_box;TombNavNode *node=n->nodes+n->target_box;
        if(node->next==-1 && n->tail!=n->target_box){node->next=n->head;if(n->head==-1)n->tail=n->target_box;n->head=n->target_box;}
        ++n->search;node->exit=-1;node->search=n->search;
    }
    return tomb_navigation_search(n,l,count);
}
static int random_axis(int lo,int hi,uint32_t *r){return lo+512+tomb_asr(tomb_long((int64_t)tomb_control_random(r)*(hi-lo-1024)),15);}
void tomb_navigation_target_box(TombNavigation *n,const TombLevel *l,int box,uint32_t *r) {
    box&=0x7fff;if(!valid(l,box))return;const TombCameraBox *b=l->boxes+box;
    n->z=random_axis(b->zmin,b->zmax,r);n->x=random_axis(b->xmin,b->xmax,r);n->y=b->height-(n->fly?384:0);n->required_box=(int16_t)box;
}
int tomb_navigation_target(TombNavigation *n,const TombLevel *l,const TombActor *a,int box,uint32_t *random,int32_t out[3]) {
    tomb_navigation_update(n,l,5);out[0]=a->x;out[1]=a->y;out[2]=a->z;if(!valid(l,box))return 0;
    int mask=15,lo[2]={0,0},hi[2]={0,0};const TombCameraBox *b=NULL;
    for(size_t budget=0;budget<l->box_count;budget++) {
        b=l->boxes+box;int plane=b->height-(n->fly?1024:0);out[1]=min(out[1],plane);
        int lower[2]={b->zmin,b->xmin},upper[2]={b->zmax,b->xmax},pos[2]={a->z,a->x};
        if(pos[0]>=lower[0] && pos[0]<=upper[0] && pos[1]>=lower[1] && pos[1]<=upper[1]) {
            memcpy(lo,lower,sizeof lo);memcpy(hi,upper,sizeof hi);
        } else for(int axis=0;axis<2;axis++) {
            int other=1-axis,index=axis?0:2;
            for(int side=0;side<2;side++) {
                if(!(side?pos[axis]>upper[axis]:pos[axis]<lower[axis]))continue;
                int bit=1<<(axis*2+side);
                if((mask&bit) && pos[other]>=lower[other] && pos[other]<=upper[other]) {
                    out[index]=side?min(out[index],upper[axis]-512):max(out[index],lower[axis]+512);
                    if(mask&16)return 2;
                    lo[other]=max(lo[other],lower[other]);hi[other]=min(hi[other],upper[other]);mask=bit;
                } else if(mask!=bit) {
                    out[index]=side?lo[axis]+512:hi[axis]-512;
                    if(mask!=15)return 2;mask|=16;
                }
            }
        }
        if(box==n->target_box) {
            for(int axis=0;axis<2;axis++){int index=axis?0:2;if(mask&(3<<(axis*2)))out[index]=axis?n->x:n->z;else if(!(mask&16))out[index]=clamp(out[index],lower[axis]+512,upper[axis]-512);}
            out[1]=n->y;return 1;
        }
        int next=n->nodes[box].exit;
        if(!valid(l,next) || (l->boxes[next].overlap&n->block))break;
        box=next;
    }
    for(int axis=0;axis<2;axis++) {
        int index=axis?0:2,lower=axis?b->xmin:b->zmin,upper=axis?b->xmax:b->zmax;
        if(mask&(3<<(axis*2)))out[index]=random_axis(lower,upper,random);
        else if(!(mask&16))out[index]=clamp(out[index],lower+512,upper-512);
    }
    out[1]=b->height-(n->fly?384:0);return 0;
}
void tomb_creature_info(TombCreatureInfo *i,const TombCreature *c,const TombNavigation *n,const TombLevel *l,const TombActor *a,int object,const TombActor *lara,const int16_t *sine) {
    int box=tomb_navigation_box(l,a),enemy=tomb_navigation_box(l,lara);const int16_t *z=zone(n,l);
    memset(i,0,sizeof *i);i->zone=valid(l,box)?z[box]:-1;i->enemy_zone=valid(l,enemy)?z[enemy]:-2;
    if((valid(l,enemy) && (l->boxes[enemy].overlap&n->block)) || (valid(l,box) && n->nodes[box].search==(uint16_t)(n->search|0x8000)))i->enemy_zone|=0x4000;
    int pivot=object==16 || object==17?200:object==7?375:object==8?500:object==18?2000:object==23 || object==19 || object==12 || object==13 || object==14?400:object==10 || object==11?600:object==15?250:object>=20 && object<=22?150:0;
    int32_t dx=lara->x-a->x-tomb_asr(pivot*tomb_hazard_sine(sine,a->yaw),14),dz=lara->z-a->z-tomb_asr(pivot*tomb_hazard_sine(sine,a->yaw+16384),14);
    int angle=tomb_object_angle(dz,dx);i->distance=tomb_long((int64_t)dx*dx+(int64_t)dz*dz);
    i->angle=tomb_word(angle-a->yaw);i->enemy_facing=tomb_word(angle-32768-lara->yaw);
    i->ahead=i->angle> -16384 && i->angle<16384;i->bite=i->ahead && lara->y>a->y-256 && lara->y<a->y+256;(void)c;
}
static int valid_box(const TombNavigation *n,const TombLevel *l,const TombActor *a,int zone_id,int box) {
    if(!valid(l,box) || zone(n,l)[box]!=zone_id)return 0;const TombCameraBox *b=l->boxes+box;
    return !(b->overlap&n->block) && !(a->z>b->zmin && a->z<b->zmax && a->x>b->xmin && a->x<b->xmax);
}
static int quadrant(int x,int z){return z>0?1+(x>0):x>0?3:0;}
static int stalk_box(const TombLevel *l,const TombActor *a,const TombActor *lara,int box) {
    if(!valid(l,box))return 0;const TombCameraBox *b=l->boxes+box;
    int dx=tomb_asr(b->xmin+b->xmax,1)-lara->x,dz=tomb_asr(b->zmin+b->zmax,1)-lara->z;
    if(dx>3072 || dx< -3072 || dz>3072 || dz< -3072)return 0;
    int facing=tomb_asr(lara->yaw,14)+2,side=quadrant(dx,dz);
    if(side==facing)return 0;
    return !(quadrant(a->x-lara->x,a->z-lara->z)==facing && abs(facing-side)==2);
}
static int escape_box(const TombLevel *l,const TombActor *a,const TombActor *lara,int box) {
    if(!valid(l,box))return 0;const TombCameraBox *b=l->boxes+box;
    int dx=tomb_asr(b->xmin+b->xmax,1)-lara->x,dz=tomb_asr(b->zmin+b->zmax,1)-lara->z;
    if(dx> -5120 && dx<5120 && dz> -5120 && dz<5120)return 0;
    return (dz>0)==(a->z>lara->z) || (dx>0)==(a->x>lara->x);
}
static int choose_box(const TombNavigation *n,uint32_t *r){return n->zone_count?n->nodes[tomb_control_random(r)*n->zone_count/32767].zone_box:-1;}
void tomb_creature_mood(TombCreature *c,TombNavigation *n,const TombLevel *l,const TombActor *a,int object,const TombCreatureInfo *i,const TombActor *lara,int16_t lara_health,int water,int16_t lara_top,uint32_t *r) {
    int box=tomb_navigation_box(l,a),enemy=tomb_navigation_box(l,lara);
    if(!valid(l,box))return;
    if(n->nodes[box].search==(uint16_t)(n->search|0x8000))n->required_box=-1;
    if(c->mood!=1 && n->required_box!=-1 && !valid_box(n,l,a,i->zone,n->target_box)){if(i->zone==i->enemy_zone)c->mood=0;n->required_box=-1;}
    int old=c->mood,same=i->zone==i->enemy_zone;
    if(lara_health<=0)c->mood=0;
    else if(object==34 || object==32 || object==23 || (object==20 && a->current==13) || object==17 || object==8 || object==18 || object==19 || object==10 || object==11 || object==12 || object==13 || object==14) {
        if(c->mood==1){if(!same)c->mood=0;}
        else if(c->mood==0 || c->mood==3){if(same)c->mood=1;else if(a->flags&16)c->mood=2;}
        else if(c->mood==2 && same)c->mood=1;
    } else switch(c->mood) {
        case 1:if((a->flags&16) && (tomb_control_random(r)<2048 || !same))c->mood=2;else if(!same)c->mood=0;break;
        case 0:case 3:if((a->flags&16) && (tomb_control_random(r)<2048 || !same))c->mood=2;else if(same)c->mood=i->distance<0x900000 || (c->mood==3 && n->required_box==-1)?1:3;break;
        case 2:if(same && tomb_control_random(r)<256)c->mood=3;break;
        default:break;
    }
    if(old!=c->mood){if(old==1)tomb_navigation_target_box(n,l,n->target_box,r);n->required_box=-1;}
    int choice;
    switch(c->mood) {
    case 1:if(tomb_control_random(r)<(unsigned)(object==16 || object==17?8192:object==7?8192:object==18?32767:object==8 || object==19?16384:object==10?8192:object==12?32767:object==13?8192:object==14?8192:object==15?32767:(object>=30 && object<=34) || object==27 || object==28 || object==20 || object==22 || object==23?32767:object==21?8192:1024)) {
        n->x=lara->x;n->y=lara->y;n->z=lara->z;n->required_box=(int16_t)enemy;
        if(n->fly && !water)n->y+=lara_top;
        }break;
    case 0:choice=choose_box(n,r);if(valid_box(n,l,a,i->zone,choice)) {
        if(stalk_box(l,a,lara,choice)){tomb_navigation_target_box(n,l,choice,r);c->mood=3;}
        else if(n->required_box==-1)tomb_navigation_target_box(n,l,choice,r);
        }break;
    case 3:if(n->required_box==-1 || !stalk_box(l,a,lara,n->required_box)) {
        choice=choose_box(n,r);if(valid_box(n,l,a,i->zone,choice)) {
            if(stalk_box(l,a,lara,choice))tomb_navigation_target_box(n,l,choice,r);
            else if(n->required_box==-1){tomb_navigation_target_box(n,l,choice,r);if(!same)c->mood=0;}
        }}break;
    case 2:choice=choose_box(n,r);if(valid_box(n,l,a,i->zone,choice) && n->required_box==-1) {
        if(escape_box(l,a,lara,choice))tomb_navigation_target_box(n,l,choice,r);
        else if(same && stalk_box(l,a,lara,choice)){tomb_navigation_target_box(n,l,choice,r);c->mood=3;}
        }break;
    default:break;
    }
    if(n->target_box==-1)tomb_navigation_target_box(n,l,box,r);
    int32_t target[3];tomb_navigation_target(n,l,a,box,r,target);c->target_x=target[0];c->target_y=target[1];c->target_z=target[2];
}
