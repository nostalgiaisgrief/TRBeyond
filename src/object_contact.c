/* Reconstructed from 0x163dc, 0x164a4, 0x167c4 and 0x39130. */
#include "object_contact.h"
#include "fixed.h"
#include "folly.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "enemies.h"
#include "playtest.h"
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int sn(const int16_t *s,int angle){unsigned a=(unsigned)angle&65535;int sign=a>=32768?-1:1;a&=32767;if(a>16384)a=32768-a;return sign*s[a>>4];}
int16_t tomb_object_angle(int32_t z,int32_t x) {
    /* 0x3f070: ratio quantization and rounded atan table, including the
       original -65535 quadrant constant. */
    static const int offsets[8]={0,-16384,-65535,49152,-32768,16384,32768,-49152};
    if(!z && !x)return 0;
    int quadrant=0;int64_t a=z,b=x;
    if(a<0){quadrant+=4;a=-a;}if(b<0){quadrant+=2;b=-b;}
    if(b>a){int64_t t=a;a=b;b=t;quadrant++;}
    int index=(int)(b*2048/a),angle=(int)floor(atan(index/2048.0)*10430.378350470453+.5)+offsets[quadrant];
    return tomb_word(angle<0?-angle:angle);
}
int tomb_object_pose(const TombVisual *v,const TombActor *a,TombPose *p) {
    if(a->animation<0 || (size_t)a->animation>=v->animation_count)return 0;
    const TombAnim *an=v->level->animations+a->animation;int rate=v->animations[a->animation*32+4];
    int f=a->frame-an->first_frame;if(!rate || f<0 || a->frame>an->last_frame)return 0;
    int key=f/rate,den=rate;
    if((key+1)*rate>an->last_frame)den=an->last_frame-key*rate;
    return tomb_visual_key(v,(size_t)a->animation,(size_t)(key+(f%rate>den/2)),p);
}
typedef struct Matrix {int32_t m[3][4];} Matrix;
static void rotate(Matrix *m,int axis,int angle,const int16_t *s) {
    if(!(uint16_t)angle)return;
    int p=axis==0?1:0,q=axis==2?1:2,si=sn(s,angle),co=sn(s,angle+16384);
    if(axis==1)si=-si;
    for(int i=0;i<3;i++) {
        int32_t a=m->m[i][p],b=m->m[i][q];
        m->m[i][p]=tomb_asr(tomb_long((int64_t)a*co+(int64_t)b*si),14);
        m->m[i][q]=tomb_asr(tomb_long((int64_t)b*co-(int64_t)a*si),14);
    }
}
static void angles(Matrix *m,const uint16_t r[3],const int16_t *s){rotate(m,1,r[1],s);rotate(m,0,r[0],s);rotate(m,2,r[2],s);}
void tomb_rotate_vector(int16_t yaw,int16_t pitch,int16_t roll,const int32_t in[3],int inverse,const int16_t *s,int32_t out[3]) {
    Matrix m={{{16384,0,0,0},{0,16384,0,0},{0,0,16384,0}}};uint16_t a[3]={(uint16_t)pitch,(uint16_t)yaw,(uint16_t)roll};angles(&m,a,s);
    for(int j=0;j<3;j++){int64_t n=0;for(int k=0;k<3;k++)n+=(int64_t)(inverse?m.m[k][j]:m.m[j][k])*in[k];out[j]=tomb_asr(tomb_long(n),14);}
}
static void translate(Matrix *m,int32_t x,int32_t y,int32_t z) {
    for(int i=0;i<3;i++)m->m[i][3]=tomb_long((int64_t)m->m[i][3]+(int64_t)m->m[i][0]*x+(int64_t)m->m[i][1]*y+(int64_t)m->m[i][2]*z);
}
/* Gun-hand transform follows the same torso and independent arm hierarchy as
   presentation. Simulation uses a discrete body key, not render interpolation. */
int tomb_lara_gun_point(const TombPlaytest *p,int left,int32_t out[3]){
    const TombVisual *v=p->visual;const TombActor *a=&p->lara.actor;const int16_t *s=p->animation.sine_quarter;
    const TombGunArm *arm=left?&p->pistols.left:&p->pistols.right;
    TombModel model;TombPose body,gun;int frame=arm->frame;
    int an=p->weapon_type==4?(frame<13?164:frame<47?165:frame<80?166:frame<114?167:168):(frame<5?160:frame<13?161:frame<24?162:163);
    if(!tomb_visual_model(v,0,&model) || !tomb_object_pose(v,a,&body) || body.count!=15 || (size_t)an>=v->animation_count || !tomb_visual_key(v,(size_t)an,(size_t)(frame-v->level->animations[an].first_frame),&gun))return 0;
    Matrix m={{{16384,0,0,0},{0,16384,0,0},{0,0,16384,0}}},stack[15];int top=0;
    uint16_t ar[3]={(uint16_t)p->lara.pitch,(uint16_t)a->yaw,(uint16_t)p->movement.lean};angles(&m,ar,s);Matrix base=m;
    translate(&m,body.root[0],body.root[1],body.root[2]);
    int hand=left?13:10;
    for(int j=0;j<=hand;j++){
        if(j){const unsigned char *tree=v->trees+4*(model.tree_word+4*(j-1));unsigned flags=u32(tree);
            if(flags&1){if(!top)return 0;m=stack[--top];}if(flags&2){if(top==15)return 0;stack[top++]=m;}
            translate(&m,tomb_long(u32(tree+4)),tomb_long(u32(tree+8)),tomb_long(u32(tree+12)));
        }
        if(j>=8 && j<=13){
            const TombGunArm *ga=j<11?&p->pistols.right:&p->pistols.left;
            if(p->weapon_type!=4 && (j==8 || j==11)){
                for(int row=0;row<3;row++)for(int col=0;col<3;col++)m.m[row][col]=base.m[row][col];
                rotate(&m,1,ga->yaw,s);rotate(&m,0,ga->pitch,s);rotate(&m,2,ga->roll,s);
            }
            /* Only the selected arm's pose contributes to its final hand. */
            angles(&m,gun.rotation[j],s);
        }else angles(&m,body.rotation[j],s);
        if(j==7){rotate(&m,1,p->lara.torso_yaw,s);rotate(&m,0,p->lara.torso_pitch,s);}
    }
    translate(&m,0,155,55);
    out[0]=a->x+tomb_asr(m.m[0][3],14);out[1]=a->y+tomb_asr(m.m[1][3],14);out[2]=a->z+tomb_asr(m.m[2][3],14);return 1;
}
static int transforms(const TombVisual *v,int object,const TombActor *a,int16_t pitch,int16_t roll,int16_t head,int16_t neck,const int32_t origin[3],int16_t view_yaw,int16_t view_pitch,const int16_t *s,TombSphere out[64],int joint,int32_t point[3]) {
    TombModel model;TombPose pose;
    if(!tomb_visual_model(v,(uint32_t)object,&model) || !tomb_object_pose(v,a,&pose) || pose.count!=model.mesh_count || !pose.count)return 0;
    Matrix m={{{16384,0,0,0},{0,16384,0,0},{0,0,16384,0}}},stack[64];int top=0;
    if(origin) {
        int sy=sn(s,view_yaw),cy=sn(s,view_yaw+16384),sp=sn(s,view_pitch),cp=sn(s,view_pitch+16384);
        m=(Matrix){{{cy,0,-sy,0},{tomb_asr(sp*sy,14),cp,tomb_asr(sp*cy,14),0},{tomb_asr(cp*sy,14),-sp,tomb_asr(cp*cy,14),0}}};
        translate(&m,a->x-origin[0],a->y-origin[1],a->z-origin[2]);
    }
    uint16_t ar[3]={(uint16_t)pitch,(uint16_t)a->yaw,(uint16_t)roll};angles(&m,ar,s);
    translate(&m,pose.root[0],pose.root[1],pose.root[2]);
    for(unsigned j=0;j<pose.count;j++) {
        if(j) {
            const unsigned char *tree=v->trees+4*(model.tree_word+4*(j-1));uint32_t flags=u32(tree);
            if(flags&1){if(!top)return 0;m=stack[--top];}
            if(flags&2){if(top==64)return 0;stack[top++]=m;}
            /* Caves doors and Lara have no creature extra-angle channels. */
            if(flags&28)return 0;
            translate(&m,tomb_long(u32(tree+4)),tomb_long(u32(tree+8)),tomb_long(u32(tree+12)));
        }
        angles(&m,pose.rotation[j],s);if((object==34 && j==2) || ((object>=30 && object<=32) && j==1) || (object==7 && j==3) || (object==8 && j==14) || (object==18 && (j==11 || j==12)) || (object==19 && j==22) || (object==24 && j==3) || ((object==27 || object==28) && j==7) || ((object==10 || object==11) && j==8) || ((object==12 || object==13 || object==14) && j==20) || (object==15 && j==14) || ((object==16 || object==17) && j==2))rotate(&m,1,head,s);
        if(object>=20 && object<=22){if(j==1)rotate(&m,1,head,s);if(j==3)rotate(&m,1,neck,s);}
        if(object==33 && j==3){rotate(&m,0,head,s);rotate(&m,2,neck,s);}
        if(object==23 && j==11){rotate(&m,1,head,s);rotate(&m,0,neck,s);}
        Matrix centre=m;
        if((int)j==joint) {
            translate(&centre,point[0],point[1],point[2]);
            point[0]=a->x+tomb_asr(centre.m[0][3],14);point[1]=a->y+tomb_asr(centre.m[1][3],14);point[2]=a->z+tomb_asr(centre.m[2][3],14);return 1;
        }
        if(joint>=0)continue;
        const TombMeshView *mesh=v->meshes+model.mesh_start+j;
        translate(&centre,mesh->centre[0],mesh->centre[1],mesh->centre[2]);
        out[j]=(TombSphere){(origin?0:a->x)+tomb_asr(centre.m[0][3],14),(origin?0:a->y)+tomb_asr(centre.m[1][3],14),(origin?0:a->z)+tomb_asr(centre.m[2][3],14),(int16_t)mesh->radius};
        /* DOS GetSpheres includes hidden wing meshes of grounded mutants. */
    }
    return pose.count;
}
int tomb_object_joint(const TombVisual *v,int object,const TombActor *a,int16_t pitch,int16_t roll,int16_t head,int joint,int32_t point[3],const int16_t *s) {
    return transforms(v,object,a,pitch,roll,head,0,NULL,0,0,s,NULL,joint,point);
}
int tomb_object_spheres_view(const TombVisual *v,int object,const TombActor *a,int16_t pitch,int16_t roll,int16_t head,const int32_t origin[3],int16_t view_yaw,int16_t view_pitch,const int16_t *s,TombSphere out[64]) {
    return transforms(v,object,a,pitch,roll,head,0,origin,view_yaw,view_pitch,s,out,-1,NULL);
}
int tomb_object_joint_joints(const TombVisual *v,int object,const TombActor *a,int16_t pitch,int16_t roll,int16_t head,int16_t neck,int joint,int32_t point[3],const int16_t *s){return transforms(v,object,a,pitch,roll,head,neck,NULL,0,0,s,NULL,joint,point);}
int tomb_object_spheres_joints(const TombVisual *v,int object,const TombActor *a,int16_t pitch,int16_t roll,int16_t head,int16_t neck,const int32_t origin[3],int16_t yaw,int16_t vp,const int16_t *s,TombSphere out[64]){return transforms(v,object,a,pitch,roll,head,neck,origin,yaw,vp,s,out,-1,NULL);}
int tomb_object_spheres(const TombVisual *v,int object,const TombActor *a,int16_t pitch,int16_t roll,const int16_t *s,TombSphere out[64]) {
    return tomb_object_spheres_view(v,object,a,pitch,roll,0,NULL,0,0,s,out);
}
static void local(const TombActor *o,const TombActor *a,const int16_t *s,int32_t *x,int32_t *z) {
    int32_t dx=a->x-o->x,dz=a->z-o->z,si=sn(s,o->yaw),co=sn(s,o->yaw+16384);
    *x=tomb_asr(tomb_long((int64_t)dx*co-(int64_t)dz*si),14);
    *z=tomb_asr(tomb_long((int64_t)dz*co+(int64_t)dx*si),14);
}
int tomb_object_push(const TombActor *o,const int16_t b[6],TombActor *a,int radius,const int16_t *s) {
    int32_t x,z;local(o,a,s,&x,&z);
    int lo=b[0]-radius,hi=b[1]+radius,near=b[4]-radius,far=b[5]+radius;
    if(x<lo || x>hi || z<near || z>far)return 0;
    int left=x-lo,right=hi-x,back=far-z,front=z-near;
    if(left<=right && left<=back && left<=front)x=lo;
    else if(right<=left && right<=back && right<=front)x=hi;
    else if(back<=left && back<=right && back<=front)z=far;
    else z=near;
    int si=sn(s,o->yaw),co=sn(s,o->yaw+16384);
    a->x=o->x+tomb_asr(tomb_long((int64_t)x*co+(int64_t)z*si),14);
    a->z=o->z+tomb_asr(tomb_long((int64_t)z*co-(int64_t)x*si),14);return 1;
}
static int objects_collide(TombObjects *w,TombActor *a,TombContact *c,TombAnimContext *ctx,int16_t pitch,int16_t roll,TombQuery query,void *user,int *hit,int only_object) {
    *hit=0;
    TombPose lara;TombSphere ls[64];int ln=0;
    if(!tomb_object_pose(w->visual,a,&lara))return 0;
    const TombRoomPortals *portals=w->visual->portals+a->room;int first_room=a->room;
    /* BaddieCollision visits the current room, then its portal neighbours.
       Initial item insertion is at the room-list head (descending item IDs). */
    for(size_t pass=0;pass<=portals->count;pass++) {
      int room=first_room;
      if(pass){const unsigned char *p=portals->data+32*(pass-1);room=(int)(p[0]|(unsigned)p[1]<<8);}
      for(int next=w->enemies?w->enemies->room_head[room]:(int)w->count-1;next>=0;next=w->enemies?w->enemies->room_next[next]:next-1) {
        size_t i=(size_t)next;
        TombObject head;
        for(int part=0;part<(w->items[i].object==44?2:1);part++){
        TombObject *o=w->items+i;
        if(only_object>=0 && o->object!=only_object)continue;
        if(o->object==145 || (w->invincible && (o->object==37 || o->object==38 || o->object==180)))continue;
        if(part){if(!tomb_folly_head(w,o,&head))return 0;o=&head;if(o->actor.current==2)continue;}
        int enemy=tomb_enemy_object(o->object),blade=o->object==36 || o->object==42,solid=o->object==147 || o->object==161 || o->object==163 || o->object==177 || o->object==181,hazard=o->object==180 || o->object==43 || o->object==24 || o->object==37 || o->object==38 || o->object==53;
        if((!enemy && !blade && !hazard && !solid && o->object!=44 && o->object!=45 && o->object!=47 && (o->object<57 || o->object>64 || !(c->flags&8))) || !(o->actor.flags&32) || (o->actor.flags&6)==6)continue;
        TombCreature *creature=enemy && w->enemies?w->enemies->items+i:NULL;
        if(o->actor.room!=room || llabs((long long)a->x-o->actor.x)>=4096 || llabs((long long)a->y-o->actor.y)>=4096 || llabs((long long)a->z-o->actor.z)>=4096)continue;
        TombPose pose;if(!tomb_object_pose(w->visual,&o->actor,&pose))return 0;
        if(a->y+lara.bounds[2]>=o->actor.y+pose.bounds[3] || o->actor.y+pose.bounds[2]>=a->y+lara.bounds[3])continue;
        int32_t x,z;local(&o->actor,a,ctx->sine_quarter,&x,&z);
        if(x<pose.bounds[0]-100 || x>pose.bounds[1]+100 || z<pose.bounds[4]-100 || z>pose.bounds[5]+100)continue;
        TombSphere ds[64];int dn=tomb_object_spheres_joints(w->visual,o->object,&o->actor,creature?creature->pitch:0,creature?creature->roll:0,creature?creature->head:0,creature?creature->neck:0,NULL,0,0,ctx->sine_quarter,ds);
        if(!ln)ln=tomb_object_spheres(w->visual,0,a,pitch,roll,ctx->sine_quarter,ls);
        if(!ln || !dn)return 0;
        uint32_t touching=0;
        for(int d=0;d<dn;d++)for(int l=0;l<ln;l++) {
            int64_t dx=(int64_t)ds[d].x-ls[l].x,dy=(int64_t)ds[d].y-ls[l].y,dz=(int64_t)ds[d].z-ls[l].z,r=(int64_t)ds[d].radius+ls[l].radius;
            if(ds[d].radius>0 && ls[l].radius>0 && dx*dx+dy*dy+dz*dz<r*r)touching|=1u<<d;
        }
        /* The altar and EmberEmitter use DOS ObjectCollision (16388),
           including its mesh-sphere test before the bounds push. */
        if((solid && o->object!=147 && o->object!=177) || (o->object>=43 && o->object<=45))touching=1;
        if(creature)creature->touch=touching;
        if(hazard){
            w->enemies->items[i].touch=touching;
            /* Rolling balls push airborne Lara; stopped balls remain solid. */
            if(o->object!=43 && o->object!=24 && !(o->object==38 && ((o->actor.flags&6)!=2 || (a->flags&8))))continue;
        }
        if(blade){w->enemies->items[i].touch=touching;if((o->actor.flags&6)==2)continue;}
        int32_t dx=a->x-o->actor.x,dz=a->z-o->actor.z;
        if(!touching || !(c->flags&8) || !tomb_object_push(&o->actor,pose.bounds,a,enemy?0:100,ctx->sine_quarter))continue;
        if(!solid && !blade && !hazard && (o->object<43 || o->object>47) && (creature?creature->health>0:o->actor.current!=o->actor.goal) && (c->flags&16)) {
            int cx=(pose.bounds[0]+pose.bounds[1])/2,cz=(pose.bounds[4]+pose.bounds[5])/2;
            int si=sn(ctx->sine_quarter,o->actor.yaw),co=sn(ctx->sine_quarter,o->actor.yaw+16384);
            dx-=tomb_asr(cx*co+cz*si,14);dz-=tomb_asr(cz*co-cx*si,14);
            int angle=tomb_object_angle(dz,dx);
            *hit=1+((uint16_t)(a->yaw-angle+32768+8192)/16384);
        }
        int16_t facing=c->facing;c->positive_limit=32512;c->negative_limit=-384;c->ceiling_limit=0;
        c->facing=tomb_object_angle(a->z-c->old_z,a->x-c->old_x);
        query(user,a,c,762);c->facing=facing;
        if(c->type){a->x=c->old_x;a->z=c->old_z;}
        else {c->old_x=a->x;c->old_y=a->y;c->old_z=a->z;TombSectorRef ref;if(tomb_find_sector(w->level,a->x,a->y-10,a->z,a->room,&ref))a->room=(int16_t)ref.room;}
        ln=0;
        }
      }
    }
    return 1;
}

int tomb_doors_collide(TombObjects *w,TombActor *a,TombContact *c,TombAnimContext *ctx,int16_t pitch,int16_t roll,TombQuery query,void *user,int *hit) {
    return objects_collide(w,a,c,ctx,pitch,roll,query,user,hit,-1);
}
int tomb_scion_altar_collide(TombObjects *w,TombActor *a,TombContact *c,TombAnimContext *ctx,TombQuery query,void *user) {
    int hit=0;
    return objects_collide(w,a,c,ctx,0,0,query,user,&hit,147);
}
