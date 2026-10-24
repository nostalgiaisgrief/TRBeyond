/* Persist state, never callback addresses or allocation pointers. The payload
   uses this build's POD layouts; version/layout/level checks reject mismatches.
   All bytes and allocations are checked before committing a restore. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "save.h"
#include "enemies.h"
#include "hazards.h"
#include "progression.h"
#include "peru.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SAVE_LIMIT (16u*1024u*1024u)
typedef struct Header { char magic[8]; uint32_t version,layout,level,level_hash,bytes,checksum; } Header;
typedef struct Stream { unsigned char *data; size_t at,size; int mode,ok,version; } Stream;
static uint32_t hash(const void *data,size_t n){const unsigned char *p=data;uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
static uint32_t layout_variant(int version,int runtime_vr){size_t sizes[]={(version<5?((offsetof(TombPlaytest,sidearm_type)+7)&~(size_t)7):version<6?((offsetof(TombPlaytest,gameplay_options)+7)&~(size_t)7):version<8?((offsetof(TombPlaytest,spasm_ticks)+7)&~(size_t)7):version<9?((offsetof(TombPlaytest,shooter)+7)&~(size_t)7):version<10?((offsetof(TombPlaytest,shooter)+offsetof(TombShooter,dual)+7)&~(size_t)7):version<11?((offsetof(TombPlaytest,swim_override)+7)&~(size_t)7):version<12?((offsetof(TombPlaytest,tps_slope_jump)+7)&~(size_t)7):version<13?((offsetof(TombPlaytest,fps_shot_held)+7)&~(size_t)7):version<14?((offsetof(TombPlaytest,fps)+7)&~(size_t)7):sizeof(TombPlaytest))-(version==1?2*sizeof(int):0)-(version<15?8:0),sizeof(TombLaraStart),sizeof(TombMovement),sizeof(TombObject),sizeof(TombDoor),(version<7?offsetof(TombCreature,neck):sizeof(TombCreature)),sizeof(TombNavigation),sizeof(TombHazards),(sizeof(TombProgress)-(version<15?8:0)),sizeof(TombFollowCamera),sizeof(TombInventory)};
 /* These two VR measurements are runtime-only and never enter state().
    Preserve the pre-VR signature; also recognise signatures emitted by 87. */
 if(version>=11 && !runtime_vr)sizes[0]-=sizeof(((TombPlaytest *)0)->vr_body_bottom)+sizeof(((TombPlaytest *)0)->vr_body_height);
 return hash(sizes,sizeof sizes)^(version>=3?(uint32_t)sizeof(TombPeru):0);
}
static uint32_t layout(int version){return layout_variant(version,0);}
static void chunk(Stream *s,void *p,size_t n){if(!s->ok || n>s->size-s->at){s->ok=0;return;}if(s->mode==0)memcpy(s->data+s->at,p,n);else if(s->mode==2)memcpy(p,s->data+s->at,n);s->at+=n;}
/* v1..14 stored the 168-byte gym record before separate PS1 trigger history. */
_Static_assert(offsetof(TombGym,last_music_trigger)==168,"Legacy gym layout");
_Static_assert(sizeof(TombGym)==176,"Current gym layout");
static void gym_state(Stream *s,TombGym *g){
 chunk(s,g,s->version<15?168:sizeof *g);
 if(s->mode==2 && s->version<15)g->last_music_trigger=g->current_track;
}
#define FIELD(v,f) chunk(s,&(v)->f,sizeof (v)->f)
static void state(Stream *s,TombPlaytest *p,TombObjects *w,TombFollowCamera *c,TombInventory *baseline){
 FIELD(p,ledge);FIELD(p,static_hit);FIELD(p,lara);FIELD(p,movement);FIELD(p,terrain);
 FIELD(p,animation.move_angle);FIELD(p,animation.fall_override);FIELD(p,animation.weapon_status);
 FIELD(p,collision.walk.move_angle);FIELD(p,collision.front_floor);FIELD(p,collision.front_type);FIELD(p,collision.lean);
 FIELD(p,air.move_angle);chunk(s,&p->water,offsetof(TombWater,query));gym_state(s,&p->gym);
 FIELD(p,slide.move_angle);FIELD(p,slide.last_angle);FIELD(p,slide.tilt_x);FIELD(p,slide.tilt_z);
 FIELD(p,movement_only);FIELD(p,requested_camera_mode);FIELD(p,requested_camera_elevation);FIELD(p,dead_ticks);
 FIELD(p,door_hit_ticks);FIELD(p,door_hit_direction);FIELD(p,blocked);
 FIELD(p,deferred_cd_track);FIELD(p,deferred_camera);FIELD(p,deferred_camera_flags);FIELD(p,requested_camera_angle);
 FIELD(p,camera_request);FIELD(p,pistols);FIELD(p,last_input);FIELD(p,inventory);FIELD(p,hud);FIELD(p,quest_request);FIELD(p,quest_latch);
 if(s->version>=2){FIELD(p,weapon_type);FIELD(p,requested_weapon);}
 if(s->version>=5)FIELD(p,sidearm_type);
 if(s->version>=6)FIELD(p,gameplay_options);
 if(s->version>=8){FIELD(p,spasm_ticks);FIELD(p,spasm_effect);}
 if(s->version>=9)chunk(s,&p->shooter,s->version<10?offsetof(TombShooter,dual):sizeof p->shooter);
 if(s->version>=12){FIELD(p,tps_slope_jump);FIELD(p,tps_drop_yaw_offset);}
 if(s->version>=11){FIELD(p,swim_override);FIELD(p,swim_jump_held);FIELD(p,swim_tap_ticks);}
 if(s->version>=13)FIELD(p,fps_shot_held);
 if(s->version>=14)FIELD(p,fps);
 chunk(s,c,sizeof *c);chunk(s,baseline,sizeof *baseline);
 if(s->version>=3){
  TombPeru peru=*w->peru;
  if(s->mode && s->ok && s->size-s->at>=sizeof peru)memcpy(&peru,s->data+s->at,sizeof peru);
  if(peru.flipped<0 || peru.flipped>1 || peru.scion_item< -1 || (peru.scion_item>=0 && (size_t)peru.scion_item>=w->count)){s->ok=0;return;}
  if(s->mode==2 && peru.flipped!=w->peru->flipped && !tomb_flip_map(w,0)){s->ok=0;return;}
  chunk(s,w->peru,sizeof *w->peru);
 }else if(s->mode==2){if(w->peru->flipped)tomb_flip_map(w,0);memset(w->peru,0,sizeof *w->peru);w->peru->effect=w->peru->scion_item=-1;}
 size_t count=s->version>=7?w->count:w->source_count;
 FIELD(w,deferred_actions);FIELD(w,camera);FIELD(w,camera_once);FIELD(w,last_target);
 chunk(s,w->items,count*sizeof *w->items);chunk(s,w->doors,count*sizeof *w->doors);
 chunk(s,w->hazards,sizeof *w->hazards);gym_state(s,&w->progress->music);chunk(s,&w->progress->secrets,sizeof *w->progress-sizeof w->progress->music);
 TombEnemies *e=w->enemies;FIELD(e,slots);FIELD(e,head);FIELD(e,random);FIELD(e,camera);
 if(s->version>=7)chunk(s,e->items,count*sizeof *e->items);else for(size_t j=0;j<count;j++)chunk(s,e->items+j,offsetof(TombCreature,neck));chunk(s,e->next,count*sizeof *e->next);
 chunk(s,e->room_head,w->level->room_count*sizeof *e->room_head);chunk(s,e->room_next,count*sizeof *e->room_next);chunk(s,e->room_id,count*sizeof *e->room_id);
 for(int i=0;i<8;i++){
  TombNavigation *n=e->navigation+i;uint32_t present=n->nodes!=NULL;
  if(s->mode && s->ok && s->size-s->at>=4)memcpy(&present,s->data+s->at,4);
  chunk(s,&present,4);if(present>1){s->ok=0;return;}
  chunk(s,&n->head,sizeof *n - offsetof(TombNavigation,head));
  if(present){if(s->mode==2 && !n->nodes){s->ok=0;return;}chunk(s,n->nodes,(w->level->box_count+1)*sizeof *n->nodes);}
  else if(s->mode==2){free(n->nodes);n->nodes=NULL;}
 }
 for(size_t i=0;i<w->level->room_count;i++){TombRoom *r=w->level->rooms+i;chunk(s,r->sectors,(size_t)r->nx*r->nz*sizeof *r->sectors);}
 chunk(s,w->level->boxes,w->level->box_count*sizeof *w->level->boxes);
 chunk(s,w->level->items,count*sizeof *w->level->items);
 if(s->version>=4){
  FIELD(w,current_strength);TombNavigation *n=w->current;
  if(!n || !n->nodes){s->ok=0;return;}
  chunk(s,&n->head,sizeof *n-offsetof(TombNavigation,head));
  chunk(s,n->nodes,(w->level->box_count+1)*sizeof *n->nodes);
 }else if(s->mode==2)tomb_current_reset(w);
}
#undef FIELD
static unsigned char *read_file(const char *path,Header *h){
 FILE *f=fopen(path,"rb");if(!f)return NULL;
 if(fread(h,sizeof *h,1,f)!=1 || memcmp(h->magic,"TOMBSV1",8) || (h->version!=1 && h->version!=2 && h->version!=3 && h->version!=4 && h->version!=5 && h->version!=6 && h->version!=7 && h->version!=8 && h->version!=9 && h->version!=10 && h->version!=11 && h->version!=12 && h->version!=13 && h->version!=14 && h->version!=15) || (h->layout!=layout((int)h->version) && h->layout!=layout_variant((int)h->version,1)) || h->level>15 || !h->bytes || h->bytes>SAVE_LIMIT){fclose(f);return NULL;}
 unsigned char *data=malloc(h->bytes);if(!data){fclose(f);return NULL;}
 int ok=fread(data,1,h->bytes,f)==h->bytes && fgetc(f)==EOF && hash(data,h->bytes)==h->checksum;fclose(f);if(!ok){free(data);return NULL;}return data;
}
int tomb_save_level(const char *path){Header h;unsigned char *data=read_file(path,&h);if(!data)return -1;free(data);return (int)h.level;}
int tomb_save_write(const char *path,int number,const TombPlaytest *p,const TombObjects *w,const TombFollowCamera *c,const TombInventory *baseline){
 if(number<0 || number>15 || !p || p->lara.health<=0 || !w || w->progress->complete)return 0;
 unsigned char *data=malloc(SAVE_LIMIT);if(!data)return 0;
 Stream s={data,0,SAVE_LIMIT,0,1,15};state(&s,(TombPlaytest *)p,(TombObjects *)w,(TombFollowCamera *)c,(TombInventory *)baseline);
 Header h={{'T','O','M','B','S','V','1',0},15,layout(15),(uint32_t)number,hash(w->level->file_data,w->level->file_size),(uint32_t)s.at,hash(data,s.at)};
 char temp[MAX_PATH];if(snprintf(temp,sizeof temp,"%s.tmp",path)>=(int)sizeof temp){free(data);return 0;}
 FILE *f=s.ok?fopen(temp,"wb"):NULL;int ok=f && fwrite(&h,sizeof h,1,f)==1 && fwrite(data,1,s.at,f)==s.at;
 if(f && fclose(f))ok=0;free(data);if(ok)ok=MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;if(!ok)DeleteFileA(temp);return ok;
}
int tomb_save_read(const char *path,int number,TombPlaytest *p,TombObjects *w,TombFollowCamera *c,TombInventory *baseline){
 Header h;unsigned char *data=read_file(path,&h);if(!data)return 0;
 if(h.level!=(uint32_t)number || h.level_hash!=hash(w->level->file_data,w->level->file_size)){free(data);return 0;}
 Stream s={data,0,h.bytes,1,1,(int)h.version};state(&s,p,w,c,baseline);
 if(!s.ok || s.at!=s.size){free(data);return 0;}
 if(h.version<7)tomb_objects_reset(w);
 TombNavNode *fresh[8]={0};
 for(int i=0;i<8;i++)if(!w->enemies->navigation[i].nodes){fresh[i]=calloc(w->level->box_count+1,sizeof *fresh[i]);if(!fresh[i]){for(int j=0;j<8;j++)free(fresh[j]);free(data);return 0;}}
 for(int i=0;i<8;i++)if(fresh[i])w->enemies->navigation[i].nodes=fresh[i];
 s.at=0;s.mode=2;state(&s,p,w,c,baseline);free(data);
 if(h.version==1)p->weapon_type=p->requested_weapon=1;
 if(h.version<5)p->sidearm_type=p->weapon_type==2?2:1;
 if(h.version<6)p->gameplay_options=0;
 if(h.version<8)p->spasm_ticks=p->spasm_effect=0;
 if(h.version<13)p->fps_shot_held=0;
 if(h.version<14)memset(&p->fps,0,sizeof p->fps);
 if(h.version<12)p->tps_slope_jump=p->tps_drop_yaw_offset=0;
 if(h.version<11)p->swim_override=p->swim_jump_held=p->swim_tap_ticks=0;
 if(h.version<10)p->shooter.dual=p->shooter.mode_held=p->shooter.next_hand=p->shooter.fire_delay=p->shooter.drop_latched=0;
 if(h.version<9)tomb_shooter_start(p,TOMB_MODE_ORIGINAL);
 if(h.version<7){
  /* Earlier saves predate reserved hatchlings. Restore their parent links
     and include the new children in the room lists after reading old lists. */
  size_t child=w->source_count;
  for(size_t i=0;i<w->source_count;i++)if(w->items[i].object==161 || w->items[i].object==163 || w->items[i].object==181){
   if(child<w->count && w->items[child].object>=0)w->enemies->items[i].required=(int16_t)(++child);
  }
  TombEnemies *e=w->enemies;
  for(size_t i=0;i<w->level->room_count;i++)e->room_head[i]=-1;
  for(size_t i=0;i<w->count;i++){
   int room=w->items[i].actor.room;e->room_id[i]=room;e->room_next[i]=e->room_head[room];e->room_head[room]=(int)i;
  }
 }
 /* Older saves may retain the completed cabin's last animation status,
    despite its removal when the lowered room replaced it. */
 for(size_t i=0;i<w->count;i++){
  TombObject *o=w->items+i;
  if(o->object==162 && !o->active && o->actor.current==4)
   o->actor.flags=(o->actor.flags&~33u)|6;
 }
 w->invincible=(p->gameplay_options&TOMB_INVINCIBLE)!=0;
 w->no_enemies=(p->gameplay_options&TOMB_NO_ENEMIES)!=0;
 p->sound_count=0;p->status="Game loaded";return s.ok;
}
