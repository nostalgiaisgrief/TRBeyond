#ifndef TOMB_RING_H
#define TOMB_RING_H
#include "inventory.h"
/* Original item descriptor fields, without executable pointers. */
typedef struct TombRingItem {
 int16_t object,frames,frame,goal,open_frame,direction,speed,delay,pivot,pivot_now,selected_x,x,selected_y,y;
 int32_t selected_yoff,yoff,selected_zoff,zoff;
 uint32_t initial_meshes,meshes;int32_t order;
} TombRingItem;
typedef struct TombRingMotion {
 int16_t count,status,target,radius,radius_rate,camera,camera_rate,pitch,pitch_rate,angle,angle_rate,pivot,pivot_rate,x,x_rate;
 int32_t y,y_rate,z,z_rate;
} TombRingMotion;
typedef struct TombRing {
 TombRingItem items[12];TombRingMotion motion;
 int16_t radius,pitch,rotating,rotate_count,current,target,count,step,adder,left_adder,right_adder,angle;
 int32_t camera_y;int chosen,ready,sound;
} TombRing;
enum {TOMB_RING_LEFT=1,TOMB_RING_RIGHT=2,TOMB_RING_SELECT=4,TOMB_RING_BACK=8};
TOMB_EXPORT int tomb_ring_change_begin(TombRing *,int keys);
TOMB_EXPORT void tomb_ring_change_end(TombRing *,int keys,int current);
TOMB_EXPORT int tomb_ring_init_keys(TombRing *,const TombInventory *);
TOMB_EXPORT int tomb_ring_init(TombRing *,const TombInventory *);
TOMB_EXPORT void tomb_ring_motion_tick(TombRing *);
TOMB_EXPORT void tomb_ring_rotate(TombRing *,int right);
TOMB_EXPORT void tomb_ring_select(TombRing *,int deselect);
TOMB_EXPORT void tomb_ring_item_spin(TombRingItem *,int selected,int rotating,int status,int count,unsigned input);
TOMB_EXPORT int tomb_ring_compass_bone(int object,unsigned mesh_count,unsigned mesh_index);
TOMB_EXPORT void tomb_ring_compass(int16_t item_yaw,int16_t lara_yaw,int16_t *angle,int16_t *velocity);
TOMB_EXPORT int tomb_ring_animate(TombRingItem *);
/* One 30 Hz inventory frame, containing two original motion ticks. */
TOMB_EXPORT void tomb_ring_tick(TombRing *,unsigned input);
/* Layout: 0 original in-game, 1 title, 2 in-game with customisation photo. */
TOMB_EXPORT int tomb_ring_init_options(TombRing *,int title);
TOMB_EXPORT void tomb_ring_finish_item(TombRing *,int chosen);
#endif
