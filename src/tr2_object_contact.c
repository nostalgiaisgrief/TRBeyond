#include "tr2_world.h"

int tr2_object_overlap(const Tr2Level *l,const Tr2Actor *o,const Tr2Actor *a,int radius){
    int b[6],ab[6];if(!tr2_best_bounds(l,o,b) || !tr2_best_bounds(l,a,ab))return 0;
    if(o->y+b[3]<=a->y+ab[2] || o->y+b[2]>=a->y+ab[3])return 0;
    int s=tr2_sin(o->yaw),c=tr2_cos(o->yaw),dx=a->x-o->x,dz=a->z-o->z;
    int x=tr2_asr(tr2_long((int64_t)dx*c-(int64_t)dz*s),14),z=tr2_asr(tr2_long((int64_t)dx*s+(int64_t)dz*c),14);
    return x>=b[0]-radius && x<=b[1]+radius && z>=b[4]-radius && z<=b[5]+radius;
}
int tr2_push_horizontal(const Tr2Actor *o,Tr2Actor *a,const int b[6],int radius){
    int s=tr2_sin(o->yaw),c=tr2_cos(o->yaw),dx=a->x-o->x,dz=a->z-o->z;
    int x=tr2_asr(tr2_long((int64_t)dx*c-(int64_t)dz*s),14),z=tr2_asr(tr2_long((int64_t)dx*s+(int64_t)dz*c),14);
    int lo=b[0]-radius,hi=b[1]+radius,back=b[4]-radius,front=b[5]+radius;
    if(x<lo || x>hi || z<back || z>front)return 0;
    int left=x-lo,right=hi-x,forward=front-z,behind=z-back;
    if(left<=right && left<=forward && left<=behind)x=lo;
    else if(right<=left && right<=forward && right<=behind)x=hi;
    else if(forward<=left && forward<=right && forward<=behind)z=front;
    else z=back;
    a->x=o->x+tr2_asr(tr2_long((int64_t)x*c+(int64_t)z*s),14);
    a->z=o->z+tr2_asr(tr2_long((int64_t)z*c-(int64_t)x*s),14);
    return 1;
}
/* ItemPushLara (0x413a10), non-spaz branch used by Winston. The contact
   check rejects a push into walls, then refreshes the collision origin. */
int tr2_object_push(const Tr2Level *l,const Tr2Actor *o,Tr2Actor *a,Tr2Contact *contact){return tr2_object_push_reaction(l,o,a,contact,0);}
int tr2_object_push_reaction(const Tr2Level *l,const Tr2Actor *o,Tr2Actor *a,Tr2Contact *contact,int react){
    return tr2_object_push_bounds(l,o,a,contact,react,1);
}
int tr2_object_push_bounds(const Tr2Level *l,const Tr2Actor *o,Tr2Actor *a,Tr2Contact *contact,int react,int expand){
    int b[6];if(!tr2_best_bounds(l,o,b))return 0;
    int s=tr2_sin(o->yaw),c=tr2_cos(o->yaw),dx=a->x-o->x,dz=a->z-o->z;
    if(!tr2_push_horizontal(o,a,b,expand?contact->radius:0))return 0;
    Tr2World *w=l->world;
    if(react && b[3]-b[2]>256 && w){
        int mx=(b[0]+b[1])/2,mz=(b[4]+b[5])/2;
        int center_x=tr2_asr(tr2_long((int64_t)mx*c+(int64_t)mz*s),14);
        int center_z=tr2_asr(tr2_long((int64_t)mz*c-(int64_t)mx*s),14);
        w->hit_direction=(int16_t)(((unsigned)(a->yaw+32768-tr2_atan(dz-center_z,dx-center_x)+8192)&65535)>>14);
        if(!w->hit_frame && a->event_count<16){int n=a->event_count++;a->event_kind[n]=5;a->event_value[n]=31;}
        w->hit_frame=tr2_word(w->hit_frame+1);if(w->hit_frame>34)w->hit_frame=34;
    }
    int facing=contact->facing;contact->positive=32512;contact->negative=-384;contact->ceiling_limit=0;
    contact->facing=tr2_word(tr2_atan(a->z-contact->old[2],a->x-contact->old[0]));
    tr2_contact(l,a,contact,762,1);contact->facing=(int16_t)facing;
    if(contact->type){a->x=contact->old[0];a->z=contact->old[2];return 0;}
    contact->old[0]=a->x;contact->old[1]=a->y;contact->old[2]=a->z;
    Tr2Sector sector;if(tr2_find_sector(l,a->x,a->y-10,a->z,a->room,&sector))a->room=(int16_t)sector.room;
    return 1;
}
/* Object63 collision435780 delegates to DoorCollision413920 only in state0. */
void tr2_object63_contact(const Tr2Level *l,const Tr2Object *o,Tr2Actor *a,Tr2Contact *contact){
    if(o->pose.control.current!=0 || !(contact->flags&8))return;
    if(!tr2_object_overlap(l,&o->pose,a,contact->radius) || !tr2_touch(l,&o->pose,o->id,a,0))return;
    tr2_object_push_reaction(l,&o->pose,a,contact,o->pose.control.current!=o->pose.control.goal && (contact->flags&16));
}
/* DrawLara 0x419ea8 uses an un-interpolated hit frame without changing state. */
const uint8_t *tr2_hit_frame(const Tr2Level *l,const Tr2World *w){
    static const unsigned animations[4]={125,127,126,128};
    if(!w || w->hit_direction<0 || w->hit_direction>3 || w->hit_frame<0)return NULL;
    unsigned animation=animations[w->hit_direction];if(animation>=l->animations.count)return NULL;
    const uint8_t *an=l->animations.data+animation*32;
    uint64_t offset=tr2_u32(an)+(uint64_t)w->hit_frame*an[5]*2;
    if(offset+an[5]*2>l->frames.count*2u)return NULL;
    return l->frames.data+offset;
}
