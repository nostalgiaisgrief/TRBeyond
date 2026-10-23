#ifndef TOMB_LEVEL_H
#define TOMB_LEVEL_H
#include "step.h"
typedef struct TombSector {
    uint16_t floor_index, box;
    uint8_t below;
    int8_t floor;
    uint8_t above;
    int8_t ceiling;
} TombSector;
typedef struct TombStaticPlacement {
    int32_t x,y,z;
    uint16_t rotation,intensity,id;
} TombStaticPlacement;
typedef struct TombStaticDef {
    uint32_t id;
    uint16_t mesh,flags;
    int16_t bounds[6]; /* min/max X, Y, Z collision bounds */
    int16_t draw_bounds[6]; /* Original visibility bounds; distinct from collision. */
} TombStaticDef;
typedef struct TombItem {
    int32_t x,y,z;
    int16_t object,room,yaw,intensity;
    uint16_t flags;
    int16_t state;
    uint8_t state_known; /* File placements do not establish runtime state. */
} TombItem;
typedef struct TombRoom {
    int32_t x,z,bottom,top;
    uint16_t nz,nx;
    TombSector *sectors;
    int16_t alternate,flags;
    TombStaticPlacement *statics;
    size_t static_count;
} TombRoom;
typedef struct TombFixedCamera {
    int32_t x,y,z;
    int16_t room;
    uint16_t flags;
} TombFixedCamera;

typedef struct TombCameraBox {
    int32_t zmin,zmax,xmin,xmax;
    int16_t height;
    uint16_t overlap;
} TombCameraBox;

typedef struct TombLevel {
    TombRoom *rooms;
    size_t room_count;
    uint16_t *floor_data;
    size_t floor_count;
    TombAnim *animations;
    size_t animation_count;
    TombChange *changes;
    size_t change_count;
    TombRange *ranges;
    size_t range_count;
    int16_t *commands;
    size_t command_count;
    unsigned char *file_data;
    size_t file_size,parsed_bytes;
    TombStaticDef *static_defs;
    size_t static_def_count,static_parsed_bytes;
    TombItem *items;
    size_t item_count,item_parsed_bytes;
    TombCameraBox *boxes;
    size_t box_count;
    size_t camera_count;
    TombFixedCamera *cameras;
    uint16_t *overlaps;
    size_t overlap_count;
    int16_t *zones; /* Ground, alternate ground, fly; then the flipped-room triple. */
    int dos_level; /* Canonical campaign index, or -1 for an unknown filename. */
} TombLevel;
/* Loads the TR1 prefix through item placements. Rendering meshes and
   later sections remain in file_data but are not decoded yet. On failure returns
   NULL with a diagnostic, releasing every allocation. Own result with free. */
TOMB_EXPORT TombLevel *tomb_level_load(const char *,char *error,size_t error_size);
TOMB_EXPORT void tomb_level_free(TombLevel *);
typedef struct TombSectorRef { int32_t room,index; } TombSectorRef;
TOMB_EXPORT int tomb_find_sector(const TombLevel *,int32_t x,int32_t y,int32_t z,
                                int32_t start_room,TombSectorRef *);
typedef struct TombHeights {
    int16_t floor,ceiling;
    int32_t floor_type; /* 0 flat, 1 small tilt, 2 steep tilt */
    uint32_t trigger_index,object_references;
} TombHeights;
/* Structural geometry ONLY. object_references > 0 means object height callbacks
   are unresolved. Never use this result as complete gameplay collision then.
   heavy != 0 reproduces the DOS large-slope suppression flag. */
TOMB_EXPORT int tomb_static_heights(const TombLevel *,TombSectorRef,int32_t x,
                                   int32_t z,int heavy,TombHeights *);
/* Returns 1 if this callback is reconstructed and required state is known,
   otherwise 0 without changing height. ceiling is 0 or 1. */
TOMB_EXPORT int tomb_item_height(const TombItem *,int32_t x,int32_t y,int32_t z,
                                 int ceiling,int16_t *height);
/* Runtime item array indexed by trigger item number. Unimplemented callbacks
   or unknown states remain explicitly counted in object_references. */
TOMB_EXPORT int tomb_item_heights(const TombLevel *,TombSectorRef,int32_t x,int32_t y,
    int32_t z,int heavy,const TombItem *,size_t item_count,TombHeights *);
TOMB_EXPORT int tomb_floor_tilt(const TombLevel *,TombSectorRef,int32_t x,int32_t y,
                               int32_t z,int16_t *tilt);
typedef struct TombTerrainSample { int32_t floor,ceiling,type; } TombTerrainSample;
typedef struct TombTerrain {
    TombTerrainSample samples[4]; /* centre, front, left, right */
    uint32_t trigger_index,object_references;
    int16_t quadrant,tilt_x,tilt_z;
    uint8_t static_meshes_pending;
} TombTerrain;
/* Structural branch of DOS 0x151c0. Static mesh contacts are NOT evaluated;
   static_meshes_pending is always set on success. Object references accumulate
   across samples. Success means a structural result, not complete collision.
   lara_y is the original global Lara position used by the tilt helper.
   Output/contact are committed only on success. sine has 1025 original entries. */
TOMB_EXPORT int tomb_terrain_contact(const TombLevel *,const TombActor *,TombContact *,
    int32_t height,int32_t radius,int32_t lara_y,const int16_t *sine,int heavy,TombTerrain *);
typedef struct TombStaticContact {
    int16_t rooms[9];
    uint16_t room_count;
    uint8_t hit;
} TombStaticContact;
/* Original 0x158a8 and its eight-corner nearby-room query. On failure outputs
   stay unchanged. First intersecting eligible placement wins in original order. */
TOMB_EXPORT int tomb_static_contact(const TombLevel *,const TombActor *,TombContact *,
    int32_t height,int32_t radius,int16_t quadrant,TombStaticContact *);
/* Terrain plus actual static contacts, preserving the DOS order. Dynamic object
   height callbacks are still unresolved and reported by object_references. */
TOMB_EXPORT int tomb_world_contact(const TombLevel *,const TombActor *,TombContact *,
    int32_t height,int32_t radius,int32_t lara_y,const int16_t *sine,int heavy,
    TombTerrain *,TombStaticContact *);
TOMB_EXPORT int tomb_active_contact(const TombLevel *,const TombActor *,TombContact *,
    int32_t height,int32_t radius,int32_t lara_y,const int16_t *sine,int heavy,
    const TombItem *,size_t item_count,TombTerrain *,TombStaticContact *);
#endif
