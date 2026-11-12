/* Shared rendering/contact poses. Grabs use triangle surfaces and enclosed
   mesh volume; AABBs only reject distant meshes, never authorize a grab. */
#include "preview_pose.h"
static unsigned grab_u16(const unsigned char *p){return p[0]|(unsigned)p[1]<<8;}
static int32_t grab_i32(const unsigned char *p){return tomb_long((uint32_t)grab_u16(p)|(uint32_t)grab_u16(p+2)<<16);}
static void grab_identity(double m[16]){memset(m,0,16*sizeof *m);for(int i=0;i<4;i++)m[5*i]=1;}
static void grab_multiply(double a[16],const double b[16]){
    double out[16]={0};for(int c=0;c<4;c++)for(int r=0;r<4;r++)for(int k=0;k<4;k++)out[c*4+r]+=a[k*4+r]*b[c*4+k];memcpy(a,out,sizeof out);
}
static void grab_translate(double m[16],double x,double y,double z){for(int r=0;r<3;r++)m[12+r]+=m[r]*x+m[4+r]*y+m[8+r]*z;}
static void grab_rotation(double m[16],const uint16_t angles[3]){
    float f[16];double d[16];tomb_preview_rotation(angles,angles,0,f);for(int i=0;i<16;i++)d[i]=f[i];grab_multiply(m,d);
}
int tomb_vr_pickup_pose(const TombVr *v,const TombPlaytest *p,const TombObject *object,int16_t yaw,TombVrPickupPose *out){
    const TombVisual *visual=p->visual;int id=tomb_pickup_model(object->object);TombModel *model=&out->model;TombPose *pose=&out->pose;
    if(!visual || id<0 || !tomb_visual_model(visual,(unsigned)id,model) || !model->mesh_count || model->mesh_count>64 ||
       (size_t)model->mesh_start+model->mesh_count>visual->mesh_count || !tomb_visual_key(visual,model->animation,0,pose) || pose->count!=model->mesh_count)return 0;
    const TombActor *a=&object->actor;int held=-1;
    if(v && p->objects)for(int h=0;h<2;h++)if(v->hands[h].pickup>=0 && (size_t)v->hands[h].pickup<p->objects->count && p->objects->items+v->hands[h].pickup==object){held=h;a=&v->hands[h].pickup_start;}
    double cx=(pose->bounds[0]+pose->bounds[1])*.5,cz=(pose->bounds[4]+pose->bounds[5])*.5,support=a->y;
    TombSectorRef ref;TombHeights floor;
    if(tomb_find_sector(p->level,a->x,a->y,a->z,a->room,&ref) && tomb_static_heights(p->level,ref,a->x,a->z,0,&floor) && abs(floor.floor-a->y)<32){
        for(int x=0;x<2;x++)for(int z=0;z<2;z++){
            int32_t local[3]={(int32_t)(pose->bounds[x]-cx),0,(int32_t)(pose->bounds[4+z]-cz)},point[3];
            tomb_rotate_vector(yaw,0,0,local,0,p->animation.sine_quarter,point);
            int px=a->x+point[0],pz=a->z+point[2];
            if(tomb_find_sector(p->level,px,a->y,pz,a->room,&ref) && tomb_static_heights(p->level,ref,px,pz,0,&floor) && abs(floor.floor-a->y)<256 && floor.floor<support)support=floor.floor;
        }
    }
    out->light_actor=*a;out->light_actor.y=(int32_t)support-2;out->light_actor.yaw=yaw;
    double m[16],stack[64][16];int depth=0;grab_identity(m);if(held>=0)tomb_vr_pickup_motion(v,held,m);
    grab_translate(m,a->x,support-2,a->z);uint16_t angles[3]={0,(uint16_t)yaw,0};grab_rotation(m,angles);
    grab_translate(m,pose->root[0]-cx,pose->root[1]-pose->bounds[3],pose->root[2]-cz);grab_rotation(m,pose->rotation[0]);memcpy(out->matrix[0],m,sizeof m);
    for(unsigned j=1;j<model->mesh_count;j++){
        size_t offset=(size_t)model->tree_word*4+(j-1)*16;if(offset+16>visual->tree_words*4)return 0;
        const unsigned char *bone=visual->trees+offset;int flags=grab_i32(bone);
        if(flags&1){if(!depth)return 0;memcpy(m,stack[--depth],sizeof m);}
        if(flags&2){if(depth>=64)return 0;memcpy(stack[depth++],m,sizeof m);}
        grab_translate(m,grab_i32(bone+4),grab_i32(bone+8),grab_i32(bone+12));grab_rotation(m,pose->rotation[j]);memcpy(out->matrix[j],m,sizeof m);
    }
    return 1;
}
int tomb_vr_key_matrix(const TombVr *v,const TombPlaytest *p,int h,TombModel *model,double m[16]){
    if(!p->visual || !tomb_visual_model(p->visual,v->hands[h].key,model) || !model->mesh_count || model->mesh_start>=p->visual->mesh_count)return 0;
    const TombMeshView *mesh=p->visual->meshes+model->mesh_start;if(!mesh->vertex_count)return 0;
    int lo[3]={32767,32767,32767},hi[3]={-32768,-32768,-32768};
    for(unsigned n=0;n<mesh->vertex_count;n++)for(int j=0;j<3;j++){int q=(int16_t)grab_u16(mesh->vertices+n*mesh->vertex_stride+j*2);if(q<lo[j])lo[j]=q;if(q>hi[j])hi[j]=q;}
    tomb_vr_item_matrix(v,h,m);uint16_t angles[3]={32768,0,0};grab_rotation(m,angles);
    grab_translate(m,-(lo[0]+hi[0])*.5,-(lo[1]+hi[1])*.5,-(lo[2]+hi[2])*.5);return 1;
}
typedef struct GrabTriangle { double v[3][3]; } GrabTriangle;
typedef struct GrabMesh { GrabTriangle *triangles;size_t count;double lo[3],hi[3]; } GrabMesh;
static void grab_cross(const double a[3],const double b[3],double c[3]){c[0]=a[1]*b[2]-a[2]*b[1];c[1]=a[2]*b[0]-a[0]*b[2];c[2]=a[0]*b[1]-a[1]*b[0];}
static double grab_dot(const double a[3],const double b[3]){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static int grab_mesh(const TombMeshView *mesh,const double matrix[16],GrabMesh *out){
    memset(out,0,sizeof *out);size_t count=0;for(int g=0;g<4;g++)count+=mesh->face_count[g]*(g%2?1:2);
    if(!count || count>1000000)return 0;
    out->triangles=malloc(count*sizeof *out->triangles);if(!out->triangles)return 0;
    for(int j=0;j<3;j++){out->lo[j]=1e30;out->hi[j]=-1e30;}
    for(int g=0;g<4;g++){int n=g%2?3:4;
        for(size_t f=0;f<mesh->face_count[g];f++){
            const unsigned char *face=mesh->faces[g]+f*(n+1)*2;double points[4][3];
            for(int i=0;i<n;i++){
                unsigned index=grab_u16(face+2*i);if(index>=mesh->vertex_count){free(out->triangles);return 0;}
                const unsigned char *vertex=mesh->vertices+index*mesh->vertex_stride;double q[3];for(int j=0;j<3;j++)q[j]=(int16_t)grab_u16(vertex+j*2);
                for(int j=0;j<3;j++){double a=matrix[j]*q[0]+matrix[4+j]*q[1]+matrix[8+j]*q[2]+matrix[12+j];points[i][j]=a;out->lo[j]=fmin(out->lo[j],a);out->hi[j]=fmax(out->hi[j],a);}
            }
            for(int i=1;i<n-1;i++){GrabTriangle *t=out->triangles+out->count++;memcpy(t->v[0],points[0],sizeof points[0]);memcpy(t->v[1],points[i],sizeof points[i]);memcpy(t->v[2],points[i+1],sizeof points[i+1]);}
        }
    }return 1;
}
static int grab_separated(const GrabTriangle *a,const GrabTriangle *b,const double axis[3]){
    if(grab_dot(axis,axis)<1e-16)return 0;
    double al=1e30,ah=-1e30,bl=1e30,bh=-1e30;
    for(int i=0;i<3;i++){double x=grab_dot(a->v[i],axis),y=grab_dot(b->v[i],axis);al=fmin(al,x);ah=fmax(ah,x);bl=fmin(bl,y);bh=fmax(bh,y);}
    double tolerance=1e-6*sqrt(grab_dot(axis,axis));return ah<bl-tolerance || bh<al-tolerance;
}
static int grab_triangles_touch(const GrabTriangle *a,const GrabTriangle *b){
    double ea[3][3],eb[3][3],na[3],nb[3],axis[3];
    for(int i=0;i<3;i++)for(int j=0;j<3;j++){ea[i][j]=a->v[(i+1)%3][j]-a->v[i][j];eb[i][j]=b->v[(i+1)%3][j]-b->v[i][j];}
    grab_cross(ea[0],ea[1],na);grab_cross(eb[0],eb[1],nb);
    if(grab_dot(na,na)<1e-16 || grab_dot(nb,nb)<1e-16)return 0;
    if(grab_separated(a,b,na) || grab_separated(a,b,nb))return 0;
    for(int i=0;i<3;i++){
        grab_cross(na,ea[i],axis);if(grab_separated(a,b,axis))return 0;
        grab_cross(nb,eb[i],axis);if(grab_separated(a,b,axis))return 0;
        for(int j=0;j<3;j++){grab_cross(ea[i],eb[j],axis);if(grab_separated(a,b,axis))return 0;}
    }return 1;
}
static int grab_inside(const GrabMesh *mesh,const double point[3]){
    double angle=0;
    for(size_t i=0;i<mesh->count;i++){
        double a[3],b[3],c[3],cross[3];for(int j=0;j<3;j++){a[j]=mesh->triangles[i].v[0][j]-point[j];b[j]=mesh->triangles[i].v[1][j]-point[j];c[j]=mesh->triangles[i].v[2][j]-point[j];}
        double la=length(a),lb=length(b),lc=length(c);if(la<1e-6 || lb<1e-6 || lc<1e-6)return 1;
        grab_cross(b,c,cross);angle+=2*atan2(grab_dot(a,cross),la*lb*lc+grab_dot(a,b)*lc+grab_dot(b,c)*la+grab_dot(c,a)*lb);
    }return fabs(angle)>2*PI;
}
static int grab_overlap(const GrabMesh *a,const GrabMesh *b){
    for(int j=0;j<3;j++)if(a->hi[j]<b->lo[j] || b->hi[j]<a->lo[j])return 0;
    for(size_t i=0;i<a->count;i++)for(size_t j=0;j<b->count;j++)if(grab_triangles_touch(a->triangles+i,b->triangles+j))return 1;
    return grab_inside(a,b->triangles[0].v[0]) || grab_inside(b,a->triangles[0].v[0]);
}
static int grab_hand(const TombVr *v,const TombPlaytest *p,int h,GrabMesh *out){
    uint16_t indices[15];if(!p->visual || !v->input.hand[h].valid || !v->input.aim[h].valid || !tomb_visual_lara_meshes(p->visual,v->gym_hands,indices))return 0;
    double matrix[16];const TombMeshView *mesh=p->visual->meshes+indices[h?10:13];tomb_vr_hand_matrix(v,h,matrix);matrix[13]+=v->view_offset;return grab_mesh(mesh,matrix,out);
}
int tomb_vr_pickup_contact(const TombVr *v,const TombPlaytest *p,int h,const TombObject *o){
    int16_t yaw=0;if(p->objects)for(size_t i=0;i<p->objects->count;i++)if(o==p->objects->items+i){if(i<p->level->item_count)yaw=p->level->items[i].yaw;break;}
    TombVrPickupPose pose;if(!tomb_vr_pickup_pose(v,p,o,yaw,&pose))return 0;
    if(p->objects)for(int k=0;k<2;k++)if(v->hands[k].pickup>=0 && (size_t)v->hands[k].pickup<p->objects->count && p->objects->items+v->hands[k].pickup==o)
        for(unsigned i=0;i<pose.model.mesh_count;i++)pose.matrix[i][13]+=v->view_offset;
    GrabMesh hand;if(!grab_hand(v,p,h,&hand))return 0;int hit=0;
    for(unsigned i=0;i<pose.model.mesh_count && !hit;i++){
        GrabMesh item;if(grab_mesh(p->visual->meshes+pose.model.mesh_start+i,pose.matrix[i],&item)){hit=grab_overlap(&hand,&item);free(item.triangles);}
    }free(hand.triangles);return hit;
}
int tomb_vr_key_contact(const TombVr *v,const TombPlaytest *p,int h,int owner){
    TombModel model;double matrix[16];if(!tomb_vr_key_matrix(v,p,owner,&model,matrix))return 0;
    matrix[13]+=v->view_offset;
    GrabMesh hand,item;if(!grab_hand(v,p,h,&hand))return 0;int hit=0;
    if(grab_mesh(p->visual->meshes+model.mesh_start,matrix,&item)){hit=grab_overlap(&hand,&item);free(item.triangles);}
    free(hand.triangles);return hit;
}
