#ifndef TR2_RUNTIME_H
#define TR2_RUNTIME_H
#include "tr2_level.h"
#include "tr2_control.h"

enum { TR2_NO_HEIGHT=-32512 };
typedef struct {int x,y,z,radius;} Tr2Sphere;
typedef struct {int shade,divider,direction[3];} Tr2Light;
typedef struct {int point[7][3],velocity[7][3];int16_t pitch[7],yaw[7];int ready;} Tr2Hair;
typedef struct { int room,index; } Tr2Sector;
typedef struct {int floor,ceiling,type,tilt_x,tilt_z;uint32_t trigger;} Tr2Height;
typedef struct {
    Tr2Control control;
    int32_t x,y,z;
    int16_t yaw,pitch,move_angle,speed,fall_speed,room,calculated_fall,gun_status;
    int32_t water_distance;
    int32_t event_count,event_kind[16],event_value[16];
    int extra_animation;
} Tr2Actor;
typedef struct {int32_t x,y,z;int16_t room,pad;} Tr2Position;
typedef struct {int16_t air,dive_count;} Tr2Water;
typedef struct {int16_t yaw,pitch,torso_yaw,torso_pitch;int active;} Tr2Look;
uint32_t tr2_look_input(Tr2Look *look,const Tr2Actor *actor,uint32_t input);
typedef struct {
    int32_t sample[4][3],radius,positive,negative,ceiling_limit,shift[3],old[3];
    int16_t old_state,old_animation,old_frame,facing,quadrant,type;
    uint32_t trigger;
    int8_t tilt_x,tilt_z,pad,hit_static;
    uint16_t flags;
} Tr2Contact;
typedef struct {
    Tr2Position position,target;
    int32_t distance,speed;
    int16_t angle,elevation;
    int initialized,shift;
    int32_t chase_radius_squared;
} Tr2Camera;

int tr2_math_load(const char *path);
void tr2_math_set(const int16_t *table);
int tr2_sin(int angle);
int tr2_cos(int angle);
int tr2_atan(int z,int x);
int tr2_best_bounds(const Tr2Level *l,const Tr2Actor *a,int bounds[6]);
void tr2_rotate_offset(int yaw,int pitch,const int vector[3],int out[3]);
void tr2_hair_step(Tr2Hair *hair,const int bones[6][3],const Tr2Sphere body[5],int water_status,int water_height,int floor,int wind);
int tr2_hair_samples(const Tr2Level *l,const Tr2Actor *a,const Tr2Look *look,int anchor[3],Tr2Sphere body[5]);
int tr2_object_overlap(const Tr2Level *l,const Tr2Actor *o,const Tr2Actor *a,int radius);
int tr2_object_push(const Tr2Level *l,const Tr2Actor *o,Tr2Actor *a,Tr2Contact *contact);
int tr2_object_push_reaction(const Tr2Level *l,const Tr2Actor *o,Tr2Actor *a,Tr2Contact *contact,int react);
int tr2_push_horizontal(const Tr2Actor *o,Tr2Actor *a,const int b[6],int radius);
int tr2_object_push_bounds(const Tr2Level *l,const Tr2Actor *o,Tr2Actor *a,Tr2Contact *contact,int react,int expand);
int16_t tr2_word(int n);
int32_t tr2_long(int64_t n);
int tr2_asr(int64_t n,int bits);
int tr2_room_light_phase(const Tr2Level *l,const Tr2Room *room,int phase);
void tr2_room_light(const Tr2Level *l,int room,int x,int y,int z,int phase,int depth,Tr2Light *out);
uint32_t tr2_shade_rgb(int shade,int water);
int tr2_joint(const Tr2Level *l,const Tr2Actor *a,int id,int joint,int point[3]);
int tr2_spheres(const Tr2Level *l,const Tr2Actor *a,int id,Tr2Sphere out[64]);
int tr2_spheres_shot(const Tr2Level *l,const Tr2Actor *a,int id,const Tr2Position *origin,int yaw,int pitch,Tr2Sphere out[64]);
uint32_t tr2_touch(const Tr2Level *l,const Tr2Actor *a,int id,const Tr2Actor *b,int bid);
int tr2_find_sector(const Tr2Level *l,int x,int y,int z,int room,Tr2Sector *out);
int tr2_heights(const Tr2Level *l,Tr2Sector ref,int x,int y,int z,int chunky,Tr2Height *out);
int tr2_query(const Tr2Level *l,int x,int y,int z,int room,Tr2Height *out,Tr2Sector *sector);
void tr2_bridge_height(int id,int object_y,int yaw,int x,int y,int z,int *floor,int *ceiling);
int tr2_water_height(const Tr2Level *l,int x,int z,int room,int *depth);
void tr2_water_control(const Tr2Level *l,Tr2Actor *actor,Tr2Water *water,uint32_t input);
void tr2_water_tick(const Tr2Level *l,Tr2Actor *actor,Tr2Water *water,uint32_t input);
void tr2_player_tick(const Tr2Level *l,Tr2Actor *actor,Tr2Water *water,uint32_t input);
void tr2_water_transition(const Tr2Level *l,Tr2Actor *actor,Tr2Water *water);
int tr2_animate(const Tr2Level *l,Tr2Actor *actor);
int tr2_animate_required(const Tr2Level *l,Tr2Actor *actor,int16_t *required);
int tr2_actor_init(const Tr2Level *l,Tr2Actor *actor);
int tr2_actor_bounds(const Tr2Level *l,const Tr2Actor *actor,int bounds[6]);
int tr2_vault(const Tr2Level *l,Tr2Actor *actor,Tr2Contact *contact,uint32_t input);
int tr2_grab(const Tr2Level *l,Tr2Actor *actor,Tr2Contact *contact,uint32_t input,int up);
int tr2_hang_control(Tr2Actor *actor,uint32_t input);
int tr2_ladder_marked(const Tr2Level *l,const Tr2Actor *actor);
int tr2_ladder_test(const Tr2Level *l,int x,int y,int z,int dx,int dz,int height,int room,int *shift);
int tr2_ladder_position(const Tr2Level *l,const Tr2Actor *actor,int front,int right,int origin,int height,int *shift);
int tr2_ladder_up(const Tr2Level *l,const Tr2Actor *actor,int front,int right,int *shift,int *ledge);
int tr2_ladder_stance(const Tr2Level *l,Tr2Actor *actor,int radius);
int tr2_ladder_hang(const Tr2Level *l,Tr2Actor *actor,const Tr2Contact *contact);
int tr2_ladder_control(Tr2Actor *actor,uint32_t input);
int tr2_ladder_collision(const Tr2Level *l,Tr2Actor *actor,Tr2Contact *contact,uint32_t input);
int tr2_hang_collision(const Tr2Level *l,Tr2Actor *actor,Tr2Contact *contact,uint32_t input);
int tr2_surface_exit(const Tr2Level *l,Tr2Actor *actor,Tr2Contact *contact,uint32_t input);
void tr2_set_animation(const Tr2Level *l,Tr2Actor *actor,int animation,int frame_offset);
int tr2_landing_damage(Tr2Actor *actor);
int tr2_air_control(const Tr2Level *l,Tr2Actor *actor,uint32_t input);
int tr2_air_collision(const Tr2Level *l,Tr2Actor *actor,Tr2Contact *contact,uint32_t input);
void tr2_ground_tick(const Tr2Level *l,Tr2Actor *actor,uint32_t input);
void tr2_ground_control(const Tr2Level *l,Tr2Actor *actor,uint32_t input);
int tr2_contact(const Tr2Level *l,const Tr2Actor *actor,Tr2Contact *contact,int height,int statics);
int tr2_contact_body(const Tr2Level *l,const Tr2Actor *query,Tr2Contact *contact,int height,int statics,int body_y);
int tr2_static_contact(const Tr2Level *l,const Tr2Actor *actor,Tr2Contact *contact,int height,int room);
void tr2_camera_tick(const Tr2Level *l,const Tr2Actor *actor,Tr2Camera *camera);
int tr2_cinematic_camera(const Tr2Level *l,const Tr2Position *origin,int yaw,int *frame,Tr2Camera *camera,int *fov,int *roll);
void tr2_camera_view_tick(const Tr2Level *l,const Tr2Actor *actor,Tr2Camera *camera,const Tr2Look *look);
int tr2_camera_los(const Tr2Level *l,const Tr2Position *start,Tr2Position *end,int chunky);
int tr2_camera_adjust(const Tr2Level *l,const Tr2Position *target,Tr2Position *ideal,int32_t radius_squared,int chunky);
int tr2_camera_move(const Tr2Level *l,Tr2Camera *camera,Tr2Position ideal,int speed);
int tr2_camera_chase(const Tr2Level *l,const Tr2Actor *actor,Tr2Camera *camera,int chunky);
int tr2_camera_clamp(const Tr2Level *l,Tr2Position *position,int margin);
int tr2_camera_look(const Tr2Level *l,const Tr2Actor *actor,Tr2Camera *camera,const Tr2Look *look);
#endif
