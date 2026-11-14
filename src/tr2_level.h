#ifndef TR2_LEVEL_H
#define TR2_LEVEL_H
#include <stdint.h>
#include <stddef.h>

enum { TR2_AUTOSAVE=1, TR2_CONSISTENT_WEAPONS=2 };
typedef struct { uint32_t options; int32_t sidearm,last_drawn,switch_pending,campaign_home; } Tr2Customisation;

typedef struct { const uint8_t *data; uint32_t count; } Tr2Span;
typedef struct {
    int32_t x,z,bottom,top;
    Tr2Span vertices,quads,triangles,sprites,portals,sectors,lights,statics;
    uint16_t nz,nx,flags;
    int16_t ambient,ambient2,light_mode,alternate;
} Tr2Room;
typedef struct {
    uint8_t *bytes;
    size_t size,parsed;
    uint32_t tile_count,room_count;
    const uint8_t *palette,*palette16,*tiles16;
    Tr2Room *rooms;
    Tr2Span floors,mesh_data,mesh_offsets,animations,changes,ranges,commands,trees,frames;
    Tr2Span models,statics,textures,sprites,sequences,cameras,sounds,boxes,overlaps,zones;
    Tr2Span animated_textures,items,sound_details,sample_indices;
    const uint8_t *sound_map;
    int lara;
    int object_floors;
    void *world;
    Tr2Span cinematic;
    int sunset;
    int listener_room;
    Tr2Customisation custom;
} Tr2Level;

uint16_t tr2_u16(const uint8_t *p);
int16_t tr2_i16(const uint8_t *p);
uint32_t tr2_u32(const uint8_t *p);
int32_t tr2_i32(const uint8_t *p);
Tr2Level *tr2_load(const char *path,char *error,size_t cap);
void tr2_free(Tr2Level *level);
const uint8_t *tr2_model(const Tr2Level *level,uint32_t id);
int tr2_validate(const Tr2Level *level,char *error,size_t cap);
#endif
