#include "tr2_world.h"
#include <string.h>

/* 14-bit matrices used by GetJointAbsPosition/GetSpheres. Translation stays
   in fixed point until the final world-space result, as in the executable. */
typedef struct {int32_t v[3][4];} Matrix;
static void rotate(Matrix *m,int axis,int angle){
    if(!tr2_word(angle))return;
    int s=tr2_sin(angle),c=tr2_cos(angle),p=axis==0?1:axis==1?2:0,q=axis==0?2:axis==1?0:1;
    for(int i=0;i<3;i++){int a=m->v[i][p],b=m->v[i][q];
        m->v[i][p]=tr2_asr(tr2_long((int64_t)a*c+(int64_t)b*s),14);
        m->v[i][q]=tr2_asr(tr2_long((int64_t)b*c-(int64_t)a*s),14);}
}
static void translate(Matrix *m,int x,int y,int z){for(int i=0;i<3;i++)m->v[i][3]=tr2_long((int64_t)m->v[i][3]+(int64_t)m->v[i][0]*x+(int64_t)m->v[i][1]*y+(int64_t)m->v[i][2]*z);}
void tr2_rotate_offset(int yaw,int pitch,const int vector[3],int out[3]){
    Matrix m={{{16384,0,0,0},{0,16384,0,0},{0,0,16384,0}}};rotate(&m,1,yaw);rotate(&m,0,pitch);
    translate(&m,vector[0],vector[1],vector[2]);for(int i=0;i<3;i++)out[i]=tr2_asr(m.v[i][3],14);
}
static int packed(Matrix *m,const uint8_t **p,const uint8_t *end){
    if(*p+2>end)return 0;unsigned a=tr2_u16(*p);*p+=2;
    int axis=a>>14;if(axis){rotate(m,axis-1,(a&1023)<<6);return 1;}
    if(*p+2>end)return 0;unsigned b=tr2_u16(*p);*p+=2;
    rotate(m,1,(((a&15)<<6)|(b>>10))<<6);rotate(m,0,((a>>4)&1023)<<6);rotate(m,2,(b&1023)<<6);return 1;
}
static const uint8_t *nearest_frame(const Tr2Level *l,const Tr2Actor *a){
    if((uint32_t)a->control.animation>=l->animations.count)return NULL;
    const uint8_t *an=l->animations.data+a->control.animation*32;int rate=an[4],delta=a->control.frame-tr2_i16(an+16);
    if(!rate || delta<0)return NULL;int index=delta/rate,part=delta%rate,next=(index+1)*rate;
    if(part && next>tr2_i16(an+18))rate+=tr2_i16(an+18)-next;
    if(part>rate/2)index++;
    uint32_t off=tr2_u32(an)+(uint32_t)index*an[5]*2;
    return off+18<=l->frames.count*2?l->frames.data+off:NULL;
}
static int weapon_torso(const Tr2Level *l,const Tr2Actor *a,const Tr2Combat *g,Matrix *out){
    const uint8_t *an=l->animations.data+a->control.animation*32,*model=tr2_model(l,0),*end=l->frames.data+l->frames.count*2;
    int rate=an[4],delta=a->control.frame-tr2_i16(an+16);if(!rate || delta<0 || !model)return 0;
    int index=delta/rate,part=delta%rate,next=(index+1)*rate;
    if(part && next>tr2_i16(an+18))rate+=tr2_i16(an+18)-next;
    Matrix samples[2];
    for(int k=0;k<(part?2:1);k++){
        const uint8_t *f=l->frames.data+tr2_u32(an)+(index+k)*an[5]*2;
        if(f+18>end)return 0;Matrix m={{{16384,0,0,0},{0,16384,0,0},{0,0,16384,0}}};
        rotate(&m,1,a->yaw);rotate(&m,0,a->pitch);rotate(&m,2,a->control.roll);
        translate(&m,tr2_i16(f+12),tr2_i16(f+14),tr2_i16(f+16));f+=18;if(!packed(&m,&f,end))return 0;
        const uint8_t *b=l->trees.data+(tr2_u32(model+8)+24)*4;
        translate(&m,tr2_i32(b+4),tr2_i32(b+8),tr2_i32(b+12));
        for(int j=0;j<6;j++){if(f+4>end)return 0;f+=(tr2_u16(f)>>14)?2:4;}
        if(!packed(&m,&f,end))return 0;rotate(&m,1,g->torso_yaw);rotate(&m,0,g->torso_pitch);samples[k]=m;
    }
    *out=samples[0];if(part)for(int i=0;i<3;i++)for(int j=0;j<4;j++)out->v[i][j]=rate==2?tr2_asr(samples[0].v[i][j]+samples[1].v[i][j],1):samples[0].v[i][j]+(samples[1].v[i][j]-samples[0].v[i][j])*part/rate;
    return 1;
}
static int pose_matrix(const Tr2Level *l,const Tr2Actor *a,int id,const Tr2Look *look,const Matrix *view,const Tr2Combat *gun,Matrix out[64]){
    const uint8_t *model=tr2_model(l,(uint32_t)id),*f=nearest_frame(l,a),*end=l->frames.data+l->frames.count*2;
    if(!model || !f)return 0;int n=tr2_u16(model+4);if(n<1 || n>64)return 0;
    const Tr2Creature *creature=NULL;const Tr2World *world=l->world;
    if(world && (uintptr_t)a>=(uintptr_t)world->objects && (uintptr_t)a<(uintptr_t)(world->objects+world->count)){
        uintptr_t offset=(uintptr_t)a-(uintptr_t)world->objects;if(offset%sizeof(Tr2Object)==0)creature=world->creatures+offset/sizeof(Tr2Object);
    }
    Matrix m={{{16384,0,0,0},{0,16384,0,0},{0,0,16384,0}}},stack[64];int depth=0;
    if(view)m=*view;
    rotate(&m,1,a->yaw);rotate(&m,0,a->pitch);rotate(&m,2,a->control.roll);
    translate(&m,tr2_i16(f+12),tr2_i16(f+14),tr2_i16(f+16));f+=18;
    if(!packed(&m,&f,end))return 0;out[0]=m;
    for(int i=1;i<n;i++){
        uint32_t at=tr2_u32(model+8)+(uint32_t)(i-1)*4;if(at+4>l->trees.count)return 0;
        const uint8_t *b=l->trees.data+at*4;int flags=tr2_i32(b);
        if((flags&1) && depth)m=stack[--depth];if(flags&2)stack[depth++]=m;
        const uint8_t *translation=b;
        if(gun && gun->weapon==8 && i==11){
            const uint8_t *an=l->animations.data+a->control.animation*32;
            /* 0x42babc uses the right shoulder offset in the interpolated
               flare path. Preserve this executable quirk. */
            if(an[4] && (a->control.frame-tr2_i16(an+16))%an[4])translation=l->trees.data+(tr2_u32(model+8)+28)*4;
        }
        translate(&m,tr2_i32(translation+4),tr2_i32(translation+8),tr2_i32(translation+12));
        if(gun && ((gun->rifle_active && i>=8 && i<=13) || (gun->weapon==8 && i>=11 && i<=13))){
            int body=gun->weapon==8 && !gun->flare_control;const Tr2Actor *source=body?a:&gun->rifle;
            const Tr2Level *assets=gun->rifle_active?tr2_weapon_assets(l,tr2_weapon_model(gun->weapon)):l;
            const uint8_t *rend=assets->frames.data+assets->frames.count*2;
            const uint8_t *an=assets->animations.data+source->control.animation*32;
            uint32_t off=tr2_u32(an)+(unsigned)((source->control.frame-tr2_i16(an+16))/(body?an[4]:1))*an[5]*2;
            if(off+18>assets->frames.count*2 || f+2>end)return 0;
            const uint8_t *rf=assets->frames.data+off+18;
            for(int j=0;j<i;j++){if(rf+4>rend)return 0;rf+=(tr2_u16(rf)>>14)?2:4;}
            f+=(tr2_u16(f)>>14)?2:4;if(!packed(&m,&rf,rend))return 0;
        }else if(!packed(&m,&f,end))return 0;
        rotate(&m,(id==29 && i==15) || id==22?2:1,tr2_creature_joint_rotation(id,i,creature));
        if(gun && i==7 && !weapon_torso(l,a,gun,&m))return 0;
        if(look && id==0 && (i==7 || i==14)){rotate(&m,1,i==7?look->torso_yaw:look->yaw);rotate(&m,0,i==7?look->torso_pitch:look->pitch);}
        out[i]=m;
    }return n;
}
static int pose(const Tr2Level *l,const Tr2Actor *a,int id,const Tr2Look *look,Matrix out[64]){return pose_matrix(l,a,id,look,NULL,NULL,out);}
int tr2_weapon_joint(const Tr2Level *l,const Tr2Actor *a,const Tr2Combat *g,int joint,int point[3]){
    Matrix matrices[64];int n=pose_matrix(l,a,0,NULL,NULL,g,matrices);if(joint<0 || joint>=n)return 0;
    Matrix m=matrices[joint];translate(&m,point[0],point[1],point[2]);point[0]=a->x+tr2_asr(m.v[0][3],14);point[1]=a->y+tr2_asr(m.v[1][3],14);point[2]=a->z+tr2_asr(m.v[2][3],14);return 1;
}
int tr2_spheres_shot(const Tr2Level *l,const Tr2Actor *a,int id,const Tr2Position *origin,int yaw,int pitch,Tr2Sphere out[64]){
    int sy=tr2_sin(yaw),cy=tr2_cos(yaw),sp=tr2_sin(pitch),cp=tr2_cos(pitch);
    Matrix view={{{cy,0,-sy,0},{tr2_asr(sy*sp,14),cp,tr2_asr(cy*sp,14),0},{tr2_asr(sy*cp,14),-sp,tr2_asr(cy*cp,14),0}}};
    translate(&view,a->x-origin->x,a->y-origin->y,a->z-origin->z);
    Matrix matrices[64];int n=pose_matrix(l,a,id,NULL,&view,NULL,matrices);const uint8_t *model=tr2_model(l,(uint32_t)id);if(!n || !model)return 0;
    unsigned first=tr2_u16(model+6);if(first+(unsigned)n>l->mesh_offsets.count)return 0;
    for(int j=0;j<n;j++){
        unsigned off=tr2_u32(l->mesh_offsets.data+(first+j)*4);if(off+10>l->mesh_data.count*2)return 0;
        const uint8_t *mesh=l->mesh_data.data+off;Matrix m=matrices[j];translate(&m,tr2_i16(mesh),tr2_i16(mesh+2),tr2_i16(mesh+4));
        out[j]=(Tr2Sphere){tr2_asr(m.v[0][3],14),tr2_asr(m.v[1][3],14),tr2_asr(m.v[2][3],14),tr2_i16(mesh+6)};
    }return n;
}
int tr2_best_bounds(const Tr2Level *l,const Tr2Actor *a,int bounds[6]){
    const uint8_t *f=nearest_frame(l,a);if(!f)return 0;for(int i=0;i<6;i++)bounds[i]=tr2_i16(f+i*2);return 1;
}
int tr2_joint(const Tr2Level *l,const Tr2Actor *a,int id,int joint,int point[3]){
    Matrix matrices[64];int n=pose(l,a,id,NULL,matrices);if(joint<0 || joint>=n)return 0;
    Matrix m=matrices[joint];translate(&m,point[0],point[1],point[2]);
    point[0]=a->x+tr2_asr(m.v[0][3],14);point[1]=a->y+tr2_asr(m.v[1][3],14);point[2]=a->z+tr2_asr(m.v[2][3],14);return 1;
}
int tr2_spheres(const Tr2Level *l,const Tr2Actor *a,int id,Tr2Sphere out[64]){
    Matrix matrices[64];int n=pose(l,a,id,NULL,matrices);const uint8_t *model=tr2_model(l,(uint32_t)id);if(!n || !model)return 0;
    unsigned first=tr2_u16(model+6);if(first+(unsigned)n>l->mesh_offsets.count)return 0;
    for(int j=0;j<n;j++){
        unsigned off=tr2_u32(l->mesh_offsets.data+(first+j)*4);if(off+10>l->mesh_data.count*2)return 0;
        const uint8_t *mesh=l->mesh_data.data+off;Matrix m=matrices[j];translate(&m,tr2_i16(mesh),tr2_i16(mesh+2),tr2_i16(mesh+4));
        out[j]=(Tr2Sphere){a->x+tr2_asr(m.v[0][3],14),a->y+tr2_asr(m.v[1][3],14),a->z+tr2_asr(m.v[2][3],14),tr2_i16(mesh+6)};
    }return n;
}
uint32_t tr2_touch(const Tr2Level *l,const Tr2Actor *a,int id,const Tr2Actor *b,int bid){
    Tr2Sphere x[64],y[64];int nx=tr2_spheres(l,a,id,x),ny=tr2_spheres(l,b,bid,y);uint32_t mask=0;
    for(int i=0;i<nx && i<32;i++)if(x[i].radius>0)for(int j=0;j<ny;j++)if(y[j].radius>0){
        int64_t dx=x[i].x-y[j].x,dy=x[i].y-y[j].y,dz=x[i].z-y[j].z,r=x[i].radius+y[j].radius;
        if(dx*dx+dy*dy+dz*dz<r*r){mask|=1u<<i;break;}
    }
    return mask;
}
int tr2_hair_samples(const Tr2Level *l,const Tr2Actor *a,const Tr2Look *look,int anchor[3],Tr2Sphere body[5]){
    Matrix matrices[64];if(pose(l,a,0,look,matrices)<15)return 0;
    static const int joints[]={0,7,14,8,11},meshes[]={0,7,14,8,8};
    const uint8_t *model=tr2_model(l,0);unsigned first=tr2_u16(model+6);
    for(int j=0;j<5;j++){
        const uint8_t *mesh=l->mesh_data.data+tr2_u32(l->mesh_offsets.data+(first+meshes[j])*4);
        Matrix m=matrices[joints[j]];translate(&m,tr2_i16(mesh),tr2_i16(mesh+2),tr2_i16(mesh+4));
        body[j]=(Tr2Sphere){a->x+tr2_asr(m.v[0][3],14),a->y+tr2_asr(m.v[1][3],14),a->z+tr2_asr(m.v[2][3],14),tr2_i16(mesh+6)};
        if(j>=3)body[j].radius=body[j].radius*3/2;
    }
    Matrix m=matrices[14];translate(&m,0,-23,-55);
    anchor[0]=a->x+tr2_asr(m.v[0][3],14);anchor[1]=a->y+tr2_asr(m.v[1][3],14);anchor[2]=a->z+tr2_asr(m.v[2][3],14);return 1;
}
