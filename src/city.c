/* DOS push block 0x2eb00..0x2f439 and underwater switch 0x345d4. */
#include "city.h"
#include "fixed.h"
#include "object_contact.h"
#include "enemies.h"
#include <math.h>
#include <stdlib.h>
static TombSector *sector(TombLevel *l,TombSectorRef r){return l->rooms[r.room].sectors+r.index;}
int tomb_city_bounds(const TombActor *a,const TombActor *o,int16_t pitch,int16_t roll,int water,const int16_t *sine){
 int yaw=tomb_word(a->yaw-o->yaw),limit=water?14560:1820;
 if(pitch< -limit || pitch>limit || roll< -limit || roll>limit || yaw< -(water?14560:5460) || yaw>(water?14560:5460))return 0;
 int32_t delta[3]={a->x-o->x,a->y-o->y,a->z-o->z},v[3];tomb_rotate_vector(o->yaw,0,0,delta,1,sine,v);
 if(water)return v[0]>=-1024 && v[0]<=1024 && v[1]>=-1024 && v[1]<=1024 && v[2]>=-1024 && v[2]<=512;
 return v[0]>=-300 && v[0]<=300 && !v[1] && v[2]>=-692 && v[2]<=-512;
}
/* Save the occupied floor for reset; movement removes it before animation. */
int tomb_block_floor(TombObjects *w,size_t id,int delta){
 TombActor *a=&w->items[id].actor;TombSectorRef low,high;
 if(!tomb_find_sector(w->level,a->x,a->y,a->z,a->room,&low) || !tomb_find_sector(w->level,a->x,a->y+delta-1024,a->z,low.room,&high))return 0;
 TombSector *s=sector(w->level,low),*top=sector(w->level,high);TombDoor *save=w->doors+id;
 if(delta<0){save->count=1;save->parts[0]=(TombDoorPart){low,*s,-1};}else save->count=0;
 int floor=s->floor==-127?top->ceiling+delta/256:s->floor+delta/256;
 if(s->floor!=-127 && floor==top->ceiling)floor=-127;s->floor=(int8_t)floor;
 if(s->box<w->level->box_count && (w->level->boxes[s->box].overlap&0x8000)){
  if(delta<0)w->level->boxes[s->box].overlap|=0x4000;else w->level->boxes[s->box].overlap&=(uint16_t)~0x4000;
 }
 return 1;
}
static int clear_static(TombLevel *l,TombActor a,int height,int radius,int quadrant){
 TombContact c={0};TombStaticContact sc;return tomb_static_contact(l,&a,&c,height,radius,(int16_t)quadrant,&sc) && !sc.hit;
}
static int destination(TombLevel *l,TombActor *a,int height){
 TombSectorRef r;if(!tomb_find_sector(l,a->x,a->y,a->z,a->room,&r))return 0;
 if(sector(l,r)->floor*256!=a->y)return 0;a->room=(int16_t)r.room;
 if(!tomb_find_sector(l,a->x,a->y-height,a->z,a->room,&r))return 0;
 return sector(l,r)->ceiling*256<=a->y-height;
}
int tomb_block_can_move(TombObjects *w,const TombActor *lara,size_t id,int quadrant,int pull){
 if(id>=w->count || quadrant<0 || quadrant>3)return 0;
 TombActor a=w->items[id].actor;TombSectorRef ref;
 if(!tomb_find_sector(w->level,a.x,a.y,a.z,a.room,&ref))return 0;
 int floor=sector(w->level,ref)->floor;if(floor!=-127 && floor*256!=a.y-1024)return 0;
 const int dx[4]={0,1024,0,-1024},dz[4]={1024,0,-1024,0};int x=dx[quadrant]*(pull?-1:1),z=dz[quadrant]*(pull?-1:1);
 a.x+=x;a.z+=z;
 if(!clear_static(w->level,a,1000,500,quadrant) || !destination(w->level,&a,1024))return 0;
 if(pull){a.x+=x;a.z+=z;if(!destination(w->level,&a,762))return 0;
  a=*lara;a.x+=x;a.z+=z;TombSectorRef r;if(!tomb_find_sector(w->level,a.x,a.y,a.z,a.room,&r))return 0;a.room=(int16_t)r.room;
  if(!clear_static(w->level,a,762,100,(quadrant+2)&3))return 0;
 }
 return 1;
}
int tomb_block_tick(TombObjects *w,size_t id,TombAnimContext *c){
 TombObject *o=w->items+id;TombActor *a=&o->actor;
 if(o->flags&0x100){if(!tomb_block_floor(w,id,1024))return 0;a->flags|=6;o->active=0;return 1;}
 if(!tomb_object_animate(o,c))return 0;
 TombSectorRef ref;TombHeights h;
 if(!tomb_find_sector(w->level,a->x,a->y,a->z,a->room,&ref) || !tomb_item_heights(w->level,ref,a->x,a->y,a->z,0,w->level->items,w->count,&h))return 0;
 if(h.floor>a->y)a->flags|=8;
 else if(a->flags&8){a->y=h.floor;a->flags=(a->flags&~14u)|4;if(c->event)c->event(c->user,5,70,a);}
 a->room=(int16_t)ref.room;
 if((a->flags&6)==4){a->flags&=(uint8_t)~6u;o->active=0;if(!tomb_block_floor(w,id,-1024))return 0;
  if(!tomb_find_sector(w->level,a->x,a->y,a->z,a->room,&ref) || !tomb_item_heights(w->level,ref,a->x,a->y,a->z,0,w->level->items,w->count,&h) || !tomb_objects_trigger_heavy(w,h.trigger_index))return 0;
 }
 TombItem *item=w->level->items+id;item->state=a->current;item->state_known=1;
 return 1;
}
static int16_t approach(int16_t a,int16_t b){int d=tomb_word(b-a);return tomb_word(a+(d>364?364:d< -364?-364:d));}
static int water_align(TombActor *a,const TombActor *o,const int16_t *sine,int16_t *pitch,int16_t *roll){
 int32_t offset[3]={0,0,108},pos[3];
 tomb_rotate_vector(o->yaw,0,0,offset,0,sine,pos);pos[0]+=o->x;pos[1]+=o->y;pos[2]+=o->z;
 int dx=pos[0]-a->x,dy=pos[1]-a->y,dz=pos[2]-a->z;
 int distance=(int)sqrt((double)((int64_t)dx*dx+(int64_t)dy*dy+(int64_t)dz*dz));
 if(distance>16){a->x+=dx*16/distance;a->y+=dy*16/distance;a->z+=dz*16/distance;}else{a->x=pos[0];a->y=pos[1];a->z=pos[2];}
 *pitch=approach(*pitch,0);*roll=approach(*roll,0);a->yaw=approach(a->yaw,o->yaw);
 return a->x==pos[0] && a->y==pos[1] && a->z==pos[2] && !*pitch && !*roll && a->yaw==o->yaw;
}
int tomb_water_switch_contact(TombObject *o,TombActor *a,TombAnimContext *c,uint32_t input,int water,int16_t *pitch,int16_t *roll){
 if(!(input&64) || water!=1 || a->current!=13 || (o->actor.flags&6) || (o->actor.current!=0 && o->actor.current!=1) || !tomb_city_bounds(a,&o->actor,*pitch,*roll,1,c->sine_quarter))return 1;
 if(!water_align(a,&o->actor,c->sine_quarter,pitch,roll))return 1;
 a->fall_speed=0;a->goal=40;int budget=120;while(a->current!=40 && budget--)if(!tomb_animate(a,c))return 0;
 if(a->current!=40)return 0;a->goal=13;c->weapon_status=1;
 o->actor.goal=o->actor.current==1?0:1;o->actor.flags=(o->actor.flags&~6u)|3;o->active=1;
 return tomb_object_animate(o,c);
}
/* KeyHoleCollision 0x346c4 and PuzzleHoleCollision 0x34904.
   Return 2 requests the original keys ring; 3 requests the refusal sound. */
int tomb_quest_contact(TombObject *o,TombActor *a,TombAnimContext *c,TombInventory *inv,int16_t *pitch,int16_t *roll,int action){
 int puzzle=o->object>=118 && o->object<=121,key=o->object>=137 && o->object<=140;
 if(!puzzle && !key)return 1;
 if(!tomb_switch_bounds(a,&o->actor,*pitch,*roll))return 1;
 if(puzzle && a->current==43 && a->frame==3372){o->object+=4;return 1;}
 if(a->current!=2 || c->weapon_status || (a->flags&8) || (!action && inv->chosen<0))return 1;
 if(o->actor.flags&6){inv->chosen=-1;return action?3:1;}
 if(inv->chosen==-1)return 2;
 int id=inv->chosen,expected=o->object-4;inv->chosen=-1;
 int slot=tomb_quest_slot(id);if(id!=expected || slot<0 || !inv->quest[slot])return 3;
 --inv->quest[slot];int32_t offset[3]={0,0,puzzle?327:362},v[3];
 tomb_rotate_vector(o->actor.yaw,0,0,offset,0,c->sine_quarter,v);
 a->x=o->actor.x+v[0];a->y=o->actor.y;a->z=o->actor.z+v[2];a->yaw=o->actor.yaw;*pitch=*roll=0;
 a->goal=puzzle?43:42;int target=a->goal,budget=120;while(a->current!=target && budget--)if(!tomb_animate(a,c))return 0;
 if(a->current!=target)return 0;a->goal=2;c->weapon_status=1;o->actor.flags=(o->actor.flags&~6u)|2;return 1;
}
static int fps_quest_contact(TombPlaytest *p,TombObject *o,int action){
 TombActor *a=&p->lara.actor;int64_t dx=(int64_t)a->x-o->actor.x,dz=(int64_t)a->z-o->actor.z;
 if(dx*dx+dz*dz>768*768 || abs(a->y-o->actor.y)>256 || abs(tomb_word(p->shooter.yaw-o->actor.yaw))>8192)return 1;
 TombCameraPoint from={a->x,a->y-700,a->z,a->room},to={o->actor.x,o->actor.y-512,o->actor.z,o->actor.room};
 double len=sqrt((double)(dx*dx+dz*dz));if(len>0){to.x+=(int32_t)(dx*64/len);to.z+=(int32_t)(dz*64/len);}
 if(!tomb_dos_camera_los(p->level,&from,&to,0))return 1;
 TombActor proxy=*a;proxy.animation=11;proxy.frame=185;proxy.current=proxy.goal=2;proxy.flags&=(uint8_t)~8u;
 proxy.y=o->actor.y;proxy.yaw=o->actor.yaw;
 int32_t offset[3]={0,0,400},v[3];tomb_rotate_vector(o->actor.yaw,0,0,offset,0,p->animation.sine_quarter,v);
 proxy.x=o->actor.x+v[0];proxy.z=o->actor.z+v[2];
 TombAnimContext context=p->animation;context.weapon_status=0;int16_t pitch=0,roll=0;
 int result=tomb_quest_contact(o,&proxy,&context,&p->inventory,&pitch,&roll,action);
 if(result==1 && (proxy.current==42 || proxy.current==43)){
  if(proxy.current==43)o->object+=4;
  const TombAnim *an=p->level->animations+proxy.animation;int at=an->command_index;
  for(int n=0;n<an->command_count && (size_t)at<p->level->command_count;n++){
   int op=p->level->commands[at++];int count=op==1?3:op==2 || op==5 || op==6?2:0;
   if((size_t)(at+count)>p->level->command_count)break;
   if(op==5 && context.event)context.event(context.user,5,p->level->commands[at+1],&o->actor);
   at+=count;
  }
 }
 return result;
}
int tomb_city_interact(TombPlaytest *p,uint32_t input){
 TombObjects *w=p->objects;TombActor *a=&p->lara.actor;TombAnimContext *c=&p->animation;
 if(!(input&64))p->quest_latch=0;
 /* BaddieCollision visits Lara's room and its direct portal neighbours.
    Use the room lists so moved blocks retain the original collision order. */
 tomb_enemies_room_sync(w);
 const TombRoomPortals *portals=w->visual->portals+a->room;int first=a->room;
 for(size_t pass=0;pass<=portals->count;pass++){
  int room=first;if(pass){const unsigned char *r=portals->data+32*(pass-1);room=r[0]|r[1]<<8;}
  for(int id=w->enemies->room_head[room];id>=0;id=w->enemies->room_next[id]){
  size_t i=(size_t)id;TombObject *o=w->items+i;
  if(!(o->actor.flags&32) || (o->actor.flags&6)==6 || llabs((long long)a->x-o->actor.x)>=4096 || llabs((long long)a->y-o->actor.y)>=4096 || llabs((long long)a->z-o->actor.z)>=4096)continue;
  if((o->object>=118 && o->object<=121) || (o->object>=137 && o->object<=140)){
   if(p->shooter.mode==TOMB_MODE_VR)continue;
   int result=p->shooter.mode==TOMB_MODE_FPS?fps_quest_contact(p,o,(input&64) && !p->quest_latch):tomb_quest_contact(o,a,c,&p->inventory,&p->lara.pitch,&p->movement.lean,(input&64) && !p->quest_latch);
   if(!result)return 0;
   if(result==2){int count=0;for(int q=0;q<8;q++)count+=p->inventory.quest[q];p->quest_latch=1;
    if(count)p->quest_request=1;else if(c->event)c->event(c->user,5,2,a);
    return 1;
   }
   if(result==3){p->quest_latch=1;if(c->event)c->event(c->user,5,2,a);}continue;
  }
  if(!(input&64))continue;
  if(o->object==56 && p->shooter.mode!=TOMB_MODE_FPS){
   int ok=tomb_water_switch_contact(o,a,c,input,p->lara.water_status,&p->lara.pitch,&p->lara.lean);
   p->water.pitch=p->lara.pitch;p->water.lean=p->lara.lean;
   if(!ok)return 0;continue;
  }
  if((o->object<48 || o->object>51) || p->lara.water_status || (o->actor.flags&6)==2 || (a->flags&8) || a->y!=o->actor.y)continue;
  int quadrant=(uint16_t)(a->yaw+8192)/16384;
  if(a->current==2){
   if((input&3) || c->weapon_status)continue;o->actor.yaw=tomb_word(quadrant*16384);
   if(!tomb_city_bounds(a,&o->actor,p->lara.pitch,p->movement.lean,0,c->sine_quarter))continue;
   if(quadrant==0)a->z=(a->z&~1023)+924;else if(quadrant==2)a->z=(a->z&~1023)+100;
   else if(quadrant==1)a->x=(a->x&~1023)+924;else a->x=(a->x&~1023)+100;
   a->yaw=o->actor.yaw;a->goal=38;if(!tomb_animate(a,c))return 0;if(a->current==38)c->weapon_status=1;return 1;
  }
  if(a->current!=38 || a->frame!=2091 || !tomb_city_bounds(a,&o->actor,p->lara.pitch,p->movement.lean,0,c->sine_quarter))continue;
  int pull=!(input&1);if(!(input&3) || !tomb_block_can_move(w,a,i,quadrant,pull))continue;
  o->actor.goal=pull?3:2;a->goal=pull?37:36;
  if(!tomb_block_floor(w,i,1024))return 0;o->active=1;o->actor.flags=(o->actor.flags&~6u)|3;
  if(!tomb_object_animate(o,c) || !tomb_animate(a,c))return 0;return 1;
  }
 }
 return 1;
}
