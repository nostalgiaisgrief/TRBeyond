#include "visual.h"
#include <string.h>
#include "fixed.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct Read { const unsigned char *p; size_t n,at; int bad; } Read;
static void section(Read *r,const TombLevel *l,int id) {
    if(!l->asset.version)return;
    if(r->bad || (r->at && r->at!=r->n)){r->bad=1;return;}
    TombAssetSpan span=l->asset.span[id];r->at=span.offset;r->n=span.offset+span.size;
}
static uint16_t u16(const unsigned char *p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }
static uint32_t u32(const unsigned char *p) { return u16(p)|(uint32_t)u16(p+2)<<16; }
int tomb_visual_water_face(const TombVisual *v,size_t room,const unsigned char *face,int n){
    if(room>=v->room_count || (n!=3 && n!=4))return 0;
    unsigned texture=u16(face+2*n)&32767;
    if(texture>=v->texture_count || !u16(v->textures+20*texture))return 0;
    const TombMeshView *m=v->rooms+room;int x=0,z=0,y=0;
    for(int j=0;j<n;j++){
        unsigned index=u16(face+2*j);if(index>=m->vertex_count)return 0;
        const unsigned char *p=m->vertices+index*m->vertex_stride;
        int py=(int16_t)u16(p+2);if(j && y!=py)return 0;y=py;
        x+=(int16_t)u16(p);z+=(int16_t)u16(p+4);
    }
    for(size_t k=0;k<v->portals[room].count;k++){
        const unsigned char *p=v->portals[room].data+32*k;unsigned other=u16(p);
        if(other>=v->level->room_count || !((v->level->rooms[room].flags^v->level->rooms[other].flags)&1))continue;
        int xmin=32767,xmax=-32768,zmin=32767,zmax=-32768,flat=1;
        for(int j=0;j<4;j++){
            int px=(int16_t)u16(p+8+6*j),py=(int16_t)u16(p+10+6*j),pz=(int16_t)u16(p+12+6*j);
            /* Opposite portal faces may be inset by one world unit. */
            if(abs(py-y)>1)flat=0;if(px<xmin)xmin=px;if(px>xmax)xmax=px;if(pz<zmin)zmin=pz;if(pz>zmax)zmax=pz;
        }
        if(flat && x>=xmin*n && x<=xmax*n && z>=zmin*n && z<=zmax*n)return 1;
    }
    return 0;
}
static const unsigned char *take(Read *r,size_t n,size_t size)
{
    if(r->bad || !size || n>(r->n-r->at)/size) { r->bad=1; return NULL; }
    const unsigned char *p=r->p+r->at; r->at+=n*size; return p;
}
static uint32_t count(Read *r,int width)
{ const unsigned char *p=take(r,1,(size_t)width); return p?(width==2?u16(p):u32(p)):0; }
static int mesh(Read *r,TombMeshView *m,int room)
{
    if(!room) {
        const unsigned char *p=take(r,1,10); if(!p) return 0;
        for(int i=0;i<3;++i) m->centre[i]=tomb_word(u16(p+i*2));
        m->radius=tomb_long(u32(p+6));
    }
    m->vertex_count=count(r,2); if(m->vertex_count>32767) return 0;
    m->vertex_stride=room?8:6; m->vertices=take(r,m->vertex_count,m->vertex_stride);
    if(!room) {
        int32_t n=tomb_word(count(r,2));
        if(n>0) { m->normal_count=(uint32_t)n; m->normals=take(r,m->normal_count,6); }
        else { m->light_count=(uint32_t)-n; m->lights=take(r,m->light_count,2); }
    }
    for(int group=0;group<(room?2:4);++group) {
        m->face_count[group]=count(r,2); if(m->face_count[group]>32767) return 0;
        m->faces[group]=take(r,m->face_count[group],(group&1)?8:10);
    }
    if(room) { m->sprite_count=count(r,2); m->sprites=take(r,m->sprite_count,4); }
    return !r->bad;
}
static int validate_mesh(const TombVisual *v,const TombMeshView *m)
{
    for(int group=0;group<4;++group) for(size_t i=0;i<m->face_count[group];++i) {
        size_t vertices=(group&1)?3:4; const unsigned char *p=m->faces[group]+i*(vertices+1)*2;
        for(size_t j=0;j<vertices;++j) if(u16(p+2*j)>=m->vertex_count) return 0;
        if(group<2 && (u16(p+2*vertices)&32767)>=v->texture_count) return 0;
    }
    for(size_t i=0;i<m->sprite_count;++i) {
        if(u16(m->sprites+i*4)>=m->vertex_count || u16(m->sprites+i*4+2)>=v->sprite_texture_count) return 0;
    }
    return 1;
}
void tomb_visual_free(TombVisual *v)
{ if(v) { free(v->lighting); free(v->portals); free(v->rooms); free(v->meshes); free(v); } }
int tomb_visual_model(const TombVisual *v,uint32_t id,TombModel *out)
{
    if(!v || !out) return 0;
    /* DOS SetupObjects shares the winged model/animations with the two
       ground variants; their one-mesh PHD records are placeholders. */
    if(id==21 || id==22){TombModel shared;if(tomb_visual_model(v,20,&shared)){*out=shared;out->id=id;return 1;}}
    for(size_t i=0;i<v->model_count;++i) {
        const unsigned char *p=v->models+i*18;
        if(u32(p)==id) { *out=(TombModel){id,u32(p+8),u32(p+12),u16(p+4),u16(p+6),u16(p+16)}; return 1; }
    }
    return 0;
}
int tomb_visual_key(const TombVisual *v,size_t animation,size_t key,TombPose *out)
{
    if(!v || !out || animation>=v->animation_count) return 0;
    const unsigned char *a=v->animations+animation*32;
    size_t off=u32(a); int rate=a[4],first=tomb_word(u16(a+16)),last=tomb_word(u16(a+18));
    if(!rate || last<first || key>(size_t)((last-first+rate-1)/rate) || (off&1) || off>v->frame_words*2) return 0;
    if(v->frame_words*2-off<20) return 0;
    size_t n=u16(v->frames+off+18),stride=20+4*n;
    if(n>64 || key>(v->frame_words*2-off)/stride) return 0;
    off+=key*stride;
    if(v->frame_words*2-off<stride || u16(v->frames+off+18)!=n) return 0;
    const unsigned char *p=v->frames+off; TombPose pose={0}; pose.count=(uint16_t)n;
    for(int i=0;i<6;++i) pose.bounds[i]=tomb_word(u16(p+2*i));
    for(int i=0;i<3;++i) pose.root[i]=tomb_word(u16(p+12+2*i));
    for(size_t i=0;i<n;++i) {
        uint32_t angles=u32(p+20+4*i);
        pose.rotation[i][0]=(uint16_t)(((angles>>20)&1023)<<6);
        pose.rotation[i][1]=(uint16_t)(((angles>>10)&1023)<<6);
        pose.rotation[i][2]=(uint16_t)((angles&1023)<<6);
    }
    *out=pose; return 1;
}
int tomb_visual_skateboard_key(const TombVisual *v,size_t animation,int key,TombPose *out){
    /* DOS 3376c retains the rider's absolute frame when selecting the board
       track. GetFrames (1d380) consequently reads across track boundaries.
       Preserve that addressing, bounded to the loaded frame allocation. */
    if(!v || !out || animation>=v->animation_count)return 0;
    int64_t offset=(int64_t)u32(v->animations+animation*32)+(int64_t)key*24;
    if(offset<0 || (uint64_t)offset+24>v->frame_words*2)return 0;
    const unsigned char *p=v->frames+(size_t)offset;TombPose pose={0};pose.count=1;
    for(int i=0;i<6;i++)pose.bounds[i]=tomb_word(u16(p+2*i));
    for(int i=0;i<3;i++)pose.root[i]=tomb_word(u16(p+12+2*i));
    uint32_t angles=u32(p+20);pose.rotation[0][0]=(uint16_t)(((angles>>20)&1023)<<6);pose.rotation[0][1]=(uint16_t)(((angles>>10)&1023)<<6);pose.rotation[0][2]=(uint16_t)((angles&1023)<<6);
    *out=pose;return 1;
}
int tomb_visual_lara_meshes(const TombVisual *v,int gym,uint16_t indices[15])
{
    TombModel m;
    if(!indices || !tomb_visual_model(v,gym?5:0,&m) || m.mesh_count!=15 || (size_t)m.mesh_start+15>v->mesh_count) return 0;
    uint16_t selected[15];
    for(int i=0;i<15;++i) selected[i]=(uint16_t)(m.mesh_start+i);
    if(gym) {
        TombModel standard;
        if(!tomb_visual_model(v,0,&standard) || standard.mesh_count!=15 || (size_t)standard.mesh_start+15>v->mesh_count) return 0;
        selected[14]=(uint16_t)(standard.mesh_start+14);
    }
    for(int i=0;i<15;++i) indices[i]=selected[i];
    return 1;
}
TombVisual *tomb_visual_load(const TombLevel *l,char *error,size_t cap)
{
    TombVisual *v=NULL; const char *why="invalid visual asset data";
    if(error && cap) error[0]=0;
    if(!l || !l->file_data) goto fail;
    v=calloc(1,sizeof *v); if(!v) goto fail; v->level=l;
    Read r={l->file_data,l->file_size,0,0};
    if(!l->asset.version && count(&r,4)!=32) goto fail;
    section(&r,l,BA_TEX8);
    v->tile_count=count(&r,4); v->tiles=take(&r,v->tile_count,65536); if(!l->asset.version)take(&r,1,4);
    section(&r,l,BA_ROOM);
    v->room_count=count(&r,2); if(r.bad || v->room_count!=l->room_count) goto fail;
    v->rooms=calloc(v->room_count?v->room_count:1,sizeof *v->rooms); if(!v->rooms) goto fail;
    v->portals=calloc(v->room_count?v->room_count:1,sizeof *v->portals);if(!v->portals)goto fail;
    v->lighting=calloc(v->room_count?v->room_count:1,sizeof *v->lighting);if(!v->lighting)goto fail;
    for(size_t i=0;i<v->room_count;++i) {
        take(&r,1,16); size_t words=count(&r,4); const unsigned char *p=take(&r,words,2); if(!p) goto fail;
        Read data={p,words*2,0,0}; if(!mesh(&data,v->rooms+i,1) || data.at!=data.n) goto fail;
        size_t n=count(&r,2); v->portals[i].count=n;v->portals[i].data=take(&r,n,32);
        if(r.bad)goto fail;
        for(size_t j=0;j<n;j++)if(u16(v->portals[i].data+j*32)>=v->room_count)goto fail;
        size_t nz=count(&r,2),nx=count(&r,2); take(&r,nz*nx,8);
        v->lighting[i].ambient=tomb_word(count(&r,2));
        n=count(&r,2);v->lighting[i].count=n;v->lighting[i].data=take(&r,n,18);
        n=count(&r,2); take(&r,n,18); take(&r,1,4);
    }
    section(&r,l,BA_FLOR);
    size_t n=count(&r,4); take(&r,n,2);
    section(&r,l,BA_MESH);
    size_t mesh_words=count(&r,4); const unsigned char *mesh_data=take(&r,mesh_words,2);
    v->mesh_count=count(&r,4); const unsigned char *pointers=take(&r,v->mesh_count,4); if(!pointers) goto fail;
    v->meshes=calloc(v->mesh_count?v->mesh_count:1,sizeof *v->meshes); if(!v->meshes) goto fail;
    for(size_t i=0;i<v->mesh_count;++i) {
        size_t off=u32(pointers+4*i),end=mesh_words*2;
        if((off&1) || off>=end) goto fail;
        for(size_t j=0;j<v->mesh_count;++j) { size_t next=u32(pointers+4*j); if(next>off && next<end) end=next; }
        Read data={mesh_data+off,end-off,0,0}; if(!mesh(&data,v->meshes+i,0)) goto fail;
    }
    section(&r,l,BA_ANIM);
    v->animation_count=count(&r,4); v->animations=take(&r,v->animation_count,32);
    n=count(&r,4); take(&r,n,6); n=count(&r,4); take(&r,n,8); n=count(&r,4); take(&r,n,2);
    section(&r,l,BA_MODL);
    v->tree_words=count(&r,4); v->trees=take(&r,v->tree_words,4);
    v->frame_words=count(&r,4); v->frames=take(&r,v->frame_words,2);
    v->model_count=count(&r,4); v->models=take(&r,v->model_count,18);
    n=count(&r,4); take(&r,n,32);
    section(&r,l,BA_TEXI);
    v->texture_count=count(&r,4); v->textures=take(&r,v->texture_count,20);
    v->sprite_texture_count=count(&r,4); v->sprite_textures=take(&r,v->sprite_texture_count,16);
    if(r.bad || l->item_parsed_bytes>l->file_size) goto fail;
    n=count(&r,4);take(&r,n,8); /* sprite sequences */
    section(&r,l,BA_WRLD);
    n=count(&r,4);take(&r,n,16); /* cameras */
    n=count(&r,4);take(&r,n,16); /* sound sources */
    size_t boxes=count(&r,4);take(&r,boxes,20);
    n=count(&r,4);take(&r,n,2);take(&r,boxes,12);
    v->texture_cycle_words=count(&r,4);v->texture_cycles=take(&r,v->texture_cycle_words,2);
    if(r.bad || !v->texture_cycle_words)goto fail;
    Read cycles={v->texture_cycles,v->texture_cycle_words*2,0,0};
    unsigned sequences=count(&cycles,2);
    for(unsigned i=0;i<sequences;i++){
        unsigned length=count(&cycles,2)+1;
        for(unsigned j=0;j<length;j++)if(count(&cycles,2)>=v->texture_count)goto fail;
    }
    if(cycles.bad || cycles.at!=cycles.n)goto fail;
    if(l->asset.version){n=count(&r,4);take(&r,n,22);section(&r,l,BA_PALT);}
    r.at=l->item_parsed_bytes; take(&r,1,8192); v->palette=take(&r,256,3); if(r.bad) goto fail;
    /* DOS palette loader 1f078..1f084 forces index zero to black. */
    memcpy(v->display_palette,v->palette,768);memset(v->display_palette,0,3);v->palette=v->display_palette;
    if(l->asset.edition==2){
        TombAssetSpan span=l->asset.span[BA_TEXC];Read pixels={l->file_data,span.offset+span.size,span.offset,0};
        v->tile_count=count(&pixels,4);v->rgba_tiles=take(&pixels,v->tile_count,256*256*4);
        if(pixels.bad || pixels.at!=pixels.n || !v->tile_count)goto fail;
    }
    for(size_t i=0;i<v->texture_count;++i) if((u16(v->textures+i*20+2)&32767)>=v->tile_count) goto fail;
    for(size_t i=0;i<v->sprite_texture_count;++i) if(u16(v->sprite_textures+i*16)>=v->tile_count) goto fail;
    for(size_t i=0;i<l->static_def_count;++i) if(l->static_defs[i].mesh>=v->mesh_count) goto fail;
    for(size_t i=0;i<v->room_count;++i) if(!validate_mesh(v,v->rooms+i)) goto fail;
    for(size_t i=0;i<v->mesh_count;++i) if(!validate_mesh(v,v->meshes+i)) goto fail;
    for(size_t i=0;i<v->model_count;++i) {
        const unsigned char *p=v->models+i*18;
        size_t meshes=u16(p+4),start=u16(p+6),tree=u32(p+8);
        if(start>v->mesh_count || meshes>v->mesh_count-start || tree>v->tree_words ||
           (meshes && meshes-1>(v->tree_words-tree)/4)) goto fail;
        /* Unanimated models may point to the end of the frame block (Lost Valley). */
        if((u16(p+16)!=65535?u32(p+12)>=v->frame_words*2:u32(p+12)>v->frame_words*2) || (u32(p+12)&1)) goto fail;
    }
    for(size_t i=0;i<v->animation_count;++i) {
        const unsigned char *p=v->animations+i*32; int rate=p[4];
        int first=tomb_word(u16(p+16)),last=tomb_word(u16(p+18)); if(!rate || last<first) goto fail;
        size_t keys=(size_t)((last-first+rate-1)/rate)+1;
        for(size_t k=0;k<keys;++k) { TombPose pose; if(!tomb_visual_key(v,i,k,&pose)) goto fail; }
    }
    return v;
fail:
    tomb_visual_free(v); if(error && cap) snprintf(error,cap,"%s",why); return NULL;
}

/* Original bounds interpolation 0x1d380 / 0x1d444, for Lara's model. */
int tomb_visual_bounds(const TombVisual *v,const TombActor *actor,int16_t out[6]) {
    return tomb_visual_item_bounds(v,0,actor,out);
}
int tomb_visual_item_bounds(const TombVisual *v,int object,const TombActor *actor,int16_t out[6]) {
    if(!v || !actor || !out || actor->animation<0 || (size_t)actor->animation>=v->animation_count) return 0;
    TombModel model;int meshes;
    if(tomb_visual_model(v,(uint32_t)object,&model))meshes=model.mesh_count;
    else {
        /* Sprite objects still have an animation/frame for GetBoundsAccurate.
           Their negative mesh count comes from the sprite-sequence record. */
        const unsigned char *p=v->sprite_textures+16*v->sprite_texture_count,*end=v->level->file_data+v->level->file_size;
        if(p+4>end)return 0;size_t n=u32(p);p+=4;if(n>(size_t)(end-p)/8)return 0;
        meshes=0;for(size_t i=0;i<n;i++,p+=8)if(u32(p)==(uint32_t)object){meshes=tomb_word(u16(p+4));break;}
        if(meshes>=0 || meshes<=-5)return 0;
    }
    const unsigned char *a=v->animations+(size_t)actor->animation*32;
    int rate=tomb_word(u16(a+4)),first=tomb_word(u16(a+16)),last=tomb_word(u16(a+18));
    if(rate<=0 || actor->frame<first || actor->frame>last) return 0;
    int frame=actor->frame-first,key=frame/rate,fraction=frame%rate,denominator=rate;
    size_t stride=(size_t)(20+meshes*4),base=u32(a),size=v->frame_words*2;
    if(base>size || (size_t)key>(size-base)/stride) return 0;
    base+=(size_t)key*stride;
    if(size-base<12 || (fraction && (size-base<stride || size-base-stride<12))) return 0;
    /* Preserve the original comparison with absolute last_frame. */
    if(fraction && (key+1)*rate>last) denominator=last-key*rate;
    if(denominator<=0) return 0;
    int16_t result[6];
    for(int i=0;i<6;i++) {
        int start=tomb_word(u16(v->frames+base+2*i));
        int end=fraction?tomb_word(u16(v->frames+base+stride+2*i)):start;
        result[i]=tomb_word(start+(end-start)*fraction/denominator);
    }
    for(int i=0;i<6;i++) out[i]=result[i];
    return 1;
}

/* Original 0x10a00 rotates descriptors left after strictly more than ten
   60-Hz units; one fixed gameplay tick contributes two units. */
unsigned tomb_visual_texture(const TombVisual *v,unsigned texture,uint32_t ticks){
    if(!v || !v->texture_cycles || !ticks)return texture;
    uint32_t steps=(uint32_t)(((uint64_t)ticks*2-1)/10);
    const unsigned char *p=v->texture_cycles;unsigned sequences=u16(p);p+=2;
    for(unsigned i=0;i<sequences;i++){
        unsigned length=u16(p)+1;p+=2;
        for(unsigned j=0;j<length;j++)if(u16(p+2*j)==texture)return u16(p+2*((j+steps%length)%length));
        p+=length*2;
    }
    return texture;
}
void tomb_water_effect_init(TombWaterEffect *w,const int16_t *sine,uint32_t seed){
    for(unsigned i=0;i<32;i++){
        unsigned angle=i*2048,quadrant=angle>>14,index=(angle&16383)>>4;
        int sn=sine[(quadrant&1)?1024-index:index];if(quadrant>=2)sn=-sn;
        w->wave[i]=tomb_asr(sn*2,14);w->light[i]=tomb_asr(sn*768,14);
        seed=seed*UINT32_C(1103515245)+12345;
        w->phase[i]=(int)((seed>>10)&32767)/32-511;
    }
}
int tomb_water_effect_shade(const TombWaterEffect *w,unsigned remaining,uint32_t ticks,int shade){
    shade=tomb_word(shade+w->light[((uint32_t)w->phase[remaining&31]+ticks)&31]);
    return shade<0?0:shade>8191?8191:shade;
}
void tomb_water_effect_offset(const TombWaterEffect *w,uint32_t ticks,int x,int y,int *dx,int *dy){
    *dx=w->wave[(ticks+(uint32_t)y)&31];
    *dy=w->wave[(ticks+(uint32_t)(x+*dx))&31];
}

static int water_sine(const int16_t *s,int angle){
    unsigned a=(unsigned)angle&65535;int negative=a>=32768;if(negative)a-=32768;if(a>16384)a=32768-a;
    return negative?-s[a>>4]:s[a>>4];
}
/* DOS splash 0x1dcc4 and bubble movement prefix 0x1dad0. The caller resolves
   bubble room/ceiling contact after this provisional movement. */
void tomb_water_particle_step(TombWaterParticle *p,int frames,const int16_t *s){
    if(!p->active)return;
    if(p->id==153){
        if(++p->frame>=frames){p->active=0;return;}
        p->x+=tomb_asr(p->speed*water_sine(s,p->yaw),14);
        p->z+=tomb_asr(p->speed*water_sine(s,p->yaw+16384),14);
    }else if(p->id==155){
        p->yaw=tomb_word(p->yaw+1638);p->pitch=tomb_word(p->pitch+2366);
        p->x+=tomb_asr(11*water_sine(s,p->yaw),14);p->y-=p->speed;
        p->z+=tomb_asr(8*water_sine(s,p->pitch+16384),14);
    }
}

void tomb_shadow_shape(const int16_t b[6],int size,int16_t p[8][3]){
    int x=(b[0]+b[1])/2,z=(b[4]+b[5])/2,dx=(b[1]-b[0])*size/1024,dz=(b[5]-b[4])*size/1024;
    static const int8_t offsets[8][2]={{-1,2},{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1}};
    for(int i=0;i<8;i++){p[i][0]=tomb_word(x+offsets[i][0]*dx);p[i][1]=0;p[i][2]=tomb_word(z+offsets[i][1]*dz);}
}
int tomb_sprite_shade(int16_t intensity,int depth){
    int shade=intensity;if(depth>12288){shade=tomb_word(shade+depth-12288);if(shade>8191)return -1;}
    return shade<0?0:shade;
}
