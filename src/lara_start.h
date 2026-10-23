#ifndef TOMB_LARA_START_H
#define TOMB_LARA_START_H
#include "level.h"
typedef struct TombLaraStart {
    TombActor actor;
    uint32_t item_index;
    int32_t sector_floor;
    int16_t health,air,water_status,turn_rate,move_angle,fall_override;
    int16_t pitch,lean,head_yaw,head_pitch,head_roll,torso_yaw,torso_pitch,torso_roll;
    uint8_t services_pending; /* bit 0 inventory/weapons/mesh setup; bit 1 scratch allocator */
} TombLaraStart;
/* Fresh placement initialization, relevant portions of DOS 0x24870 + 0x28d38.
   Requires exactly one Lara (object 0), ordinary placement flags (zero), valid
   starting sector and standing/swimming animation. This is not save restoration.
   Inventory, weapon/mesh setup, camera and room item lists remain separate.
   Does not mutate level data; failure preserves output. */
TOMB_EXPORT int tomb_lara_start(const TombLevel *,TombLaraStart *);
#endif
