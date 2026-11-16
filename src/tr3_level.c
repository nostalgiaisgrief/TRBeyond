#include "tr3_level.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint16_t tr3_u16(const uint8_t *p){return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
int16_t tr3_i16(const uint8_t *p){uint16_t n=tr3_u16(p);return (int16_t)(n<32768?n:(int)n-65536);}
uint32_t tr3_u32(const uint8_t *p){return tr3_u16(p)|(uint32_t)tr3_u16(p+2)<<16;}
int32_t tr3_i32(const uint8_t *p){uint32_t n=tr3_u32(p);return (int32_t)(n<=INT32_MAX?n:(int64_t)n-4294967296LL);}
typedef struct {const uint8_t *bytes;size_t size,pos;int bad;} Reader;
static const uint8_t zero[32]={0};
static const uint8_t *take(Reader *r,size_t count,size_t width){
    if(r->bad || !width || count>(r->size-r->pos)/width){r->bad=1;return zero;}
    const uint8_t *p=r->bytes+r->pos;r->pos+=count*width;return p;
}
static uint32_t count(Reader *r,int width){const uint8_t *p=take(r,1,(size_t)width);return width==2?tr3_u16(p):tr3_u32(p);}
static Tr3Span span(Reader *r,int prefix,int width){uint32_t n=count(r,prefix);const uint8_t *p=take(r,n,(size_t)width);return (Tr3Span){p,r->bad?0:n};}
void tr3_free(Tr3Level *l){if(l){free(l->rooms);free(l->bytes);free(l);}}
const uint8_t *tr3_model(const Tr3Level *l,uint32_t id){for(uint32_t i=0;i<l->models.count;i++){const uint8_t *p=l->models.data+i*18;if(tr3_u32(p)==id)return p;}return NULL;}

Tr3Level *tr3_load(const char *path,char *error,size_t cap){
    Tr3Level *l=NULL;FILE *f=fopen(path,"rb");const char *why="Cannot read level";
    if(!f)goto fail;
    if(fseek(f,0,SEEK_END))goto fail;
    long size=ftell(f);if(size<1800 || size>256*1024*1024 || fseek(f,0,SEEK_SET))goto fail;
    l=calloc(1,sizeof *l);if(!l)goto fail;l->lara=-1;l->size=(size_t)size;l->bytes=malloc(l->size);
    if(!l->bytes || fread(l->bytes,1,l->size,f)!=l->size)goto fail;
    fclose(f);f=NULL;Reader r={l->bytes,l->size,0,0};
    why="Invalid or truncated TR3 asset";
    uint32_t version=count(&r,4);if(version!=0xff180038 && version!=0xff080038 && version!=0xff180034)goto fail;
    l->palette=take(&r,256,3);l->palette16=take(&r,256,4);l->tile_count=count(&r,4);
    if(!l->tile_count || l->tile_count>256)goto fail;
    take(&r,l->tile_count,65536);l->tiles16=take(&r,l->tile_count,131072);
    /* VICT.TR2 on the supplied disc is a version-52 texture-only archive.
       Its exact file length ends after the 16-bit tiles, with no room block. */
    if(version==0xff180034){if(r.bad || r.pos!=r.size)goto fail;l->parsed=r.pos;return l;}
    take(&r,1,4);
    l->room_count=count(&r,2);if(r.bad || !l->room_count || l->room_count>1024)goto fail;
    l->rooms=calloc(l->room_count,sizeof *l->rooms);if(!l->rooms)goto fail;
    for(uint32_t i=0;i<l->room_count;i++){
        Tr3Room *room=l->rooms+i;const uint8_t *p=take(&r,1,16);
        room->x=tr3_i32(p);room->z=tr3_i32(p+4);room->bottom=tr3_i32(p+8);room->top=tr3_i32(p+12);
        uint32_t words=count(&r,4);const uint8_t *geometry=take(&r,words,2);if(r.bad)goto fail;
        Reader g={geometry,(size_t)words*2,0,0};
        room->vertices=span(&g,2,12);room->quads=span(&g,2,10);room->triangles=span(&g,2,8);room->sprites=span(&g,2,4);
        if(g.bad || g.pos!=g.size)goto fail;
        room->portals=span(&r,2,32);room->nz=(uint16_t)count(&r,2);room->nx=(uint16_t)count(&r,2);
        room->sectors.count=(uint32_t)room->nz*room->nx;room->sectors.data=take(&r,room->sectors.count,8);
        p=take(&r,1,4);room->ambient=tr3_i16(p);room->light_mode=tr3_i16(p+2);
        room->lights=span(&r,2,24);room->statics=span(&r,2,20);
        p=take(&r,1,7);room->alternate=tr3_i16(p);room->flags=tr3_u16(p+2);
        room->ambient2=p[4]; /* Original MeshEffect water scheme; spare native field. */
        if(r.bad)goto fail;
    }
    l->floors=span(&r,4,2);l->mesh_data=span(&r,4,2);l->mesh_offsets=span(&r,4,4);
    l->animations=span(&r,4,32);l->changes=span(&r,4,6);l->ranges=span(&r,4,8);l->commands=span(&r,4,2);
    l->trees=span(&r,4,4);l->frames=span(&r,4,2);l->models=span(&r,4,18);l->statics=span(&r,4,32);
    l->sprites=span(&r,4,16);l->sequences=span(&r,4,8);
    l->cameras=span(&r,4,16);l->sounds=span(&r,4,16);l->boxes=span(&r,4,8);l->overlaps=span(&r,4,2);
    l->zones.count=l->boxes.count*10;l->zones.data=take(&r,l->zones.count,2);
    l->animated_textures=span(&r,4,2);l->textures=span(&r,4,20);l->items=span(&r,4,24);
    take(&r,8192,1);l->cinematic_frames=span(&r,2,16);span(&r,2,1);
    l->sound_map=take(&r,370,2);l->sound_details=span(&r,4,8);l->sample_indices=span(&r,4,4);
    if(r.bad || r.pos!=r.size)goto fail;
    l->parsed=r.pos;
    for(uint32_t i=0;i<l->items.count;i++)if(tr3_u16(l->items.data+i*24)==0){l->lara=(int)i;break;}
    if(!tr3_validate(l,error,cap)){tr3_free(l);return NULL;}
    return l;
fail:
    if(f)fclose(f);tr3_free(l);if(error && cap)snprintf(error,cap,"%s: %s",why,path);return NULL;
}

static int faces_valid(Tr3Span faces,int n,uint32_t vertices,uint32_t textures){
    for(uint32_t i=0;i<faces.count;i++){const uint8_t *p=faces.data+i*(n+1)*2;
        for(int v=0;v<n;v++)if(tr3_u16(p+v*2)>=vertices)return 0;
        if((tr3_u16(p+n*2)&0x7fff)>=textures)return 0;
    }return 1;
}
int tr3_validate(const Tr3Level *l,char *error,size_t cap){
    const char *why="Invalid level reference";
    for(uint32_t i=0;i<l->textures.count;i++)if((tr3_u16(l->textures.data+i*20+2)&0x7fff)>=l->tile_count)goto fail;
    for(uint32_t i=0;i<l->room_count;i++){
        const Tr3Room *r=l->rooms+i;
        if(!faces_valid(r->quads,4,r->vertices.count,l->textures.count) || !faces_valid(r->triangles,3,r->vertices.count,l->textures.count))goto fail;
        if(r->alternate < -1 || r->alternate >= (int)l->room_count)goto fail;
        for(uint32_t j=0;j<r->portals.count;j++)if(tr3_u16(r->portals.data+j*32)>=l->room_count)goto fail;
        for(uint32_t j=0;j<r->sectors.count;j++){
            const uint8_t *p=r->sectors.data+j*8;
            if(tr3_u16(p)>=l->floors.count || (p[4]!=255 && p[4]>=l->room_count) || (p[6]!=255 && p[6]>=l->room_count))goto fail;
        }
    }
    for(uint32_t i=0;i<l->mesh_offsets.count;i++){
        uint32_t offset=tr3_u32(l->mesh_offsets.data+i*4);
        if(offset>=l->mesh_data.count*2)goto fail;
        Reader r={l->mesh_data.data,l->mesh_data.count*2,offset,0};
        take(&r,1,10);Tr3Span vertices=span(&r,2,6);
        int normals=tr3_i16(take(&r,1,2));take(&r,(size_t)(normals<0?-normals:normals),normals<0?2:6);
        Tr3Span q=span(&r,2,10),t=span(&r,2,8);span(&r,2,10);span(&r,2,8);
        if(r.bad || !faces_valid(q,4,vertices.count,l->textures.count) || !faces_valid(t,3,vertices.count,l->textures.count))goto fail;
    }
    for(uint32_t i=0;i<l->models.count;i++){
        const uint8_t *p=l->models.data+i*18;uint32_t n=tr3_u16(p+4),start=tr3_u16(p+6);
        if(start>l->mesh_offsets.count || n>l->mesh_offsets.count-start)goto fail;
        if(n>1 && (uint64_t)tr3_u32(p+8)+(n-1)*4>l->trees.count)goto fail;
    }
    for(uint32_t i=0;i<l->items.count;i++)if(tr3_u16(l->items.data+i*24+2)>=l->room_count)goto fail;
    return 1;
fail:if(error && cap)snprintf(error,cap,"%s",why);return 0;
}

