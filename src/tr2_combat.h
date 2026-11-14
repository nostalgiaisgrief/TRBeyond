#ifndef TR2_COMBAT_H
#define TR2_COMBAT_H
#include "tr2_runtime.h"
typedef struct {int16_t locked,yaw,pitch,roll;} Tr2ArmAim;
typedef struct {Tr2ArmAim aim;int16_t frame,flash;} Tr2GunArm;
typedef struct {int weapon,requested,target,ammo[9],shots,hits;Tr2GunArm arms[2];int16_t yaw,pitch,torso_yaw,torso_pitch;uint16_t meshes;Tr2Actor rifle;int rifle_active;int back_weapon,last_weapon,flare_age,flare_control,flare_timer;int uzi_sound[2];int rifle_sound,harpoon_reload;} Tr2Combat;
int tr2_weapon_model(int weapon);
int tr2_rifle_state(Tr2Combat *g,Tr2Actor *a,uint32_t input,int frame);
int tr2_pistol_sound(Tr2Combat *g,int arm,int frame,int action,int target,int fired,int previous_shot);
int tr2_pistol_draw(Tr2Combat *g);
int tr2_pistol_holster(Tr2Combat *g);
int tr2_pistol_frame(Tr2GunArm *arm,int action,int target,int recoil);
void tr2_weapon_aim(Tr2ArmAim *arm,int speed,int yaw,int pitch);
int tr2_weapon_sphere(const Tr2Sphere *spheres,int count,int *distance);
void tr2_weapon_target(const int bounds[6],const Tr2Actor *target,Tr2Position *out);
int tr2_weapon_damage(Tr2Actor *target,int damage);
void tr2_weapon_angles(int x,int y,int z,int16_t angles[2]);
int tr2_camera_combat(const Tr2Level *l,const Tr2Actor *a,Tr2Camera *c,const Tr2Combat *g,const Tr2Look *look);
void tr2_camera_weapon_tick(const Tr2Level *l,const Tr2Actor *a,Tr2Camera *c,const Tr2Look *look,const Tr2Combat *g);
void tr2_combat_look_pose(const Tr2Actor *a,const Tr2Combat *g,const Tr2Look *look,Tr2Look *out);
uint32_t tr2_combat_look_input(Tr2Combat *g,const Tr2Actor *a,Tr2Look *look,uint32_t input);
#endif
