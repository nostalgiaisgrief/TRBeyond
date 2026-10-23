/* DOS inventory helpers 0x240bc..0x246f8 and frame state machine 0x21498.
   Descriptor data from the supplied executable, not TRX code. */
#include "ring.h"
#include "fixed.h"
#include <string.h>
static const TombRingItem defaults[]={
{72,25,0,0,10,1,1,0,4352,0,-8192,0,0,0,0,0,456,0,5,5,0},
{99,12,0,0,11,1,1,0,3200,0,-3808,0,0,0,0,0,296,0,4294967295,4294967295,1},
{100,13,0,0,12,1,1,0,3200,0,0,0,-8192,0,0,0,296,0,4294967295,4294967295,2},
{101,12,0,0,11,1,1,0,3200,0,-3808,0,0,0,0,0,296,0,4294967295,4294967295,3},
{102,13,0,0,12,1,1,0,3200,0,-3808,0,0,0,0,0,296,0,4294967295,4294967295,4},
{103,1,0,0,0,1,1,0,3200,0,-3808,0,0,0,0,0,296,0,4294967295,4294967295,1},
{104,1,0,0,0,1,1,0,3200,0,-3808,0,0,0,0,0,296,0,4294967295,4294967295,2},
{105,1,0,0,0,1,1,0,3200,0,-3808,0,0,0,0,0,296,0,4294967295,4294967295,3},
{106,1,0,0,0,1,1,0,3200,0,-3808,0,0,0,0,0,296,0,4294967295,4294967295,4},
{107,15,0,0,14,1,1,0,5024,0,0,0,0,0,0,0,368,0,4294967295,4294967295,5},
{108,26,0,0,25,1,1,0,4032,0,-7296,0,-4096,0,0,0,216,0,4294967295,4294967295,7},
{109,20,0,0,19,1,1,0,3616,0,-8160,0,-4096,0,0,0,352,0,4294967295,4294967295,6}};
static void setup(TombRing *r,int count,int status,int target){r->motion=(TombRingMotion){0};r->motion.count=(int16_t)count;r->motion.status=(int16_t)status;r->motion.target=(int16_t)target;}
int tomb_ring_init(TombRing *r,const TombInventory *inv) {
 memset(r,0,sizeof *r);r->chosen=-1;
 for(unsigned i=0;i<sizeof defaults/sizeof defaults[0];i++) {
  const TombRingItem *d=defaults+i;int id=d->object;
  if(id!=72 && (id<99 || id>109 || !inv->counts[id-99]))continue;
  int at=r->count++;while(at>0 && r->items[at-1].order>d->order){r->items[at]=r->items[at-1];--at;}r->items[at]=*d;
 }
 if(!r->count)return 0;
 r->step=tomb_word(65536/r->count);r->left_adder=(int16_t)(r->step/24);r->right_adder=(int16_t)-r->left_adder;
 r->camera_y=-1536;r->angle=16384;setup(r,32,0,1);
 r->motion.radius=688;r->motion.radius_rate=21;r->motion.camera=-256;r->motion.camera_rate=40;r->motion.angle=-16384;r->motion.angle_rate=-1024;r->sound=111;return 1;
}
int tomb_ring_init_keys(TombRing *r,const TombInventory *inv){
 memset(r,0,sizeof *r);r->chosen=-1;
 for(int q=0;q<8;q++)if(inv->quest[q]){
  TombRingItem item={q<4?114+q:133+q-4,1,0,0,0,1,1,0,7200,0,-4352,0,0,0,0,0,256,0,UINT32_MAX,UINT32_MAX,q<4?108-q:101+q-4};
  int at=r->count++;while(at>0 && r->items[at-1].order>item.order){r->items[at]=r->items[at-1];--at;}r->items[at]=item;
 }
 if(inv->counts[8]){memmove(r->items+1,r->items,r->count*sizeof *r->items);++r->count;r->items[0]=(TombRingItem){127,1,0,0,0,1,1,0,3616,0,-8160,0,-4096,0,0,0,352,0,UINT32_MAX,UINT32_MAX,100};}
 if(inv->scion)r->items[r->count++]=(TombRingItem){150,1,0,0,0,1,1,0,7200,0,-4352,0,0,0,0,0,256,0,UINT32_MAX,UINT32_MAX,109};
 if(!r->count)return 0;
 r->step=tomb_word(65536/r->count);r->left_adder=(int16_t)(r->step/24);r->right_adder=(int16_t)-r->left_adder;
 r->camera_y=-1536;r->angle=16384;setup(r,32,0,1);
 r->motion.radius=688;r->motion.radius_rate=21;r->motion.camera=-256;r->motion.camera_rate=40;r->motion.angle=-16384;r->motion.angle_rate=-1024;r->sound=111;return 1;
}
/* Original option descriptors at c322c/c32ec/c326c/c32ac/c336c.
   The closed passport is model 81; its animated pages are model 71. */
int tomb_ring_init_options(TombRing *r,int title){
 static const TombRingItem options[]={
 {81,30,0,0,14,1,1,0,4640,0,-4320,0,0,0,0,0,384,0,19,19,0},
 {97,1,0,0,0,1,1,0,5504,0,-12288,0,0,0,0,0,352,0,UINT32_MAX,UINT32_MAX,3},
 {95,1,0,0,0,1,1,0,4224,0,-6720,0,0,0,0,0,424,0,UINT32_MAX,UINT32_MAX,1},
 {96,1,0,0,0,1,1,0,4832,0,-2336,0,0,0,0,0,368,0,UINT32_MAX,UINT32_MAX,2},
 {73,1,0,0,0,1,1,0,4640,0,-4320,0,0,0,0,0,384,0,UINT32_MAX,UINT32_MAX,5}};
 memset(r,0,sizeof *r);r->chosen=-1;r->count=title?5:4;memcpy(r->items,options,r->count*sizeof *options);
 r->step=tomb_word(65536/r->count);r->left_adder=(int16_t)(r->step/24);r->right_adder=(int16_t)-r->left_adder;
 r->camera_y=-1536;r->angle=16384;r->pitch=title==1?1024:0;setup(r,32,0,1);
 r->motion.radius=688;r->motion.radius_rate=21;r->motion.camera=-256;r->motion.camera_rate=40;r->motion.angle=-16384;r->motion.angle_rate=-1024;r->sound=111;return 1;
}
void tomb_ring_finish_item(TombRing *r,int chosen){
 TombRingItem *i=r->items+r->current;
 if(r->motion.status!=8 || !r->ready)return;
 r->chosen=chosen;i->goal=i->object==71 && i->frame==24?29:0;i->direction=i->goal?1:-1;
 setup(r,0,11,chosen>=0?12:10);r->ready=0;
}
/* DOS main<->keys vertical transfer: 0x21d36 / 0x21e37 and 0x22071 / 0x22136. */
int tomb_ring_change_begin(TombRing *r,int keys){
 if(r->motion.status!=1 || r->rotating)return 0;
 r->motion.count=24;r->motion.status=2;r->motion.target=keys?4:5;r->motion.camera_rate=0;r->motion.radius=0;r->motion.radius_rate=(int16_t)(-r->radius/24);
 r->motion.angle=tomb_word(r->angle-32768);r->motion.angle_rate=-1365;
 r->motion.pitch=keys?8192:-8192;r->motion.pitch_rate=(int16_t)(r->motion.pitch/24);r->sound=0;return 1;
}
void tomb_ring_change_end(TombRing *r,int keys,int current){
 if(current<0 || current>=r->count)current=0;r->current=(int16_t)current;
 setup(r,24,0,1);r->radius=0;r->camera_y=-256;r->pitch=keys?-8192:8192;
 r->motion.radius=688;r->motion.radius_rate=28;r->motion.pitch_rate=(int16_t)(-r->pitch/24);
 r->motion.angle=tomb_word(49152-current*r->step);r->angle=tomb_word(r->motion.angle-32768);r->motion.angle_rate=-1365;r->sound=0;
}
void tomb_ring_motion_tick(TombRing *r) {
 TombRingMotion *m=&r->motion;TombRingItem *i=r->items+r->current;
 if(m->count) {
  r->radius=tomb_word(r->radius+m->radius_rate);r->camera_y+=m->camera_rate;r->angle=tomb_word(r->angle+m->angle_rate);r->pitch=tomb_word(r->pitch+m->pitch_rate);
  i->pivot_now=tomb_word(i->pivot_now+m->pivot_rate);i->x=tomb_word(i->x+m->x_rate);i->yoff+=m->y_rate;i->zoff+=m->z_rate;
  if(!--m->count){m->status=m->target;
#define SNAP(rate,field,target) if(m->rate){m->rate=0;field=m->target;}
   SNAP(radius_rate,r->radius,radius) SNAP(camera_rate,r->camera_y,camera) SNAP(angle_rate,r->angle,angle) SNAP(pitch_rate,r->pitch,pitch)
   SNAP(pivot_rate,i->pivot_now,pivot) SNAP(x_rate,i->x,x) SNAP(y_rate,i->yoff,y) SNAP(z_rate,i->zoff,z)
#undef SNAP
  }
 }
 if(r->rotating){r->angle=tomb_word(r->angle+r->adder);if(!--r->rotate_count){r->current=r->target;r->rotating=0;r->angle=tomb_word(49152-r->current*r->step);}}
}
void tomb_ring_rotate(TombRing *r,int right) {
 r->rotating=1;r->target=(int16_t)(r->current+(right?-1:1));if(r->target<0)r->target=r->count-1;if(r->target>=r->count)r->target=0;
 r->rotate_count=24;r->adder=right?r->left_adder:r->right_adder;r->sound=108;
}
void tomb_ring_select(TombRing *r,int deselect) {
 TombRingMotion *m=&r->motion;TombRingItem *i=r->items+r->current;int sign=deselect?-1:1;
 m->pivot=deselect?0:i->pivot;m->pivot_rate=(int16_t)(sign*i->pivot/m->count);
 m->x=deselect?0:i->selected_x;m->x_rate=(int16_t)(sign*i->selected_x/m->count);
 m->y=deselect?0:i->selected_yoff;m->y_rate=sign*i->selected_yoff/m->count;
 m->z=deselect?0:i->selected_zoff;m->z_rate=sign*i->selected_zoff/m->count;
}
int tomb_ring_animate(TombRingItem *i) {
 int moving=i->frame!=i->goal;
 if(moving){if(i->delay)--i->delay;else{i->delay=i->speed;i->frame=tomb_word(i->frame+i->direction);if(i->frame>=i->frames)i->frame=0;if(i->frame<0)i->frame=i->frames-1;}}
 if(i->object==71){int f=i->frame;i->meshes=f<=14?0x57:f<19?0x5f:f==19?0x5b:f<24?0x7b:f<29?0x3b:f==29?0x13:UINT32_MAX;return moving;}
 i->meshes=i->object==72 && (i->frame==0 || i->frame>=18)?i->initial_meshes:UINT32_MAX;return moving;
}
static void close_ring(TombRing *r) {
 setup(r,32,2,13);r->motion.radius_rate=(int16_t)(-r->radius/32);r->motion.camera=-1536;r->motion.camera_rate=(int16_t)((-1536-r->camera_y)/32);
 r->motion.angle=tomb_word(r->angle-32768);r->motion.angle_rate=-1024;r->sound=112;
}
void tomb_ring_item_spin(TombRingItem *i,int selected,int rotating,int status,int count,unsigned input) {
 if(!selected){if(i->y)i->y=tomb_word(i->y+(i->y<0?256:-256));}
 else if(rotating){if(i->y)i->y=tomb_word(i->y+(i->y<0?512:-512));}
 else if(status>=7 && status<=11){if(i->y!=i->selected_y){int d=i->selected_y-i->y;i->y=tomb_word(i->y+((d>0 && d<32768)?1024:-1024));i->y=(int16_t)((uint16_t)i->y&0xfc00);}}
 else if(count==1 || !(input&(TOMB_RING_LEFT|TOMB_RING_RIGHT)))i->y=tomb_word(i->y+256);
}
static void spin(TombRing *r,unsigned input) {
 for(int j=0;j<r->count;j++)for(int tick=0;tick<2;tick++)tomb_ring_item_spin(r->items+j,j==r->current,r->rotating,r->motion.status,r->count,input);
}
void tomb_ring_tick(TombRing *r,unsigned input) {
 r->sound=0;if(r->motion.status==13)return;
 tomb_ring_motion_tick(r);tomb_ring_motion_tick(r);spin(r,input);
 if(r->rotating)return;
 TombRingItem *i=r->items+r->current;int status=r->motion.status;
 if(status==1) {
  if((input&TOMB_RING_RIGHT) && r->count>1)tomb_ring_rotate(r,1);
  else if((input&TOMB_RING_LEFT) && r->count>1)tomb_ring_rotate(r,0);
  else if(input&TOMB_RING_BACK)close_ring(r);
  else if(input&TOMB_RING_SELECT){i->goal=i->open_frame;i->direction=1;setup(r,16,7,8);r->motion.angle=tomb_word(49152-r->current*r->step);tomb_ring_select(r,0);r->sound=i->object==81?111:i->object==73?109:i->object==97?110:i->object==72?113:i->object>=99 && i->object<=102?114:111;}
 } else if(status==8) {
  if(i->object==81)i->object=71;
  int moving=0;for(int t=0;t<2;t++)if(i->y==i->selected_y)moving=tomb_ring_animate(i);
  r->ready=!moving;
  if(r->ready && ((input&TOMB_RING_BACK) || (i->object==72 && (input&TOMB_RING_SELECT)))) {
   i->goal=i->object==72?i->frames-1:i->object==71 && i->frame==24?29:0;i->direction=i->goal?1:-1;setup(r,0,11,10);r->ready=0;
  } else if(r->ready && (tomb_quest_slot(i->object)>=0 || i->object==127 || (i->object>=99 && i->object<=102) || i->object==108 || i->object==109)) {r->chosen=i->object;setup(r,0,11,12);r->ready=0;}
 } else if(status==11) {
  int moving=0;for(int t=0;t<2;t++)moving=tomb_ring_animate(i);
  if(!moving){if(i->object==71){i->object=81;i->frame=0;i->meshes=i->initial_meshes;}int target=r->motion.target;setup(r,16,target,target);tomb_ring_select(r,1);}
 } else if(status==10) {r->motion.count=16;r->motion.status=9;r->motion.target=1;r->motion.angle=tomb_word(49152-r->current*r->step);r->sound=112;}
 else if(status==12 && !r->motion.count)close_ring(r);
}

/* DrawInventoryItem 0x22b65 checks object 72 and remaining mesh count 3.
   Only the needle receives this rotation, once per inventory frame. */
int tomb_ring_compass_bone(int object,unsigned mesh_count,unsigned mesh_index) {
 return object==72 && mesh_index>0 && mesh_index<mesh_count && mesh_count-mesh_index==3;
}
void tomb_ring_compass(int16_t item_yaw,int16_t lara_yaw,int16_t *angle,int16_t *velocity) {
 int error=tomb_word(-(int)item_yaw-lara_yaw-*angle);
 *velocity=tomb_word(*velocity*19/20+error/50);*angle=tomb_word(*angle+*velocity);
}
