#include "vr.h"
#include "combat.h"
#include "enemies.h"
#include "fixed.h"
#include "city.h"
#include "object_contact.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#define PI 3.14159265358979323846
static double length(const double a[3]){return sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);}
static double distance(const double a[3],const double b[3]){double d[3];for(int j=0;j<3;j++)d[j]=a[j]-b[j];return length(d);}
static void vr_sound(TombVr *v,int id){if(v->sound_count<32)v->sounds[v->sound_count++]=id;}
static void rotate(const double q[4],const double p[3],double out[3]){
    double t[3]={2*(q[1]*p[2]-q[2]*p[1]),2*(q[2]*p[0]-q[0]*p[2]),2*(q[0]*p[1]-q[1]*p[0])};
    out[0]=p[0]+q[3]*t[0]+q[1]*t[2]-q[2]*t[1];out[1]=p[1]+q[3]*t[1]+q[2]*t[0]-q[0]*t[2];out[2]=p[2]+q[3]*t[2]+q[0]*t[1]-q[1]*t[0];
}
static void world_vector(const TombVr *v,const double p[3],double out[3]){double s=sin(v->yaw),c=cos(v->yaw);out[0]=c*p[0]-s*p[2];out[1]=-p[1];out[2]=-s*p[0]-c*p[2];}
void tomb_vr_point(const TombVr *v,const double local[3],double world[3]){
    double p[3]={local[0]-v->input.head.p[0],local[1],local[2]-v->input.head.p[2]},r[3];world_vector(v,p,r);
    for(int j=0;j<3;j++)world[j]=v->feet[j]+r[j]*TOMB_VR_SCALE;
}
void tomb_vr_direction(const TombVr *v,const TombVrPose *pose,double direction[3]){const double forward[3]={0,0,-1};double p[3];rotate(pose->q,forward,p);world_vector(v,p,direction);}
void tomb_vr_matrix(const TombVr *v,const TombVrPose *pose,double matrix[16]){
    memset(matrix,0,16*sizeof *matrix);matrix[15]=1;
    for(int i=0;i<3;i++){double basis[3]={0},p[3],r[3];basis[i]=1;rotate(pose->q,basis,p);world_vector(v,p,r);for(int j=0;j<3;j++)matrix[4*i+j]=r[j];}
    tomb_vr_point(v,pose->p,matrix+12);
}
void tomb_vr_hand_matrix(const TombVr *v,int h,double matrix[16]){
    /* The PHD wrist points along +Y, with the barrel top along +Z.
       Aim -Z is the pointing ray; grip supplies the palm position. Use the
       same frame for empty and armed hands, without a grip/aim pitch jump. */
    TombVrPose pose=v->input.aim[h];memcpy(pose.p,v->input.hand[h].p,sizeof pose.p);
    double m[16];tomb_vr_matrix(v,&pose,m);memcpy(matrix,m,sizeof m);
    for(int j=0;j<3;j++){matrix[4+j]=-m[8+j];matrix[8+j]=m[4+j];matrix[12+j]-=matrix[4+j]*25;}
}
static void gun_direction(const TombVr *v,int h,double direction[3]){
    tomb_vr_direction(v,&v->input.aim[h],direction);
    if(v->hands[h].weapon==4 && v->hands[1-h].support && v->input.hand[1-h].valid){
        double a[3],b[3];tomb_vr_point(v,v->input.hand[h].p,a);tomb_vr_point(v,v->input.hand[1-h].p,b);
        double n=distance(a,b);if(n>40)for(int j=0;j<3;j++)direction[j]=(b[j]-a[j])/n;
    }
}
int tomb_vr_barrel(const TombMeshView *mesh,int weapon,double rear[3],double muzzle[3]){
    /* End faces of the original gun meshes; the Uzi has a projecting tube. */
    static const int ends[4][8]={{8,11,12,15,9,10,13,14},{9,10,13,14,8,11,12,15},{26,27,30,31,24,25,28,29},{34,35,38,39,32,33,36,37}};
    const int *indices=ends[weapon==4?1:weapon==3?2:0];
    memset(rear,0,3*sizeof(double));memset(muzzle,0,3*sizeof(double));
    if(!mesh || !mesh->vertices || mesh->vertex_count<(weapon==3?32u:16u))return 0;
    /* The left Uzi includes an extra stock segment before its barrel tube. */
    if(weapon==3 && mesh->vertex_count>=40){
        const unsigned char *a=mesh->vertices+32*mesh->vertex_stride+2,*b=mesh->vertices+24*mesh->vertex_stride+2;
        if((int16_t)(a[0]|a[1]<<8)>(int16_t)(b[0]|b[1]<<8)+40)indices=ends[3];
    }
    for(int i=0;i<8;i++)for(int j=0;j<3;j++){
        const unsigned char *b=mesh->vertices+indices[i]*mesh->vertex_stride+2*j;
        (i<4?rear:muzzle)[j]+=(int16_t)(b[0]|b[1]<<8)*.25;
    }
    return 1;
}
void tomb_vr_weapon_matrix(const TombVr *v,int h,const TombMeshView *mesh,double matrix[16]){
    tomb_vr_hand_matrix(v,h,matrix);double rear[3],muzzle[3],f[3],up[3],right[3];
    if(h==0 && v->hands[h].support && v->hands[1].weapon==4){
        double gun[16];tomb_vr_matrix(v,&v->input.aim[1],gun);
        for(int j=0;j<3;j++)matrix[12+j]-=24*gun[j];
    }
    if(!v->hands[h].weapon || !tomb_vr_barrel(mesh,v->hands[h].weapon,rear,muzzle))return;
    for(int j=0;j<3;j++)f[j]=muzzle[j]-rear[j];double n=length(f);if(n<1)return;
    for(int j=0;j<3;j++)f[j]/=n;
    /* Map the actual barrel axis to aim -Z, keeping the gun top upright. */
    right[0]=f[1];right[1]=-f[0];right[2]=0;n=length(right);if(n<.01)return;
    for(int j=0;j<3;j++)right[j]/=n;
    up[0]=right[1]*f[2];up[1]=-right[0]*f[2];up[2]=right[0]*f[1]-right[1]*f[0];
    double original[16];memcpy(original,matrix,sizeof original);
    if(v->hands[h].weapon==4 && v->hands[1-h].support){
        double direction[3],r[3],u[3];gun_direction(v,h,direction);
        for(int j=0;j<3;j++)u[j]=original[8+j];
        r[0]=direction[1]*u[2]-direction[2]*u[1];r[1]=direction[2]*u[0]-direction[0]*u[2];r[2]=direction[0]*u[1]-direction[1]*u[0];
        double norm=length(r);if(norm>.01){for(int j=0;j<3;j++)r[j]/=norm;
            u[0]=r[1]*direction[2]-r[2]*direction[1];u[1]=r[2]*direction[0]-r[0]*direction[2];u[2]=r[0]*direction[1]-r[1]*direction[0];
            for(int j=0;j<3;j++){original[j]=r[j];original[4+j]=direction[j];original[8+j]=u[j];}
        }
    }
    for(int col=0;col<3;col++)for(int row=0;row<3;row++)matrix[4*col+row]=original[row]*right[col]+original[4+row]*f[col]+original[8+row]*up[col];
    double palm[3];tomb_vr_point(v,v->input.hand[h].p,palm);
    for(int j=0;j<3;j++)matrix[12+j]=palm[j]-matrix[4+j]*25;
}
void tomb_vr_reset(TombVr *v,const TombPlaytest *p){
    memset(v,0,sizeof *v);v->yaw=p->lara.actor.yaw*(2*PI/65536);v->feet[0]=p->lara.actor.x;v->feet[1]=p->lara.actor.y;v->feet[2]=p->lara.actor.z;v->level_room=p->lara.actor.room;
    for(int h=0;h<2;h++)v->hands[h].pickup=v->hands[h].hover=v->hands[h].lever=-1;
    v->deadzone=.18;v->selected_key=-1;v->align_heading=1;
    v->block.item=-1;
}
/* Body-relative volumes are measured from the headset, so they follow crouching
   and real turning. Shoulder priority is evaluated for BOTH hands at press time. */
int tomb_vr_zone(const TombVrInput *in,int h){
    if(!in->head.valid || !in->hand[h].valid)return 0;
    double dx=in->hand[h].p[0]-in->head.p[0],dy=in->head.p[1]-in->hand[h].p[1],dz=in->hand[h].p[2]-in->head.p[2];
    const double forward_axis[3]={0,0,-1};double facing[3];rotate(in->head.q,forward_axis,facing);
    double flat=hypot(facing[0],facing[2]);
    /* Estimate the neck pivot behind/below the eyes. Remove the positional
       change caused by pitching the headset, preserving neutral reach. */
    const double neck[3]={0,-.12,.10};double tilted[3];rotate(in->head.q,neck,tilted);
    double neutral[3]={flat>.1?-facing[0]/flat*.10:0,-.12,flat>.1?-facing[2]/flat*.10:.10};
    dx-=tilted[0]-neutral[0];dy+=tilted[1]-neutral[1];dz-=tilted[2]-neutral[2];
    double front=flat>.1?(dx*facing[0]+dz*facing[2])/flat:-dz;
    double side=flat>.1?(dx*facing[2]-dz*facing[0])/flat:dx;
    if(front<-.45)return 0;
    double horizontal=sqrt(dx*dx+dz*dz);
    if(dy>=-.30 && dy<.16 && horizontal<.50 && front<=.07)return 3;
    if(dy>=.16 && dy<.43 && fabs(side)<.32 && horizontal<.48 && front<=.15)return 2;
    if(dy>=.51 && dy<.90 && horizontal<.50 && front<=.07)return 1;
    return 0;
}
void tomb_vr_wrist_model_matrix(const double hand_matrix[16],int hand,double m[16]){
    memset(m,0,16*sizeof *m);m[15]=1;
    for(int j=0;j<3;j++){
        /* Read across the wrist, with successive rows extending toward the forearm. */
        m[j]=(hand?1:-1)*hand_matrix[8+j];m[4+j]=-hand_matrix[4+j];m[8+j]=(hand?-1:1)*hand_matrix[j];
        m[12+j]=hand_matrix[12+j]-8*hand_matrix[4+j]+14*m[8+j];
    }
}
void tomb_vr_wrist_matrix(const TombVr *v,int hand,double m[16]){
    double model[16];tomb_vr_hand_matrix(v,hand,model);tomb_vr_wrist_model_matrix(model,hand,m);
}
double tomb_vr_wrist(const TombVr *v,int hand,double position[3]){
    const TombVrPose *pose=&v->input.hand[hand];double m[16];tomb_vr_wrist_matrix(v,hand,m);memcpy(position,m+12,3*sizeof(double));
    if(!pose->valid || !v->input.head.valid)return 0;
    double head[3],toward[3],palm[3],axis[3]={hand?-1:1,0,0},normal[3],gaze[3];tomb_vr_point(v,v->input.head.p,head);
    for(int j=0;j<3;j++)toward[j]=head[j]-position[j];double n=length(toward);if(n<60 || n>120)return 0;
    for(int j=0;j<3;j++)toward[j]/=n;
    rotate(pose->q,axis,palm);world_vector(v,palm,normal);tomb_vr_direction(v,&v->input.head,gaze);
    double face=0,look=0;for(int j=0;j<3;j++){face+=normal[j]*toward[j];look-=gaze[j]*toward[j];}
    return face>.45 && look>.85?1:0;
}
static int heights_raw(TombPlaytest *p,double x,double y,double z,int room,TombHeights *out,int *resolved){
    TombSectorRef ref;if(!tomb_find_sector(p->level,(int)x,(int)y,(int)z,room,&ref))return 0;
    if(p->level->items){if(!tomb_item_heights(p->level,ref,(int)x,(int)y,(int)z,0,p->level->items,p->level->item_count,out))return 0;}
    else if(!tomb_static_heights(p->level,ref,(int)x,(int)z,0,out))return 0;
    *resolved=ref.room;return out->floor!=-32512 && out->ceiling!=-32512 && out->floor>out->ceiling;
}
static int heights(TombPlaytest *p,double x,double y,double z,int room,TombHeights *out,int *resolved){
    int ok=heights_raw(p,x,y,z,room,out,resolved);
    if(!ok || !p->level->items)return ok;
    /* Reversed bridge ramps wrap their local offset to zero on the exact
       tile boundary. Reject only an isolated spike between matching neighbours;
       real steps and walls keep their discontinuity. */
    for(int axis=0;axis<2;axis++)if(((int)(axis?z:x)&1023)==0){
        TombHeights a,b;int ar,br;double dx=axis?0:1,dz=axis?1:0;
        if(heights_raw(p,x-dx,y,z-dz,room,&a,&ar) && heights_raw(p,x+dx,y,z+dz,room,&b,&br) && abs(a.floor-b.floor)<=2 && abs(out->floor-a.floor)>128)out->floor=a.floor<b.floor?a.floor:b.floor;
    }
    return 1;
}
static double body_offset(const TombVr *v){return v->climb_offset<0?v->climb_offset:v->swimming?200-v->input.head.p[1]*TOMB_VR_SCALE:0;}
static int body_clear(TombVr *v,TombPlaytest *p,const double origin[3],int step){
    double pos[3]={origin[0],origin[1]+body_offset(v),origin[2]};
    if(v->swimming && !v->climb_offset && !p->swim_override && origin[1]<v->feet[1]){
        int water=tomb_water_height(p->level,(int)pos[0],(int)pos[2],v->level_room);
        if(water!=-32512 && pos[1]<water+50)return 0;
    }
    double standing=fmax(240,fmin(760,v->input.head.p[1]*TOMB_VR_SCALE));
    double body=v->climb_offset<0?standing+(240-standing)*fmin(1,-v->climb_offset/fmax(1,v->input.head.p[1]*TOMB_VR_SCALE-200)):v->swimming?400:standing;
    static const int offsets[5][2]={{0,0},{90,0},{-90,0},{0,90},{0,-90}};
    for(int j=0;j<5;j++){TombHeights h;int room;
        if(!heights(p,pos[0]+offsets[j][0],v->climb_offset<0?pos[1]-body*.5:pos[1]-body-160,pos[2]+offsets[j][1],v->level_room,&h,&room) || h.ceiling>pos[1]-body)return 0;
        int slope=h.floor_type==1 || h.floor_type==2;
        if(h.floor<pos[1]-step-(slope?256:0))return 0;
    }
    if(p->level->static_def_count){
        TombActor actor=p->lara.actor;actor.x=(int)pos[0];actor.y=(int)pos[1];actor.z=(int)pos[2];actor.room=(int16_t)v->level_room;
        TombContact contact={0};TombStaticContact statics;
        if(!tomb_static_contact(p->level,&actor,&contact,(int)body,90,0,&statics) || statics.hit)return 0;
    }
    if(p->objects)for(size_t i=0;i<p->objects->count;i++){
        const TombObject *o=&p->objects->items[i];
        if(o->object==38 && (o->actor.flags&6)!=6 && !(p->gameplay_options&TOMB_INVINCIBLE)){
            int16_t b[6];if(tomb_visual_item_bounds(p->visual,38,&o->actor,b)){
                double cx=o->actor.x+(b[0]+b[1])*.5,cy=o->actor.y+(b[2]+b[3])*.5,cz=o->actor.z+(b[4]+b[5])*.5;
                double r=fmax(b[1]-b[0],fmax(b[3]-b[2],b[5]-b[4]))*.5;
                double dy=cy-fmax(pos[1]-body,fmin(pos[1],cy)),dx=pos[0]-cx,dz=pos[2]-cz;
                if(dx*dx+dy*dy+dz*dz<(r+90)*(r+90))return 0;
            }
        }
        if(o->object>=48 && o->object<=51 && (o->actor.flags&6)!=6 && pos[1]>o->actor.y-1024 && pos[1]-body<o->actor.y &&
           fabs(pos[0]-o->actor.x)<602 && fabs(pos[2]-o->actor.z)<602)return 0;
    }
    /* Movement is swept in <=40-unit steps against the 90-unit body radius.
       A ray starting inside geometry would also reject escape movement. */
    return 1;
}
static void swim_recover(TombVr *v,TombPlaytest *p){
    if(!v->swimming || body_clear(v,p,v->feet,0))return;
    /* Tracking can change the compact body's height without locomotion. Find
       the nearest free placement instead of requiring a physical head retreat. */
    for(int radius=20;radius<=240;radius+=20)for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)for(int z=-1;z<=1;z++){
        double n=sqrt((double)(x*x+y*y+z*z));if(!n)continue;
        double pos[3]={v->feet[0]+radius*x/n,v->feet[1]+radius*y/n,v->feet[2]+radius*z/n};
        if(body_clear(v,p,pos,0)){memcpy(v->feet,pos,sizeof pos);return;}
    }
}
/* Use the same radius-wide support for walking, gravity and jump eligibility.
   Query above a possible step so raised bridge callbacks return their top. */
static int support(TombVr *v,TombPlaytest *p,const double pos[3],double *floor){
    TombHeights h;int room;
    if(!heights(p,pos[0],pos[1]-fmax(240,fmin(760,v->input.head.p[1]*TOMB_VR_SCALE))-160,pos[2],v->level_room,&h,&room))return 0;
    *floor=h.floor;return h.floor>=pos[1]-256;
}
static int grounded(TombVr *v,TombPlaytest *p){double floor;return !v->swimming && !v->climb_offset && v->vertical>=0 && support(v,p,v->feet,&floor) && fabs(v->feet[1]-floor)<2;}
static void move_body(TombVr *v,TombPlaytest *p,const double delta[3],int step){
    int count=(int)ceil(length(delta)/40);if(count<1)return;if(count>64)count=64;
    for(int n=0;n<count;n++)for(int axis=0;axis<3;axis++){
        double pos[3];memcpy(pos,v->feet,sizeof pos);pos[axis]+=delta[axis]/count;
        if(axis!=1 && step>0 && grounded(v,p)){
            double floor;if(support(v,p,pos,&floor) && fabs(floor-v->feet[1])<=256)pos[1]=floor;
        }
        if(body_clear(v,p,pos,step))memcpy(v->feet,pos,sizeof pos);
    }
}
static void gravity(TombVr *v,TombPlaytest *p,double dt){
    TombHeights height;int room;
    if(!heights(p,v->feet[0],v->feet[1]-v->input.head.p[1]*TOMB_VR_SCALE-160,v->feet[2],v->level_room,&height,&room)){v->vertical=0;return;}
    double bottom[3]={v->feet[0],v->feet[1]+body_offset(v),v->feet[2]};
    double floor=height.floor,supported;
    if(support(v,p,bottom,&supported) && supported>=bottom[1]-1)floor=supported;
    floor-=body_offset(v);
    if(v->feet[1]>=floor-1 && v->vertical>=0){v->feet[1]=floor;v->vertical=0;v->jumped=0;return;}
    double previous=v->vertical,next=fmin(200,previous+(previous<128?180:30)*dt);
    if(previous<0 && !v->input.primary[0] && !v->input.primary[1])next=fmin(0,previous+720*dt);
    double target=v->feet[1]+(previous+next)*15*dt;
    int land=target>=floor && next>=0;if(land)target=floor;
    double delta[3]={0,target-v->feet[1],0},old=v->feet[1];move_body(v,p,delta,0);
    if(target>old && fabs(v->feet[1]-target)>.1){
        /* The centre can leave a ledge before the body's rim clears its wall.
           Move the rim clear horizontally instead of treating it as support. */
        int clear=0;
        for(int radius=10;radius<=100 && !clear;radius+=10)for(int i=0;i<8 && !clear;i++){
            double angle=i*PI/4,pos[3]={v->feet[0]+cos(angle)*radius,target,v->feet[2]+sin(angle)*radius};
            TombHeights h;int rr;
            if(!heights(p,pos[0],old-1,pos[2],v->level_room,&h,&rr) || h.floor<target+body_offset(v))continue;
            if(body_clear(v,p,pos,0)){memcpy(v->feet,pos,sizeof pos);clear=1;}
        }
    }
    /* A blocked downward step is support, not extra time spent accelerating.
       Never resolve a ceiling by teleporting the feet down into a lower room. */
    if(fabs(v->feet[1]-target)>.1){v->vertical=0;return;}
    v->vertical=next;
    if(land){
        int health=p->lara.health;
        if(!(p->gameplay_options&TOMB_INVINCIBLE))tomb_landing_damage(&p->lara.health,(int16_t)next);
        if(old<floor-1 || previous>0)vr_sound(v,p->lara.health<health?27:4);
        v->impact=next;v->vertical=0;v->jumped=0;
    }
}
static void sync_actor(TombVr *v,TombPlaytest *p){
    TombActor *a=&p->lara.actor;a->x=(int32_t)llround(v->feet[0]);a->y=(int32_t)llround(v->feet[1]+body_offset(v));a->z=(int32_t)llround(v->feet[2]);
    TombHeights h;int room;if(heights(p,v->feet[0],a->y-100,v->feet[2],v->level_room,&h,&room)){a->room=(int16_t)room;v->level_room=room;p->lara.sector_floor=h.floor;}
}
static int reachable(TombPlaytest *p,const double from[3],const double to[3]){
    TombCameraPoint a={(int)from[0],(int)from[1],(int)from[2],p->lara.actor.room},b={(int)to[0],(int)to[1],(int)to[2],a.room};
    tomb_shooter_world_ray(p,a,&b);double result[3]={b.x,b.y,b.z};return distance(result,to)<30;
}
static int hand_object(TombVr *v,TombPlaytest *p,const double hand[3]){
    if(!p->objects)return -1;double head[3];tomb_vr_point(v,v->input.head.p,head);int best=-1;double nearest=140;
    for(size_t i=0;i<p->objects->count;i++){
        TombObject *o=p->objects->items+i;if((o->actor.flags&6)==6 || v->hands[0].pickup==(int)i || v->hands[1].pickup==(int)i || v->hands[0].lever==(int)i || v->hands[1].lever==(int)i)continue;
        int pickup=tomb_pickup_object(o->object);if(!pickup && o->object!=55 && o->object!=56)continue;
        if(pickup && (o->actor.flags&4))continue;
        double point[3]={o->actor.x,o->actor.y-80,o->actor.z};
        if(!pickup){
            /* Switch origins are not their handles: the wall lever is roughly
               475 units forward and 640 above the origin. Use its animated
               bounds, including underwater levers, in the placed orientation. */
            int16_t bounds[6];if(!tomb_visual_item_bounds(p->visual,o->object,&o->actor,bounds))continue;
            double angle=o->actor.yaw*(2*PI/65536),s=sin(angle),c=cos(angle),dx=hand[0]-o->actor.x,dz=hand[2]-o->actor.z;
            double local[3]={c*dx-s*dz,hand[1]-o->actor.y,s*dx+c*dz};
            for(int j=0;j<3;j++)local[j]=fmax(bounds[j*2],fmin(bounds[j*2+1],local[j]));
            point[0]=o->actor.x+c*local[0]+s*local[2];point[1]=o->actor.y+local[1];point[2]=o->actor.z-s*local[0]+c*local[2];
        }
        double d=distance(hand,point);
        if(d<nearest && reachable(p,head,point)){best=(int)i;nearest=d;}
    }return best;
}
static int ledge(TombVr *v,TombPlaytest *p,const double hand[3],TombVrHand *grasp){
    /* Resolve above the hand on the open side first: a thin platform may
       have a separate room below its solid floor. */

    for(int i=0;i<4;i++){
        double a=i*PI/2,x=hand[0]+cos(a)*65,z=hand[2]+sin(a)*65;TombHeights high,low;int room;
        TombSectorRef approach;int probe_room=v->level_room;
        if(tomb_find_sector(p->level,(int)(hand[0]-cos(a)*90),(int)hand[1]-110,(int)(hand[2]-sin(a)*90),probe_room,&approach))probe_room=approach.room;
        if(!heights(p,x,hand[1]-110,z,probe_room,&high,&room) || abs(high.floor-(int)hand[1])>100)continue;
        grasp->ledge_room=room;
        if(!heights(p,hand[0]-cos(a)*90,hand[1]-110,hand[2]-sin(a)*90,v->level_room,&low,&room) || low.floor-high.floor<200)continue;
        if(high.floor-high.ceiling<350)continue;
        grasp->anchor[0]=hand[0];grasp->anchor[1]=high.floor;grasp->anchor[2]=hand[2];
        grasp->ledge_normal[0]=cos(a);grasp->ledge_normal[1]=0;grasp->ledge_normal[2]=sin(a);
        return 1;
    }return 0;
}
static int surface_grip(TombVr *v,TombPlaytest *p,const double hand[3],TombVrHand *grasp){
    if(!v->swimming && !(p->gameplay_options&TOMB_VR_CLIMB_ANYTHING))return 0;
    double head[3];tomb_vr_point(v,v->input.head.p,head);
    /* A tracked hand can already be through a sloping ceiling. Start the
       query in the head's room, not in the solid side of the hand's ray. */
    TombHeights ceiling;int room;
    if(heights(p,hand[0],head[1],hand[2],v->level_room,&ceiling,&room) && fabs(hand[1]-ceiling.ceiling)<=80){
        double contact[3]={hand[0],ceiling.ceiling+2,hand[2]};
        if(reachable(p,head,contact)){memcpy(grasp->anchor,hand,sizeof grasp->anchor);memset(grasp->ledge_normal,0,sizeof grasp->ledge_normal);return 1;}
    }
    for(int ray=0;ray<7;ray++){
        double from[3],end[3];memcpy(from,hand,sizeof from);memcpy(end,hand,sizeof end);
        if(ray==0){memcpy(from,head,sizeof from);double d[3];for(int j=0;j<3;j++)d[j]=hand[j]-head[j];double n=length(d);if(n<1)continue;for(int j=0;j<3;j++)end[j]+=d[j]/n*80;}
        else end[(ray-1)/2]+=((ray-1)%2?1:-1)*80;
        TombCameraPoint a={(int)from[0],(int)from[1],(int)from[2],(int16_t)v->level_room},b={(int)end[0],(int)end[1],(int)end[2],a.room};
        tomb_shooter_world_ray(p,a,&b);double hit[3]={b.x,b.y,b.z};
        if(distance(hit,end)<3 || distance(hit,hand)>80 || distance(hit,from)<2)continue;
        if(!reachable(p,head,hit))continue;
        memcpy(grasp->anchor,hand,sizeof grasp->anchor);memset(grasp->ledge_normal,0,sizeof grasp->ledge_normal);
        return 1;
    }
    return 0;
}
/* Expand only into free space after the last grip is released. If the
   compact body is supported, raise the tracking origin gradually as the legs
   extend; otherwise preserve the view and let gravity handle the drop. */
static void climb_expand(TombVr *v,TombPlaytest *p,double dt){
    if(v->climb_offset>=0 || v->hands[0].anchored || v->hands[1].anchored)return;
    if(v->swimming){v->climb_offset=0;return;}
    double old=v->climb_offset,next=fmin(0,old+900*dt),pos[3];memcpy(pos,v->feet,sizeof pos);
    v->climb_offset=next;
    if(body_clear(v,p,pos,0))return;
    double bottom[3]={v->feet[0],v->feet[1]+old,v->feet[2]},floor;
    if(support(v,p,bottom,&floor) && fabs(bottom[1]-floor)<40){
        pos[1]-=next-old;
        if(body_clear(v,p,pos,0)){memcpy(v->feet,pos,sizeof pos);return;}
    }
    v->climb_offset=old;
}
static void release_pickup(TombVr *v,TombPlaytest *p,int h,int collect){
    TombVrHand *hand=v->hands+h;if(hand->pickup<0 || !p->objects || (size_t)hand->pickup>=p->objects->count){hand->pickup=-1;return;}
    TombObject *o=p->objects->items+hand->pickup;
    if(collect){
        /* The original routine returns zero for a NEW weapon/ammunition too;
           that return value describes ring insertion, not collection success. */
        tomb_inventory_add(&p->inventory,o->object);
        o->actor.flags|=6;o->active=0;++p->inventory.pickups;p->inventory.last_pickup=o->object;p->inventory.pickup_ticks=75;tomb_hud_pickup(&p->hud,o->object);
    }else{
        o->actor.x=p->lara.actor.x;o->actor.z=p->lara.actor.z;o->actor.y=p->lara.actor.y;o->actor.room=p->lara.actor.room;
        TombHeights floor;int room;if(heights(p,o->actor.x,o->actor.y-10,o->actor.z,o->actor.room,&floor,&room)){o->actor.y=floor.floor;o->actor.room=(int16_t)room;}
        o->actor.flags&=(uint8_t)~6u;
    }
    hand->pickup=-1;tomb_enemies_room_sync(p->objects);
}
#include "vr_interactions.h"
void tomb_vr_frame(TombVr *v,TombPlaytest *p,const TombVrInput *input,double dt,int paused){
    if(dt<=0 || dt>.1)dt=1.0/90;
    v->input=*input;v->menu_request=0;
    if(!input->head.valid){if(!v->block.moving)v->block.item=-1;v->ready=0;v->shot_count=0;for(int h=0;h<2;h++){v->hands[h].key=0;if(v->hands[h].lever>=0 && p->objects){p->objects->items[v->hands[h].lever].actor=v->hands[h].lever_frames[0];v->hands[h].lever=-1;}v->hands[h].weapon=0;v->hands[h].support=0;v->hands[h].anchored=0;v->hands[h].gripped=0;release_pickup(v,p,h,0);}return;}
    int fresh=!v->ready;
    if(v->align_heading){double d[3];tomb_vr_direction(v,&input->head,d);v->yaw+=p->lara.actor.yaw*(2*PI/65536)-atan2(d[0],d[2]);v->align_heading=0;}
    if(fresh){memcpy(v->last_head,input->head.p,sizeof v->last_head);v->ready=1;}
    if(paused){if(!v->block.moving)v->block.item=-1;memcpy(v->last_head,input->head.p,sizeof v->last_head);v->jump_latch=input->primary[0] || input->primary[1];v->shot_count=0;for(int h=0;h<2;h++){v->hands[h].key=0;if(v->hands[h].lever>=0 && p->objects){p->objects->items[v->hands[h].lever].actor=v->hands[h].lever_frames[0];v->hands[h].lever=-1;}memcpy(v->hands[h].previous,input->hand[h].p,sizeof v->hands[h].previous);v->hands[h].support=0;v->hands[h].gripped=input->grip[h]>.55;v->hands[h].triggered=input->trigger[h]>.55;v->hands[h].weapon=0;v->hands[h].anchored=0;release_pickup(v,p,h,0);}return;}
    if(p->lara.health<=0){for(int h=0;h<2;h++){v->hands[h].key=0;if(v->hands[h].lever>=0 && p->objects){p->objects->items[v->hands[h].lever].actor=v->hands[h].lever_frames[0];v->hands[h].lever=-1;}v->hands[h].weapon=0;v->hands[h].anchored=0;release_pickup(v,p,h,0);}return;}
    if(v->hands[0].anchored || v->hands[1].anchored)v->climb_offset=fmin(0,200-input->head.p[1]*TOMB_VR_SCALE);
    double before_feet[3];memcpy(before_feet,v->feet,sizeof before_feet);
    double local_delta[3]={input->head.p[0]-v->last_head[0],input->head.p[1]-v->last_head[1],input->head.p[2]-v->last_head[2]},delta[3];world_vector(v,local_delta,delta);delta[1]=0;
    for(int j=0;j<3;j++)delta[j]*=TOMB_VR_SCALE;
    if(length(delta)<300)move_body(v,p,delta,256);
    memcpy(v->last_head,input->head.p,sizeof v->last_head);
    for(int h=0;h<2;h++){
        double movement[3];for(int j=0;j<3;j++)movement[j]=fresh?0:(input->hand[h].p[j]-v->hands[h].previous[j]-local_delta[j])/dt;
        world_vector(v,movement,v->hand_velocity[h]);
    }
    double head_world[3];tomb_vr_point(v,input->head.p,head_world);TombSectorRef water_ref;
    if(v->climb_offset && tomb_find_sector(p->level,(int)head_world[0],(int)head_world[1],(int)head_world[2],v->level_room,&water_ref))v->level_room=water_ref.room;
    v->swimming=tomb_find_sector(p->level,(int)head_world[0],(int)head_world[1],(int)head_world[2],p->lara.actor.room,&water_ref) && (p->level->rooms[water_ref.room].flags&1);
    if(!v->swimming && tomb_find_sector(p->level,(int)v->feet[0],(int)(head_world[1]+200),(int)v->feet[2],v->level_room,&water_ref) && (p->level->rooms[water_ref.room].flags&1))v->swimming=1;
    if(!v->swimming && !v->climb_offset && (v->vertical>0 || v->was_swimming) && tomb_find_sector(p->level,(int)v->feet[0],(int)v->feet[1]-1,(int)v->feet[2],v->level_room,&water_ref) && (p->level->rooms[water_ref.room].flags&1))v->swimming=1;
    if(p->swim_override<0 && !v->swimming)p->swim_override=0;
    if(p->swim_override)v->swimming=p->swim_override>0;
    if(v->swimming!=v->was_swimming){
        memset(v->swim_velocity,0,sizeof v->swim_velocity);
        if(v->swimming){v->swim_velocity[1]=v->vertical*30;v->vertical=0;vr_sound(v,33);}else vr_sound(v,36);
        if(!v->swimming && v->was_swimming && v->climb_offset<0)v->pool_exit_ticks=180;
        v->was_swimming=v->swimming;
    }
    swim_recover(v,p);
    for(int h=0;h<2;h++){
        double wrist[3],target=tomb_vr_wrist(v,h,wrist);
        v->wrist_alpha[h]=target;
        if(v->swimming){if(v->hands[h].weapon)vr_sound(v,7);v->hands[h].weapon=0;v->hands[h].support=0;v->hands[h].flash=0;}
    }
    if(v->swimming)v->shot_count=0;
    int hanging=v->hands[0].anchored || v->hands[1].anchored;
    float turn=input->stick[1][0];if(fabs(turn)<.3)v->snap_latch=0;
    if(!hanging && fabs(turn)>.7 && !v->snap_latch){v->yaw+=turn>0?PI/4:-PI/4;v->snap_latch=1;}
    double forward[3];tomb_vr_direction(v,&input->head,forward);double yaw=atan2(forward[0],forward[2]);p->lara.actor.yaw=tomb_word((int)(yaw*65536/(2*PI)));
    p->shooter.yaw=p->lara.actor.yaw;p->shooter.pitch=tomb_word((int)(atan2(-forward[1],hypot(forward[0],forward[2]))*65536/(2*PI)));
    if(!hanging && !v->swimming && !v->menu_open){
        double x=input->stick[0][0],z=input->stick[0][1],n=hypot(x,z);
        if(n>v->deadzone){double speed=fmin(1,(n-v->deadzone)/(1-v->deadzone))*(v->jumped?85:47)*30*dt;delta[0]=(cos(yaw)*x+sin(yaw)*z)/n*speed;delta[1]=0;delta[2]=(-sin(yaw)*x+cos(yaw)*z)/n*speed;move_body(v,p,delta,256);}
    }
    int primary=input->primary[0] || input->primary[1];
    if(!primary)v->menu_primary=0;
    if(v->menu_open && primary)v->menu_primary=1;
    int jump=primary && !v->menu_primary;
    if(jump && !v->jump_latch && !hanging && !v->swimming && !v->menu_open && grounded(v,p)){v->vertical=-108;v->jumped=1;}
    v->jump_latch=jump;
    v->sliding=0;
    if(!hanging && grounded(v,p)){
        TombHeights h;int room;
        if(heights(p,v->feet[0],v->feet[1]-100,v->feet[2],v->level_room,&h,&room) && h.floor_type==2){
            TombSectorRef ref;int16_t tilt=0;
            if(tomb_find_sector(p->level,(int)v->feet[0],(int)v->feet[1]-100,(int)v->feet[2],room,&ref) && tomb_floor_tilt(p->level,ref,(int)v->feet[0],h.floor,(int)v->feet[2],&tilt)){
                double x=-(int8_t)(tilt&255),z=-(int8_t)((uint16_t)tilt>>8),n=hypot(x,z);
                if(n>0){v->sliding=1;double slide[3]={x/n*50*30*dt,0,z/n*50*30*dt};move_body(v,p,slide,256);}
            }
        }
    }
    int both=tomb_vr_zone(input,0)==3 && tomb_vr_zone(input,1)==3;
    if(!v->menu_open)block_gesture(v,p);
    double pull[3]={0};int anchors=0;
    for(int h=0;h<2;h++){
        TombVrHand *hand=v->hands+h;hand->flash=fmax(0,hand->flash-dt);
        if(hand->anchored==2 && !v->swimming && !(p->gameplay_options&TOMB_VR_CLIMB_ANYTHING))hand->anchored=0;
        int valid=input->hand[h].valid,grip=valid && input->grip[h]>(hand->gripped?.35:.6),pressed=grip&&!hand->gripped;
        double world[3];tomb_vr_point(v,input->hand[h].p,world);
        if(!valid || (!grip && hand->gripped)){if(hand->weapon)vr_sound(v,7);hand->weapon=0;hand->support=0;hand->anchored=0;release_pickup(v,p,h,valid && tomb_vr_zone(input,h)==3);}
        if(hand->key && (!valid || !grip))vr_key_release(v,p,h,world);
        if(h==1 && pressed && v->selected_key>=0 && !hand->weapon){int slot=tomb_quest_slot(v->selected_key);if(slot>=0 && p->inventory.quest[slot])hand->key=v->selected_key;v->selected_key=-1;}
        if(hand->lever>=0){vr_lever_move(v,p,h,valid && grip,world[1],dt);hand->gripped=grip;memcpy(hand->previous,input->hand[h].p,sizeof hand->previous);continue;}
        if(hand->key){hand->gripped=grip;memcpy(hand->previous,input->hand[h].p,sizeof hand->previous);continue;}
        int supporting=valid && grip && shotgun_support(v,h,pressed);
        hand->hover=valid && !hand->weapon && hand->pickup<0 && !hand->anchored?hand_object(v,p,world):-1;
        if(pressed && !supporting && !(v->block.item>=0 && v->block.hand==h)){
            if(hand->hover>=0){
                TombObject *o=p->objects->items+hand->hover;
                if(tomb_pickup_object(o->object))hand->pickup=hand->hover;
                else if(!(o->actor.flags&6) && (o->actor.current==0 || o->actor.current==1))vr_lever_begin(hand,p,hand->hover,world[1]);
            }else if(ledge(v,p,world,hand)){hand->anchored=1;v->climb_offset=fmin(0,200-input->head.p[1]*TOMB_VR_SCALE);v->vertical=0;}
            else if(surface_grip(v,p,world,hand)){hand->anchored=2;v->climb_offset=fmin(0,200-input->head.p[1]*TOMB_VR_SCALE);v->vertical=0;}
            else {
                int zone=tomb_vr_zone(input,h),weapon=both?3:zone==1?1:zone==2?2:zone==3 && h==1?4:0;
                if(zone==3 && !both && h==0)v->menu_request=1;
                int slot=weapon==4?1:weapon==2?2:weapon==3?3:0;
                if(weapon && !v->swimming && p->inventory.counts[slot]){hand->weapon=weapon;vr_sound(v,6);}
            }
        }
        if(hand->anchored && valid){
            for(int j=0;j<3;j++)pull[j]+=hand->anchor[j]-world[j];anchors++;
        }
        if(hand->pickup>=0 && p->objects){TombObject *o=p->objects->items+hand->pickup;o->actor.x=(int)world[0];o->actor.y=(int)world[1]+80;o->actor.z=(int)world[2];o->actor.room=p->lara.actor.room;}
        if(v->swimming && valid && !fresh && !hand->weapon && !hand->anchored && hand->pickup<0 && !hand->support)swim_stroke(v,h,dt);
        memcpy(hand->previous,input->hand[h].p,sizeof hand->previous);
        int trigger=valid && input->aim[h].valid && input->trigger[h]>(hand->triggered?.35:.6);
        hand->automatic-=dt;
        if(hand->weapon && trigger && (!hand->triggered || (hand->weapon==3 && hand->automatic<=0)) && v->shot_count<64){
            double direction[3],origin[3];tomb_vr_point(v,input->aim[h].p,origin);gun_direction(v,h,direction);
            double head[3];tomb_vr_point(v,input->head.p,head);
            if(hand->weapon==4 && hand->pump)vr_sound(v,48);
            else if(reachable(p,head,origin)){
                unsigned i=v->shot_count++;v->shots[i].origin=(TombCameraPoint){(int)origin[0],(int)origin[1],(int)origin[2],p->lara.actor.room};v->shots[i].weapon=hand->weapon;v->shots[i].hand=h;
                v->shots[i].yaw=tomb_word((int)(atan2(direction[0],direction[2])*65536/(2*PI)));v->shots[i].pitch=tomb_word((int)(atan2(-direction[1],hypot(direction[0],direction[2]))*65536/(2*PI)));
                hand->automatic=2.0/30;
                if(hand->weapon==4){hand->pump=1;}
            }
        }
        hand->gripped=grip;hand->triggered=trigger;
    }
    if(anchors){
        for(int j=0;j<3;j++)pull[j]/=anchors;
        double start[3];memcpy(start,v->feet,sizeof start);
        if(length(pull)<400)move_body(v,p,pull,0);v->vertical=0;
        if(v->swimming)for(int j=0;j<3;j++)v->swim_velocity[j]=(v->feet[j]-start[j])/dt;
        double speed=length(v->swim_velocity);if(speed>2400)for(int j=0;j<3;j++)v->swim_velocity[j]*=2400/speed;
    }
    climb_expand(v,p,dt);
    if(v->swimming && !anchors){
        /* Original swimming gains 2 world units/tick per tick, up to 50:
           1800 units/s^2 and 1500 units/s at 30 Hz. Physical strokes remain. */
        double kick=v->menu_open || v->menu_primary?0:(input->primary[0] || input->primary[1]);
        if(kick){
            double change[3];for(int j=0;j<3;j++)change[j]=1500*forward[j]-v->swim_velocity[j];
            double n=length(change),fraction=n>0?fmin(1,1800*dt/n):0;
            for(int j=0;j<3;j++)v->swim_velocity[j]+=change[j]*fraction;
            double speed=length(v->swim_velocity);if(speed>1500)for(int j=0;j<3;j++)v->swim_velocity[j]*=1500/speed;
        }
        double drag=kick?1:exp(-dt*1.3);
        for(int j=0;j<3;j++){v->swim_velocity[j]*=drag;delta[j]=v->swim_velocity[j]*dt;}v->swim_speed=length(v->swim_velocity);move_body(v,p,delta,0);
    }
    if(!v->swimming && !v->hands[0].anchored && !v->hands[1].anchored && p->lara.health>0)gravity(v,p,dt);
    if(!anchors && !v->sliding && (v->swimming || v->vertical==0)){
        v->sound_distance+=distance(before_feet,v->feet);
        if(v->sound_distance>(v->swimming?900:500)){v->sound_distance=0;vr_sound(v,v->swimming?35:0);}
    }
    v->move_speed=hypot(v->feet[0]-before_feet[0],v->feet[2]-before_feet[2])/(30*dt);
    if(!v->swimming && !anchors && !v->vertical && !v->jumped && fabs(v->feet[1]-before_feet[1])<=256)v->view_offset+=before_feet[1]-v->feet[1];
    v->view_offset*=exp(-dt*16);if(v->swimming)v->view_offset=0;
    sync_actor(v,p);
}
int tomb_vr_tick(TombVr *v,TombPlaytest *p){
    if(!v->ready)return 1;
    TombActor *a=&p->lara.actor;
    double head[3];tomb_vr_point(v,v->input.head.p,head);TombSectorRef ref;
    int water=tomb_find_sector(p->level,(int)head[0],(int)head[1],(int)head[2],a->room,&ref) && (p->level->rooms[ref.room].flags&1);
    int body_water=tomb_find_sector(p->level,(int)v->feet[0],(int)(head[1]+200),(int)v->feet[2],v->level_room,&ref) && (p->level->rooms[ref.room].flags&1);
    p->lara.water_status=(p->swim_override>0 || water || body_water)?1:0;
    if(water && v->swimming && !(p->gameplay_options&TOMB_INVINCIBLE)){if(p->lara.air>0)--p->lara.air;else if(p->lara.health>0)p->lara.health-=5;}else p->lara.air=1800;
    sync_actor(v,p);a->flags=v->vertical!=0?a->flags|8:a->flags&~8u;a->fall_speed=(int16_t)fmax(v->vertical,v->impact);if(v->impact>0)a->flags|=8;a->speed=(int16_t)v->move_speed;
    if(p->lara.health>0){a->animation=v->swimming?108:11;a->frame=p->level->animations[a->animation].first_frame;a->current=a->goal=v->swimming?(water?13:33):2;}
    int32_t before[3]={a->x,a->y,a->z};
    if(water && v->swimming && !v->hands[0].anchored && !v->hands[1].anchored && p->objects){TombContact contact={0};contact.old_x=a->x;contact.old_y=a->y;contact.old_z=a->z;contact.positive_limit=32512;contact.negative_limit=-400;contact.ceiling_limit=400;tomb_water_current(p->objects,a,&contact,&p->water);}
    if(v->pool_exit_ticks>0){a->current=55;--v->pool_exit_ticks;}
    int old_swim=p->swim_override;
    p->vr_body_bottom=v->feet[1]+body_offset(v);
    double standing=fmax(240,fmin(760,v->input.head.p[1]*TOMB_VR_SCALE));
    p->vr_body_height=v->climb_offset<0?standing+(240-standing)*fmin(1,-v->climb_offset/fmax(1,v->input.head.p[1]*TOMB_VR_SCALE-200)):v->swimming?400:standing;
    int ok=tomb_playtest_tick(p,(!v->menu_open && !v->menu_primary && (v->input.primary[0] || v->input.primary[1]))?16:0);
    v->impact=0;
    if(p->swim_override!=old_swim){v->swimming=p->swim_override>0;v->vertical=v->swim_speed=0;memset(v->swim_velocity,0,sizeof v->swim_velocity);}
    int32_t after[3]={a->x,a->y,a->z};for(int j=0;j<3;j++)v->feet[j]+=after[j]-before[j];v->level_room=p->lara.actor.room;
    for(unsigned i=0;i<v->shot_count;i++)if(p->lara.health>0){
        TombVrHand *hand=v->hands+v->shots[i].hand;
        if(tomb_vr_fire(p,v->shots[i].weapon,v->shots[i].origin,v->shots[i].yaw,v->shots[i].pitch)){
            if(hand->weapon==v->shots[i].weapon){hand->flash=.045;++v->fired[v->shots[i].hand];}
        }else if(v->shots[i].weapon==4)hand->pump=0;
    }
    if(ok)ok=block_tick(v,p);
    if(p->lara.health<=0 && !v->death_sound){
        /* Read the death vocal from the original animation's sound commands. */
        if(p->level->animation_count>25){
            const TombAnim *death=p->level->animations+25;int index=death->command_index;
            static const int sizes[7]={0,3,2,0,0,2,2};
            for(int n=0;n<death->command_count && index>=0 && (size_t)index<p->level->command_count;n++){
                int op=p->level->commands[index++];if(op<1 || op>6 || (size_t)(index+sizes[op])>p->level->command_count)break;
                if(op==5){vr_sound(v,p->level->commands[index+1]&0x3fff);break;}index+=sizes[op];
            }
        }
        v->death_sound=1;
    }
    if(v->sliding && p->lara.health>0)vr_sound(v,3);
    for(unsigned i=0;i<v->sound_count && p->sound_count<TOMB_SOUND_EVENTS;i++){
        p->sounds[p->sound_count++]=(TombSoundEvent){9,v->sounds[i],2,a->x,a->y,a->z,NULL};++p->sound_events;
    }
    v->sound_count=0;
    v->shot_count=0;return ok;
}
