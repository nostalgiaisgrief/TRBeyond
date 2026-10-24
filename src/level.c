#include "level.h"
#include "fixed.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Reader { const unsigned char *data; size_t size,pos; int failed; } Reader;
static const unsigned char *take(Reader *r,size_t count,size_t width)
{
    if (r->failed || !width || count>(r->size-r->pos)/width) { r->failed=1; return NULL; }
    const unsigned char *p=r->data+r->pos; r->pos+=count*width; return p;
}
static void section(Reader *r,const TombLevel *l,int id) {
    if(!l->asset.version)return;
    if(r->failed || (r->pos && r->pos!=r->size)){r->failed=1;return;}
    TombAssetSpan span=l->asset.span[id];r->pos=span.offset;r->size=span.offset+span.size;
}
static uint16_t u16(const unsigned char *p) { return (uint16_t)((uint16_t)p[0]|(uint16_t)p[1]<<8); }
static uint32_t u32(const unsigned char *p) { return (uint32_t)u16(p)|(uint32_t)u16(p+2)<<16; }
static int16_t s16(const unsigned char *p) { return tomb_word(u16(p)); }
static int32_t s32(const unsigned char *p) { return tomb_long(u32(p)); }
static int8_t s8(unsigned char n) { return (int8_t)(n<128?(int)n:(int)n-256); }
static uint32_t read_count(Reader *r,int bytes)
{
    const unsigned char *p=take(r,1,(size_t)bytes);
    return !p?0:bytes==2?u16(p):u32(p);
}
void tomb_level_free(TombLevel *l)
{
    if (!l) return;
    if (l->rooms) for(size_t i=0;i<l->room_count;++i) {
        free(l->rooms[i].sectors); free(l->rooms[i].statics);
    }
    free(l->rooms); free(l->floor_data); free(l->animations); free(l->changes);
    free(l->ranges); free(l->commands); free(l->static_defs); free(l->items); free(l->cameras); free(l->boxes); free(l->overlaps); free(l->zones); free(l->file_data); free(l);
}
TombLevel *tomb_level_load(const char *path,char *error,size_t cap)
{
    const char *why="invalid or truncated TR1 data";
    TombLevel *l=NULL; FILE *file=NULL;
    if (error && cap) error[0]=0;
    if (!path || !(file=fopen(path,"rb"))) { why="cannot open level file"; goto fail; }
    if (fseek(file,0,SEEK_END)) goto fail;
    long bytes=ftell(file);
    if (bytes<8 || bytes>256L*1024L*1024L || fseek(file,0,SEEK_SET)) goto fail;
    l=calloc(1,sizeof *l); if(!l) goto fail;
    static const char *const names[]={"GYM.PHD","LEVEL1.PHD","LEVEL2.PHD","LEVEL3A.PHD","LEVEL3B.PHD","LEVEL4.PHD","LEVEL5.PHD","LEVEL6.PHD","LEVEL7A.PHD","LEVEL7B.PHD","LEVEL8A.PHD","LEVEL8B.PHD","LEVEL8C.PHD","LEVEL10A.PHD","LEVEL10B.PHD","LEVEL10C.PHD"};
    const char *base=path;for(const char *p=path;*p;p++)if(*p=='/' || *p=='\\')base=p+1;
    char name[32];size_t len=strlen(base);l->dos_level=-1;
    if(len<sizeof name){for(size_t i=0;i<=len;i++)name[i]=base[i]>='a' && base[i]<='z'?(char)(base[i]-'a'+'A'):base[i];
        if(len>4 && !strcmp(name+len-4,".TBL"))memcpy(name+len-4,".PHD",5);
        for(size_t i=0;i<sizeof names/sizeof names[0];i++)if(!strcmp(name,names[i])){l->dos_level=(int)i;break;}}
    l->file_size=(size_t)bytes; l->file_data=malloc(l->file_size);
    if(!l->file_data || fread(l->file_data,1,l->file_size,file)!=l->file_size) goto fail;
    fclose(file); file=NULL;
    int format=tomb_asset_open(&l->asset,l->file_data,l->file_size);
    if(format<0){why="invalid, corrupt or unsupported Beyond level";goto fail;}
    l->music_edition=(int)l->asset.edition;
    Reader r={l->file_data,l->file_size,0,0};
    if(!format && read_count(&r,4)!=32) { why="expected original TR1 PHD version 32"; goto fail; }
    section(&r,l,BA_TEX8);
    uint32_t n=read_count(&r,4); take(&r,n,65536); if(!format)take(&r,1,4);
    section(&r,l,BA_ROOM);
    l->room_count=read_count(&r,2);
    if(r.failed || !l->room_count || l->room_count>32767 || l->room_count>(r.size-r.pos)/28) goto fail;
    l->rooms=calloc(l->room_count,sizeof *l->rooms); if(!l->rooms) goto fail;
    for(size_t i=0;i<l->room_count;++i) {
        TombRoom *room=l->rooms+i;
        const unsigned char *p=take(&r,1,20); if(!p) goto fail;
        room->x=s32(p); room->z=s32(p+4); room->bottom=s32(p+8); room->top=s32(p+12);
        take(&r,u32(p+16),2); n=read_count(&r,2); take(&r,n,32);
        p=take(&r,1,4); if(!p) goto fail;
        room->nz=u16(p); room->nx=u16(p+2);
        if(room->nx<3 || room->nz<3) { why="room sector grid too small for original edge rules"; goto fail; }
        size_t sectors=(size_t)room->nx*room->nz;
        p=take(&r,sectors,8); if(!p) goto fail;
        room->sectors=calloc(sectors,sizeof *room->sectors); if(!room->sectors) goto fail;
        for(size_t j=0;j<sectors;++j,p+=8) room->sectors[j]=(TombSector){u16(p),u16(p+2),p[4],s8(p[5]),p[6],s8(p[7])};
        take(&r,1,2); n=read_count(&r,2); take(&r,n,18);
        room->static_count=read_count(&r,2); p=take(&r,room->static_count,18); if(!p) goto fail;
        if(room->static_count) {
            room->statics=calloc(room->static_count,sizeof *room->statics); if(!room->statics) goto fail;
            for(size_t j=0;j<room->static_count;++j,p+=18)
                room->statics[j]=(TombStaticPlacement){s32(p),s32(p+4),s32(p+8),u16(p+12),u16(p+14),u16(p+16)};
        }
        p=take(&r,1,4); if(!p) goto fail;
        room->alternate=s16(p); room->flags=s16(p+2);
    }
    section(&r,l,BA_FLOR);
    l->floor_count=read_count(&r,4);
    const unsigned char *p=take(&r,l->floor_count,2); if(!p) goto fail;
    l->floor_data=calloc(l->floor_count?l->floor_count:1,sizeof *l->floor_data); if(!l->floor_data) goto fail;
    for(size_t i=0;i<l->floor_count;++i) l->floor_data[i]=u16(p+2*i);
    section(&r,l,BA_MESH);
    n=read_count(&r,4); take(&r,n,2); n=read_count(&r,4); take(&r,n,4);
    section(&r,l,BA_ANIM);
    l->animation_count=read_count(&r,4); p=take(&r,l->animation_count,32); if(!p) goto fail;
    l->animations=calloc(l->animation_count?l->animation_count:1,sizeof *l->animations); if(!l->animations) goto fail;
    for(size_t i=0;i<l->animation_count;++i,p+=32) l->animations[i]=(TombAnim){s32(p+8),s32(p+12),s16(p+6),
        s16(p+16),s16(p+18),s16(p+20),s16(p+22),s16(p+24),s16(p+26),s16(p+28),s16(p+30)};
    l->change_count=read_count(&r,4); p=take(&r,l->change_count,6); if(!p) goto fail;
    l->changes=calloc(l->change_count?l->change_count:1,sizeof *l->changes); if(!l->changes) goto fail;
    for(size_t i=0;i<l->change_count;++i,p+=6) l->changes[i]=(TombChange){s16(p),s16(p+2),s16(p+4)};
    l->range_count=read_count(&r,4); p=take(&r,l->range_count,8); if(!p) goto fail;
    l->ranges=calloc(l->range_count?l->range_count:1,sizeof *l->ranges); if(!l->ranges) goto fail;
    for(size_t i=0;i<l->range_count;++i,p+=8) l->ranges[i]=(TombRange){s16(p),s16(p+2),s16(p+4),s16(p+6)};
    l->command_count=read_count(&r,4); p=take(&r,l->command_count,2); if(!p) goto fail;
    l->commands=calloc(l->command_count?l->command_count:1,sizeof *l->commands); if(!l->commands) goto fail;
    for(size_t i=0;i<l->command_count;++i) l->commands[i]=s16(p+2*i);
    l->parsed_bytes=r.pos;
    section(&r,l,BA_MODL);
    n=read_count(&r,4); take(&r,n,4); /* mesh trees */
    n=read_count(&r,4); take(&r,n,2); /* animation frames */
    n=read_count(&r,4); take(&r,n,18); /* moveable models */
    l->static_def_count=read_count(&r,4); p=take(&r,l->static_def_count,32); if(!p) goto fail;
    if(l->static_def_count) {
        l->static_defs=calloc(l->static_def_count,sizeof *l->static_defs); if(!l->static_defs) goto fail;
        for(size_t i=0;i<l->static_def_count;++i,p+=32) {
            TombStaticDef *s=l->static_defs+i;
            s->id=u32(p); s->mesh=u16(p+4); s->flags=u16(p+30);
            for(int j=0;j<6;++j) { s->bounds[j]=s16(p+18+j*2); s->draw_bounds[j]=s16(p+6+j*2); }
            for(size_t j=0;j<i;++j) if(l->static_defs[j].id==s->id) goto fail;
        }
    }
    section(&r,l,BA_TEXI);
    l->static_parsed_bytes=r.pos;
    n=read_count(&r,4); take(&r,n,20); /* object textures */
    n=read_count(&r,4); take(&r,n,16); /* sprite textures */
    n=read_count(&r,4); take(&r,n,8); /* sprite sequences */
    section(&r,l,BA_WRLD);
    n=read_count(&r,4); l->camera_count=n; p=take(&r,n,16); if(!p)goto fail;
    if(n) {
        l->cameras=calloc(n,sizeof *l->cameras);if(!l->cameras)goto fail;
        for(size_t i=0;i<n;i++,p+=16) {
            l->cameras[i]=(TombFixedCamera){s32(p),s32(p+4),s32(p+8),s16(p+12),u16(p+14)};
            if(l->cameras[i].room<0 || (size_t)l->cameras[i].room>=l->room_count)goto fail;
        }
    }
    l->sound_sources_offset=r.pos;
    n=read_count(&r,4); take(&r,n,16); /* sound sources */
    uint32_t boxes=read_count(&r,4); p=take(&r,boxes,20); if(!p) goto fail;
    l->box_count=boxes;
    if(boxes) {
        l->boxes=calloc(boxes,sizeof *l->boxes); if(!l->boxes) goto fail;
        for(size_t i=0;i<boxes;i++,p+=20) {
            l->boxes[i]=(TombCameraBox){s32(p),s32(p+4),s32(p+8),s32(p+12),s16(p+16),u16(p+18)};
            if(l->boxes[i].zmin>l->boxes[i].zmax || l->boxes[i].xmin>l->boxes[i].xmax) goto fail;
        }
    }
    n=read_count(&r,4);p=take(&r,n,2);if(!p)goto fail;
    l->overlap_count=n;l->overlaps=calloc(n?n:1,sizeof *l->overlaps);if(!l->overlaps)goto fail;
    for(size_t i=0;i<n;i++)l->overlaps[i]=u16(p+2*i);
    p=take(&r,boxes,12);if(!p)goto fail;
    l->zones=calloc(boxes?6*boxes:1,sizeof *l->zones);if(!l->zones)goto fail;
    for(size_t i=0;i<6*boxes;i++)l->zones[i]=s16(p+2*i);
    n=read_count(&r,4); take(&r,n,2); /* animated textures */
    l->item_count=read_count(&r,4); p=take(&r,l->item_count,22); if(!p) goto fail;
    if(l->item_count) {
        l->items=calloc(l->item_count,sizeof *l->items); if(!l->items) goto fail;
        for(size_t i=0;i<l->item_count;++i,p+=22) {
            TombItem *item=l->items+i;
            item->object=s16(p); item->room=s16(p+2); item->x=s32(p+4); item->y=s32(p+8);
            item->z=s32(p+12); item->yaw=s16(p+16); item->intensity=s16(p+18); item->flags=u16(p+20);
            if(item->object<0 || item->room<0 || (size_t)item->room>=l->room_count) goto fail;
        }
    }
    section(&r,l,BA_PALT);
    l->item_parsed_bytes=r.pos;
    take(&r,8960,1);
    section(&r,l,BA_CINE);l->cinematic_offset=r.pos;
    n=read_count(&r,2);take(&r,n,16);n=read_count(&r,2);take(&r,n,1);
    section(&r,l,BA_SFXB);l->sound_bank_offset=r.pos;
    take(&r,256,2);n=read_count(&r,4);take(&r,n,8);
    n=read_count(&r,4);take(&r,n,1);n=read_count(&r,4);take(&r,n,4);
    if(r.failed || r.pos!=r.size)goto fail;
    for(size_t i=0;i<l->room_count;++i) {
        const TombRoom *room=l->rooms+i;
        for(size_t j=0;j<room->static_count;++j) {
            size_t k=0;
            while(k<l->static_def_count && l->static_defs[k].id!=room->statics[j].id) ++k;
            if(k==l->static_def_count) { why="undefined static mesh ID"; goto fail; }
        }
        for(size_t j=0;j<(size_t)room->nx*room->nz;++j) {
            const TombSector *s=room->sectors+j;
            if(s->floor_index && s->floor_index>=l->floor_count) goto fail;
            if((s->below!=255 && s->below>=l->room_count) || (s->above!=255 && s->above>=l->room_count)) goto fail;
        }
    }
    return l;
fail:
    if(file) fclose(file);
    tomb_level_free(l);
    if(error && cap) snprintf(error,cap,"%s",why);
    return NULL;
}
