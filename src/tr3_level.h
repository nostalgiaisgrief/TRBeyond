#ifndef TR3_LEVEL_H
#define TR3_LEVEL_H
#include <stdint.h>
#include <stddef.h>

typedef struct { const uint8_t *data; uint32_t count; } Tr3Span;
typedef struct {
    int32_t x,z,bottom,top;
    Tr3Span vertices,quads,triangles,sprites,portals,sectors,lights,statics;
    uint16_t nz,nx,flags;
    int16_t ambient,ambient2,light_mode,alternate;
} Tr3Room;
typedef struct {
    uint8_t *bytes;
    size_t size,parsed;
    uint32_t tile_count,room_count;
    const uint8_t *palette,*palette16,*tiles16;
    Tr3Room *rooms;
    Tr3Span floors,mesh_data,mesh_offsets,animations,changes,ranges,commands,trees,frames;
    Tr3Span models,statics,textures,sprites,sequences,cameras,sounds,boxes,overlaps,zones;
    Tr3Span animated_textures,items,sound_details,sample_indices;
    const uint8_t *sound_map;
    int lara;
    int object_floors;
    Tr3Span cinematic_frames;
} Tr3Level;

uint16_t tr3_u16(const uint8_t *p);
int16_t tr3_i16(const uint8_t *p);
uint32_t tr3_u32(const uint8_t *p);
int32_t tr3_i32(const uint8_t *p);
Tr3Level *tr3_load(const char *path,char *error,size_t cap);
void tr3_free(Tr3Level *level);
const uint8_t *tr3_model(const Tr3Level *level,uint32_t id);
int tr3_validate(const Tr3Level *level,char *error,size_t cap);
#endif
