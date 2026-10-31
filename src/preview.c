#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <GL/gl.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "visual.h"
#include "picture.h"
#include "lighting.h"
#include "object_contact.h"
#include "lara_start.h"
#include "fixed.h"
#include "preview_camera.h"
#include "follow_camera.h"
#include "hazards.h"
#include "enemies.h"
#include "combat.h"
#include "progression.h"
#include "peru.h"
#include "folly.h"
#include "text.h"
#include "ring.h"
#include "room_visibility.h"
#include "preview_pose.h"
#include "render_interpolation.h"
#include "playtest.h"
#include "sound_win.h"
#include "level_select.h"
#include "city.h"
#include "save.h"
#include "cinematic.h"
#include "movie.h"

/* Modern presentation of reconstructed DOS simulation and lighting. */
static TombVisual *visual;
static TombLevel *level;
static TombLaraStart start;
static GLuint *tiles;
static TombPlaytest play;
/* Borrow the preceding level resources only while its cinematic is playing. */
static TombVisual *cinematic_equipment;
static GLuint *cinematic_equipment_tiles;
static int cinematic_sidearm,cinematic_shotgun;

static int settings_draw_distance=1; /* Original, maximum with fog, maximum without fog. */
static const char *draw_distance_names[]={"Original","Max Surfaces With Fog","Max Surfaces No Fog"};
static double world_far_distance=20480;
static double world_far_plane(void){
 if(settings_draw_distance==0)return 20480;
 double lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};
 for(size_t j=0;j<level->room_count;j++){
  const TombRoom *r=level->rooms+j;
  double a[3]={r->x,r->top,r->z},b[3]={(double)r->x+r->nx*1024,r->bottom,(double)r->z+r->nz*1024};
  for(int k=0;k<3;k++){lo[k]=fmin(lo[k],a[k]);hi[k]=fmax(hi[k],b[k]);}
 }
 double x=hi[0]-lo[0],y=hi[1]-lo[1],z=hi[2]-lo[2];
 return fmax(20480,sqrt(x*x+y*y+z*z)+8192);
}
static TombObjects objects;
static int16_t sine_table[1025];
static TombWaterEffect water_effect;
static uint32_t effect_ticks;
static int water_eye,water_room,water_object;
static size_t water_draw_room=SIZE_MAX;
static int room_translucent_pass;
static double effect_focal;
static double render_pixel_scale=1.0;
static int overlay_pass; /* 0: complete overlay, 1: scenery/ring, 2: sharp UI. */
static int pose_mode=0,caves=0;
static int cinema_mode,cinema_fov,cinema_roll,media_busy,media_skip,cutscene_seen,media_test,cutscene_only;
static const char *movie_only;
static int play_movie(const char *,int);
static int play_ending(int);
static int play_cutscene(int);
static int play_cutscene_sequence(int,int);
static double accumulator=0,camera_alpha=1;
static int running=1, paused=0, animation=11, width=960, height=720;
static double orbit=0, distance=1536, elapsed=0, camera_hold=0;
static TombFollowCamera camera;
static int camera_bounce;
static double shooter_previous[3],shooter_current[3];
/* Render-only weapon presentation; reset alongside the camera on load. */
static struct { int weapon; double lower,previous_lower,focus,previous_focus; } fps_view;
static double fps_view_sample(double previous,double current){
    double t=previous+camera_alpha*(current-previous);
    return t*t*(3-2*t);
}
static void fps_view_tick(void){
    if(!camera.ready || !fps_view.weapon || play.shooter.mode!=TOMB_MODE_FPS){
        fps_view.weapon=play.weapon_type;fps_view.lower=fps_view.previous_lower=0;
        fps_view.focus=fps_view.previous_focus=play.shooter.mode_held?1:0;
        return;
    }
    fps_view.previous_lower=fps_view.lower;fps_view.previous_focus=fps_view.focus;
    if(fps_view.weapon!=play.weapon_type){
        if(fps_view.lower<1){fps_view.lower+=.25;if(fps_view.lower>1)fps_view.lower=1;}
        else fps_view.weapon=play.weapon_type;
    }else if(fps_view.lower>0){fps_view.lower-=.25;if(fps_view.lower<0)fps_view.lower=0;}
    fps_view.focus+=play.shooter.mode_held?.25:-.25;
    if(fps_view.focus<0)fps_view.focus=0;if(fps_view.focus>1)fps_view.focus=1;
}
static double block_view_previous,block_view_current;
static double block_view_sample(void){
    return block_view_previous+camera_alpha*(block_view_current-block_view_previous);
}
static TombScreenRect *room_rects;
static int manual_camera=0;
static int playing_track=0,audio_paused=0;
static char audio_error[128];
static TombSoundBank sound_bank;
static TombSoundMixer sound_mixer,ring_sound_mixer;
static TombSoundOutput *sound_output,*ring_sound_output;
static int sound_failed;
static int draw_requested;
static void render_reset(void);
static int inventory_key_only,inventory_ring_current[3];
static int inventory_open,inventory_selected,inventory_stats,inventory_keys,level_number=1;
static double level_name_time=5.0;
static double detail_notice_time;
static char detail_notice[128];
static int inventory_capture,transition_test,peru_test,folly_test,midas_test;
static TombRing inventory_ring,ring_previous;
static int ring_interpolate;
static double ring_alpha=1;
static int16_t compass_previous;
static double inventory_accumulator;
static unsigned inventory_input;
static int ring_capture=-1,ring_object=99;
static int16_t compass_angle,compass_velocity;
static GLuint *font_tiles;
static TombInventory level_inventory;
static int load_next_level(void);
static int title_mode,options_ring,death_menu,slot_menu,slot_current,menu_action,option_row;
static int new_game_menu,new_game_level=1;
static int new_game_row,new_game_custom,new_game_mode,controls_page;
static unsigned shooter_bindings[9]={'W','S','A','D',VK_SHIFT,VK_SPACE,VK_LBUTTON,VK_RBUTTON,'E'};
static unsigned fps_bindings[10]={'W','S','A','D',VK_SHIFT,VK_SPACE,'E',VK_LBUTTON,VK_RBUTTON,VK_TAB};
static const char *fps_binding_names[10]={"Forward","Back","Left","Right","Walk","Jump","Action","Shoot","Weapon Mode","Inventory"};
static unsigned *active_mode_bindings(void){return play.shooter.mode==TOMB_MODE_FPS?fps_bindings:shooter_bindings;}
static const char *shooter_binding_names[9]={"Forward","Back","Left","Right","Walk","Jump","Action","Draw Weapon","Inventory"};
static uint32_t new_game_options;
static int home_from_level_select;
static int save_levels[17],settings_music=10,settings_sound=10,settings_detail=2,settings_vr_deadzone=18;
static int settings_framerate=0,settings_vsync=1,settings_force43=0;
/* Visual styles are independent of source IDs; Custom is a preset status. */
static const char *visual_style_names[]={"PS1","DOS","DOS 3DFX","PS1 Colour with DOS Effect"};
static const char *preset_names[]={"Optimal","DOS","DOS 3DFX","PS1"};
static int settings_water_style=3,settings_preset=0;
static int settings_full_colour=0,settings_texture_filtering=0;
static int water_ps1_colour(void){return settings_water_style==0 || settings_water_style==3;}
static int water_tint(int wet){return water_ps1_colour()?(water_eye || wet?2:0):settings_water_style==2?(water_eye || wet?1:0):water_eye;}
static int front_capture,front_test,autosave_switch_test;
static int door_render_test,block_render_test,remaining_render_test,jaw_render_test,jaw_diagnostic;
static FILE *jaw_log;
static int lighting_test,interpolation_test,water_render_test;
static char menu_error[128];
static TombLevel *title_level;
static TombVisual *title_visual;
static GLuint *title_tiles,*title_font_tiles,title_backdrop;
/* DOS default layout at 0xc20fc; native binding order puts Roll before Look. */
static unsigned bindings[13]={VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_DELETE,VK_NEXT,VK_SHIFT,VK_MENU,VK_CONTROL,VK_SPACE,VK_END,VK_NUMPAD0,VK_ESCAPE};
static const char *binding_names[13]={"Run","Back","Left","Right","Step Left","Step Right","Walk","Jump","Action","Draw Weapon","Roll","Look","Inventory"};
static int binding_wait;
static void mouse_release(void);
static void mouse_inventory_begin(void);
static void inventory_begin(void);
static int crystal_pending=-1,crystal_held,crystal_render;
static int passport_shortcut,passport_save;
static void refresh_slots(void);
static int backdrop_dirty,backdrop_shade;
static unsigned char *cinematic_pixels,cinematic_palette[768],cinematic_light[256];
static int cinematic_width,cinematic_height;
static void frontend_title(void);
static void ui_text(int,int,const char *,unsigned);
static double ui_scale(void);
static int load_game_level(int,int,const char *);
#include "preview_sources.h"
static void asset_path(char *path,const char *file){
 char relative[100];snprintf(relative,sizeof relative,"DATA/%s",file);
 size_t n=strlen(relative);if(!(source_legacy && source_selected==1) && n>4 && !_stricmp(relative+n-4,".PHD"))memcpy(relative+n-4,".TBL",5);
 if(source_selected==0 && n>4 && !_stricmp(relative+n-4,".PCX"))memcpy(relative+n-4,".tbi",5);
 source_path(path,relative);
}
static void game_level_path(char *path,int number){
 if(number>=UB_FIRST_LEVEL){char relative[80];snprintf(relative,sizeof relative,"DATA/%s",level_choices[number].file);ub_path(path,relative);}
 else asset_path(path,level_choices[number].file);
}
static void save_path(char *path,int slot){
 char base[MAX_PATH],folder[MAX_PATH];
 if(front_test)snprintf(folder,sizeof folder,"%s/build/test-saves",install_root);
 else if(campaign_selected==CAMPAIGN_COMBINED){
  if(source_legacy && source_selected==1)snprintf(base,sizeof base,"%s/saves",install_root);
  else{char root[MAX_PATH];source_base(root,source_selected);snprintf(base,sizeof base,"%s/SAVES",root);}
  CreateDirectoryA(base,NULL);snprintf(folder,sizeof folder,"%s/Combined",base);
 }
 else if(campaign_selected==CAMPAIGN_UB)ub_path(folder,"SAVES");
 else if(source_legacy && source_selected==1)snprintf(folder,sizeof folder,"%s/saves",install_root);
 else {source_base(base,source_selected);snprintf(folder,sizeof folder,"%s/SAVES",base);}
 CreateDirectoryA(folder,NULL);
 if(slot==16)snprintf(path,MAX_PATH,"%s/autosave.tsv",folder);
 else snprintf(path,MAX_PATH,"%s/slot%02d.tsv",folder,slot+1);
}
static TombInventory autosave_inventory;
static int autosave_switch_held(size_t index);
static int autosave_level_pending,autosave_switch_pending;
static struct AutosaveSwitch { int16_t goal;uint8_t status; } *autosave_switches;
static size_t autosave_switch_count;
static void autosave_reset(int new_level){
 autosave_inventory=play.inventory;autosave_level_pending=new_level;autosave_switch_pending=0;
 free(autosave_switches);autosave_switch_count=objects.source_count;
 autosave_switches=calloc(autosave_switch_count,sizeof *autosave_switches);
 if(!autosave_switches){autosave_switch_count=0;return;}
 for(size_t i=0;i<autosave_switch_count;i++){
  autosave_switches[i].goal=objects.items[i].actor.goal;
  autosave_switches[i].status=objects.items[i].actor.flags&6;
 }
}
/* Compare committed inventory at tick boundaries, after pickups, receptacles and
   Midas conversion have updated their world state. Selecting a key is not use. */
static int autosave_poll(void){
 if(title_mode || cinema_mode || media_busy)return 0;
 if(play.gameplay_options&TOMB_SAVE_CRYSTALS){autosave_level_pending=autosave_switch_pending=0;return 0;}
 int changed=memcmp(autosave_inventory.quest,play.inventory.quest,sizeof play.inventory.quest)!=0 ||
  autosave_inventory.counts[8]!=play.inventory.counts[8] || autosave_inventory.scion!=play.inventory.scion;
 /* Manual land-switch use leaves idle status and changes its goal. Timed
    automatic returns start active, so must not create a second autosave. */
 for(size_t i=0;i<autosave_switch_count && i<objects.count;i++){
  if(autosave_switch_held(i))continue; /* VR partial pulls are not committed. */
  const TombObject *o=objects.items+i;struct AutosaveSwitch *old=autosave_switches+i;
  if(o->object==55 && !old->status && old->goal!=o->actor.goal)
   autosave_switch_pending=1;
  old->goal=o->actor.goal;old->status=o->actor.flags&6;
 }
 if(!(play.gameplay_options&TOMB_AUTOSAVE) || play.lara.health<=0)autosave_switch_pending=0;
 /* Original/TPS finish Lara's pull before saving the resulting trigger state.
    FPS/VR dispatch the action directly, and do not enter these animations. */
 int switch_due=autosave_switch_pending && play.lara.actor.current!=40 && play.lara.actor.current!=41;
 if(switch_due)autosave_switch_pending=0;
 int due=autosave_level_pending || changed || switch_due;
 autosave_inventory=play.inventory;autosave_level_pending=0;
 if(!due || !(play.gameplay_options&TOMB_AUTOSAVE) || play.lara.health<=0 || objects.progress->complete)return 0;
 char path[MAX_PATH];save_path(path,16);
 int ok=tomb_save_write(path,level_number,&play,&objects,&camera,&level_inventory);
 if(!ok)fprintf(stderr,"Autosave could not be written\n");
 return ok;
}
static void settings_path(char *path){snprintf(path,MAX_PATH,"%s/settings.ini",install_root);}
static unsigned *control_bindings(void){return controls_page==2?fps_bindings:controls_page?shooter_bindings:bindings;}
#include "preview_settings.h"

static void vr_sound_listener(double listener[3],int *room);
static int vr_sound_yaw(void);
/* DOS speech and secret fanfare belong to Music & Speech, not world SFX. */
static TombLevel *speech_level;
static TombSoundBank speech_bank;
static TombSoundMixer speech_effect_mixer;
static TombSoundOutput *speech_effect_output;
static int speech_level_number=-1,speech_source=-1;
static void speech_free(void){
 tomb_sound_output_close(speech_effect_output);speech_effect_output=NULL;
 tomb_sound_bank_free(&speech_bank);tomb_level_free(speech_level);speech_level=NULL;
 speech_level_number=speech_source=-1;
}
static TombSoundBank *speech_samples(void){
 if(level_number>=UB_FIRST_LEVEL || music_source()==source_selected)return &sound_bank;
 if(speech_level_number!=level_number || speech_source!=music_source()){
  speech_free();speech_level_number=level_number;speech_source=music_source();
  char path[MAX_PATH],relative[80],error[128];
  snprintf(relative,sizeof relative,"DATA/%s",level_choices[level_number].file);
  size_t n=strlen(relative);
  if(!(source_legacy && music_source()==1) && n>4)memcpy(relative+n-4,".TBL",5);
  source_path_for(path,relative,music_source());
  speech_level=tomb_level_load(path,error,sizeof error);
  if(speech_level && tomb_sound_bank_load(&speech_bank,speech_level)){
   speech_bank.source_count=0; /* Never duplicate the other edition's ambient effects. */
   tomb_sound_init(&speech_effect_mixer,&speech_bank);
   speech_effect_output=tomb_sound_output_open(&speech_effect_mixer);
  }
 }
 return &speech_bank;
}
static void sound_tick(void) {
    double listener[3]={play.lara.actor.x,play.lara.actor.y-450,play.lara.actor.z};
    int room=play.lara.actor.room;
    if(play.shooter.mode==TOMB_MODE_VR)vr_sound_listener(listener,&room);
    if(play.shooter.mode==TOMB_MODE_SHOOTER || play.shooter.mode==TOMB_MODE_FPS)room=tomb_shooter_listener(&play,listener);
    else if(camera.ready && play.shooter.mode!=TOMB_MODE_VR){
        memcpy(listener,camera.target,sizeof listener);room=camera.room;
    }
    int wet=room>=0 && (size_t)room<level->room_count && (level->rooms[room].flags&1);
    double origin[3]={play.lara.actor.x,play.lara.actor.y,play.lara.actor.z};
    int yaw=play.lara.actor.yaw+play.lara.head_yaw+play.lara.torso_yaw;
    if(play.shooter.mode==TOMB_MODE_SHOOTER || play.shooter.mode==TOMB_MODE_FPS){memcpy(origin,listener,sizeof origin);yaw=play.shooter.yaw;}
    else if(play.shooter.mode==TOMB_MODE_VR){memcpy(origin,listener,sizeof origin);yaw=vr_sound_yaw();}
    TombSoundEvent effects[TOMB_SOUND_EVENTS],secrets[TOMB_SOUND_EVENTS];size_t ne=0,ns=0;
    for(size_t i=0;i<play.sound_count;i++){
      if(music_source()!=0 && music_selected!=source_selected && play.sounds[i].kind==5 && play.sounds[i].id==173)secrets[ns++]=play.sounds[i];
      else effects[ne++]=play.sounds[i];
    }
    if(ns)speech_samples();
    tomb_sound_output_spatial_frame(sound_output,effects,ne,objects.peru && objects.peru->flipped,wet,listener,origin,yaw);
    if(speech_effect_output)tomb_sound_output_spatial_frame(speech_effect_output,secrets,ns,0,wet,listener,origin,yaw);
}

static TombSoundOutput *music_output;
static TombSoundMixer music_mixer;
static TombSoundBank music_bank;
static TombSoundSample music_sample;
static unsigned char *music_data;
#include "music_ps1.h"
static unsigned playing_music_serial;
static TombPS1Music ps1_music;
static int requested_music_track;
static int combined_background;
static double combined_background_gain;
static void audio_close_stream(void) {
    tomb_sound_output_close(music_output);music_output=NULL;free(music_data);music_data=NULL;audio_error[0]=0;
}
static void audio_stop(void) {
    audio_close_stream();speech_free();playing_track=0;playing_music_serial=0;requested_music_track=0;
    ps1_music=(TombPS1Music){0};audio_paused=0;combined_background=0;combined_background_gain=0;
}
static void audio_start_track(int track,int gain) {
    audio_close_stream();playing_track=track;
    int source_selection=combined_background?1:music_source();
        if(track) {
            int source=source_selection==0?(track>=2 && track<=57?track:0):tomb_music_source(track),ready=0;
            /* DOS 38cf7 requests sound ID track+148 from the current level. */
            if(source==-3){
                TombSoundBank *bank=speech_samples();
                int detail=bank->map[track+148];
                if(bank->details && detail>=0 && (size_t)detail<bank->detail_count){
                    unsigned sample=bank->details[detail].sample;
                    if(sample<bank->sample_count){music_sample=bank->samples[sample];ready=1;}
                }
            }else{
                char file[MAX_PATH],relative[80];
                snprintf(relative,sizeof relative,"AUDIO/%s%02d.wav",source>0?"cd":"track",source>0?source:track);source_path_for(file,relative,source_selection);
                FILE *f=fopen(file,"rb");long bytes=0;
                if(f){if(!fseek(f,0,SEEK_END))bytes=ftell(f);rewind(f);if(bytes>0 && bytes<100000000)music_data=malloc((size_t)bytes);if(music_data && fread(music_data,1,(size_t)bytes,f)!=(size_t)bytes){free(music_data);music_data=NULL;}fclose(f);}
                ready=music_data && tomb_sound_sample(&music_sample,music_data,(size_t)bytes);
            }
            if(ready){
                memset(&music_bank,0,sizeof music_bank);music_bank.samples=&music_sample;music_bank.sample_count=1;tomb_sound_init(&music_mixer,&music_bank);
                music_mixer.voices[0]=(TombSoundVoice){.active=1,.volume=32767,.pitch=100,.mode=source_selection==0?(title_mode?2:0):(source>0?2:0)};music_output=tomb_sound_output_open_gain(&music_mixer,gain);
            }
            if(!music_output)snprintf(audio_error,sizeof audio_error,"Music track %d unavailable",track);
        }
}
static void audio_update(int suspended,double dt) {
    int track=title_mode?2:caves?objects.progress->music.current_track:play.gym.current_track;
    unsigned serial=title_mode?0:caves?objects.progress->music.audio_serial:play.gym.audio_serial;
    if(music_selected==3 && !title_mode){
        if(track<2 || track>57)track=0;
        int changed=track!=requested_music_track || serial!=playing_music_serial;
        if(changed){
            requested_music_track=track;playing_music_serial=serial;
            combined_background=0;combined_background_gain=0;
            /* A trigger starts immediately: no PS1 fade-out wait. */
            audio_start_track(track,32767);
        }
        int finished=!music_output || (!combined_background && music_sample.rate &&
            (uint64_t)tomb_sound_output_stats(music_output).played_samples*music_sample.rate >=
            (uint64_t)music_sample.frames*TOMB_SOUND_RATE);
        if(finished && !combined_background){
            combined_background=1;combined_background_gain=0;
            audio_start_track(tomb_music_level_track(level_number),0);
        }
        if(combined_background){
            if(!suspended)combined_background_gain=fmin(1.0,combined_background_gain+dt);
            tomb_sound_output_gain(music_output,(int)(32767*combined_background_gain));
        }
    }else if(music_source()==0){
        if(track<2 || track>57)track=0;
        /* Use the device playback position, not the mixer position (which is
           several buffers ahead), to recognize a completed one-shot. */
        if(!title_mode && ps1_music.active && music_output && music_sample.rate &&
           (uint64_t)tomb_sound_output_stats(music_output).played_samples*music_sample.rate >=
           (uint64_t)music_sample.frames*TOMB_SOUND_RATE)tomb_ps1_music_finished(&ps1_music);
        if(track!=requested_music_track || serial!=playing_music_serial){
            tomb_ps1_music_request(&ps1_music,track);
            requested_music_track=track;playing_music_serial=serial;
        }
        if(title_mode || !suspended){
            /* The USA executable installs its CD update in the vertical-blank
               callback (0x80053b04), independent of the 30 Hz game simulation. */
            ps1_music.clock+=dt*60.0;
            while(ps1_music.clock>=1.0){
                ps1_music.clock-=1.0;
                if(tomb_ps1_music_tick(&ps1_music))audio_start_track(ps1_music.active,0);
            }
        }
        tomb_sound_output_gain(music_output,ps1_music.gain*32767/129);
    }else if(track!=playing_track){
        audio_start_track(track,32767);playing_music_serial=serial;
    }
    tomb_sound_output_volume(music_output,settings_music);
    if(music_output)tomb_sound_output_update(music_output,(title_mode?0:suspended) || (music_selected==0 && ps1_music.stopped));
    tomb_sound_output_volume(speech_effect_output,settings_sound);tomb_sound_output_update(speech_effect_output,suspended);
    tomb_sound_output_volume(sound_output,settings_sound);tomb_sound_output_volume(ring_sound_output,settings_sound);
}
static void update_camera(double dt) {
    (void)dt;
    fps_view_tick();
    if(play.shooter.mode && play.lara.health>0 && !cinema_mode && !(objects.peru && objects.peru->scion_item>=0) && play.lara.actor.current!=50 && play.lara.actor.current!=51) {
        double pos[3];tomb_shooter_anchor(&play,pos);
        memcpy(camera.previous_eye,camera.eye,sizeof camera.eye);memcpy(camera.previous_target,camera.target,sizeof camera.target);
        memcpy(shooter_previous,shooter_current,sizeof shooter_current);memcpy(shooter_current,pos,sizeof pos);if(!camera.ready)memcpy(shooter_previous,pos,sizeof pos);
        block_view_previous=block_view_current;block_view_current=play.fps.block_view;
        if(!camera.ready)block_view_previous=block_view_current;
        tomb_shooter_view(&play,pos,camera.eye,camera.target,&camera.room);
        if(!camera.ready){memcpy(camera.previous_eye,camera.eye,sizeof camera.eye);memcpy(camera.previous_target,camera.target,sizeof camera.target);}
        camera.ready=1;camera.fixed_target=0;
        if(objects.enemies)for(int i=0;i<3;i++)objects.enemies->camera[i]=(int32_t)camera.eye[i];
        if(objects.peru)objects.peru->camera_target_y=(int32_t)camera.target[1];
        return;
    }
    if(!caves) {
        tomb_objects_camera_begin(&objects);
        TombSectorRef ref;TombHeights h;const TombActor *a=&play.lara.actor;
        if(tomb_find_sector(level,a->x,a->y,a->z,a->room,&ref) && tomb_static_heights(level,ref,a->x,a->z,0,&h))
            tomb_objects_trigger(&objects,h.trigger_index,h.floor==a->y);
    }
    if(objects.peru && objects.peru->scion_item>=0){
        TombDosCamera shot=camera.dos;int fov,roll;
        if(tomb_scion_camera(&objects,sine_table,&shot,&fov,&roll)){
            int cut=!camera.ready || camera.fixed_target!=-2;
            for(int i=0;i<3;i++){camera.previous_eye[i]=camera.eye[i];camera.previous_target[i]=camera.target[i];}
            camera.eye[0]=shot.eye.x;camera.eye[1]=shot.eye.y;camera.eye[2]=shot.eye.z;
            camera.target[0]=shot.target.x;camera.target[1]=shot.target.y;camera.target[2]=shot.target.z;
            if(cut){memcpy(camera.previous_eye,camera.eye,sizeof camera.eye);memcpy(camera.previous_target,camera.target,sizeof camera.target);}
            camera.dos=shot;camera.room=shot.eye.room;camera.ready=1;camera.fixed_target=-2;
            return;
        }
    }
    /* DOS normal-Lara effect 0x1df30 changes camera mode/FOV, retaining
       the cinematic eye and target for the next chase-camera tick. */
    if(camera.fixed_target==-2)camera.fixed_target=0;
    if(!camera.ready)camera_bounce=0;
    for(unsigned i=0;i<play.sound_count;i++)if(play.sounds[i].kind==10)camera_bounce=play.sounds[i].id;
    for(unsigned i=0;i<play.sound_count;i++)if(play.sounds[i].kind==8){
        TombActor impact={0};impact.x=play.sounds[i].x;impact.y=play.sounds[i].y;impact.z=play.sounds[i].z;
        camera_bounce=tomb_dos_camera_stomp(&camera.dos.eye,&impact,camera_bounce);
    }
    TombActor focus=play.lara.actor;int object=0;TombCameraRequest r=play.camera_request;
    r.pitch=play.lara.pitch;r.angle=tomb_word((int)r.angle+(int)(orbit*10430.378350470453));
    if(distance!=1536)r.distance=(int32_t)distance;
    if(objects.camera.active && (objects.camera.active==5 || !(r.flags&(TOMB_CAMERA_COMBAT|TOMB_CAMERA_LOOK)))) {
        if(objects.camera.active==5)r.flags&=~(TOMB_CAMERA_COMBAT|TOMB_CAMERA_LOOK);
        r.fixed_index=(int16_t)objects.camera.index;r.speed=objects.camera.speed;
        if(objects.camera.target>=0) {
            size_t id=(size_t)objects.camera.target;const TombObject *o=objects.items+id;TombModel m;
            object=o->object;r.item_target=1;focus=o->actor;
            if(!tomb_object_supported(object) && tomb_visual_model(visual,object,&m) && m.animation<level->animation_count){focus.animation=(int16_t)m.animation;focus.frame=level->animations[m.animation].first_frame;}
        }
    }
    if(!camera.ready && (play.gameplay_options&TOMB_SKIP_STATS))r.flags|=TOMB_CAMERA_SNAP;
    if(!tomb_camera_tick_effects(&camera,level,visual,&focus,object,sine_table,&r,&camera_bounce,&objects.enemies->random))play.status="DOS camera query failed";
    if(objects.enemies)for(int i=0;i<3;i++)objects.enemies->camera[i]=(int32_t)camera.eye[i];
    objects.peru->camera_target_y=(int32_t)camera.target[1];
}
static void pool_start(void) {
    tomb_objects_reset(&objects);
    audio_stop();tomb_sound_output_reset(sound_output);camera.ready=0;render_reset();paused=0;camera_hold=0;
    tomb_playtest_init(&play,level,sine_table,visual);
    play.lara.actor.x=40448; play.lara.actor.y=3328; play.lara.actor.z=57800;
    play.lara.actor.room=13; play.lara.actor.yaw=0;
    play.animation.move_angle=0;
}
/* Reproducible Caves slope fixture, also used by the native integration test. */
static void slope_start(int backward) {
    tomb_objects_reset(&objects);
    audio_stop();tomb_sound_output_reset(sound_output);camera.ready=0;render_reset();paused=0;camera_hold=0;
    orbit=0;distance=1536;elapsed=0;accumulator=0;pose_mode=0;
    tomb_playtest_init(&play,level,sine_table,visual);play.movement_only=1;play.objects=&objects;
    TombActor *a=&play.lara.actor;
    a->x=73216;a->y=1024;a->z=14848;a->room=0;a->yaw=backward?-16384:16384;
    play.animation.move_angle=a->yaw;
}
static void switch_start(int which) {
    static const unsigned switches[3]={10,42,52};
    tomb_objects_reset(&objects);
    audio_stop();tomb_sound_output_reset(sound_output);camera.ready=0;render_reset();paused=0;camera_hold=0;
    orbit=0;distance=1536;elapsed=0;accumulator=0;pose_mode=0;
    tomb_playtest_init(&play,level,sine_table,visual);play.movement_only=1;play.objects=&objects;
    const TombActor *s=&objects.items[switches[which]].actor;TombActor *a=&play.lara.actor;
    a->x=s->x+(s->yaw==16384?400:s->yaw==-16384?-400:0);
    a->z=s->z+(s->yaw==0?400:s->yaw==-32768?-400:0);
    a->y=s->y;a->room=s->room;a->yaw=s->yaw;play.animation.move_angle=a->yaw;
}
static void hazard_start(int which) {
    tomb_objects_reset(&objects);audio_stop();tomb_sound_output_reset(sound_output);
    camera.ready=0;render_reset();paused=0;camera_hold=0;orbit=0;distance=1536;elapsed=0;accumulator=0;pose_mode=0;
    tomb_playtest_init(&play,level,sine_table,visual);play.movement_only=1;play.objects=&objects;
    TombActor *a=&play.lara.actor;
    if(which==0){a->x=25088;a->y=4607;a->z=54784;a->room=14;a->yaw=-32768;}
    else if(which==1){a->x=36352;a->y=2048;a->z=74240;a->room=32;a->yaw=0;}
    else {a->x=74752;a->y=3072;a->z=22016;a->room=6;a->yaw=0;}
    play.animation.move_angle=a->yaw;
}
static void combat_start(int species) {
    audio_stop();tomb_sound_output_reset(sound_output);camera.ready=0;render_reset();paused=0;camera_hold=0;
    orbit=0;distance=1536;elapsed=0;accumulator=0;pose_mode=0;draw_requested=0;
    if(tomb_combat_fixture(&play,&objects,sine_table,species)<0)play.status="Combat fixture unavailable";
}
static void pickup_start(int large) {
    audio_stop();tomb_sound_output_reset(sound_output);camera.ready=0;render_reset();paused=0;camera_hold=0;
    orbit=0;distance=1536;elapsed=0;accumulator=0;pose_mode=0;draw_requested=0;inventory_open=0;
    if(tomb_pickup_fixture(&play,&objects,sine_table,large)<0)play.status="Pickup fixture unavailable";
}
static void city_start(int which) {
    audio_stop();tomb_sound_output_reset(sound_output);camera.ready=0;render_reset();paused=0;camera_hold=0;
    orbit=0;distance=1536;elapsed=0;accumulator=0;pose_mode=0;draw_requested=0;inventory_open=0;
    if(!tomb_city_fixture(&play,&objects,sine_table,which))play.status="City fixture unavailable";
}
static void interest_start(void){
    tomb_objects_reset(&objects);audio_stop();tomb_sound_output_reset(sound_output);camera.ready=0;render_reset();paused=0;camera_hold=0;accumulator=0;inventory_open=0;draw_requested=0;
    tomb_playtest_init(&play,level,sine_table,visual);play.objects=&objects;play.movement_only=1;
    play.lara.actor.x=78336;play.lara.actor.y=3072;play.lara.actor.z=32256;play.lara.actor.room=4;play.lara.actor.yaw=0;
}
static void finish_start(void) {
    tomb_objects_reset(&objects);audio_stop();tomb_sound_output_reset(sound_output);camera.ready=0;render_reset();paused=0;camera_hold=0;orbit=0;distance=1536;elapsed=0;accumulator=0;pose_mode=0;inventory_open=0;
    tomb_playtest_init(&play,level,sine_table,visual);play.movement_only=1;play.objects=&objects;
    if(level_number==2){play.lara.actor.x=9728;play.lara.actor.y=-1024;play.lara.actor.z=29184;play.lara.actor.room=87;}
    else if(level_number==3){const TombRoom *r=level->rooms+50;play.lara.actor.x=r->x+3*1024+512;play.lara.actor.z=r->z+512;play.lara.actor.room=50;TombSectorRef ref;TombHeights h;for(int j=0;j<r->nx*r->nz;j++){int x=r->x+j/r->nz*1024+512,z=r->z+j%r->nz*1024+512;if(tomb_find_sector(level,x,r->bottom,z,50,&ref) && tomb_static_heights(level,ref,x,z,0,&h) && (h.trigger_index==4238 || h.trigger_index==4241)){play.lara.actor.x=x;play.lara.actor.y=h.floor;play.lara.actor.z=z;break;}}}
    else {play.lara.actor.x=42496;play.lara.actor.y=7168;play.lara.actor.z=83456;play.lara.actor.room=11;}
}
static unsigned u16(const unsigned char *p) { return p[0]|((unsigned)p[1]<<8); }
static int s16(const unsigned char *p) { return tomb_word(u16(p)); }
static int32_t s32(const unsigned char *p) {
    return tomb_long((uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24));
}
static void colour(unsigned index,float shade) {
    const unsigned char *p=visual->palette+3*(index&255);
    glColor3f(p[0]/63.f*shade,p[1]/63.f*shade,p[2]/63.f*shade);
}
static void audio_route(const wchar_t *endpoint){
    /* Keep mixer positions/voices but replace WinMM handles bound to the old
       endpoint. This includes menu effects and the already-playing CD track. */
    int music=music_output!=NULL,world=sound_output!=NULL,ring=ring_sound_output!=NULL,speech=speech_effect_output!=NULL;
    tomb_sound_output_close(music_output);tomb_sound_output_close(sound_output);tomb_sound_output_close(ring_sound_output);
    tomb_sound_output_close(speech_effect_output);
    if(!tomb_sound_output_route(endpoint))fprintf(stderr,"VR audio endpoint unavailable; using Windows default output\n");
    music_output=music?tomb_sound_output_open(&music_mixer):NULL;
    sound_output=world?tomb_sound_output_open(&sound_mixer):NULL;
    ring_sound_output=ring?tomb_sound_output_open(&ring_sound_mixer):NULL;
    speech_effect_output=speech?tomb_sound_output_open(&speech_effect_mixer):NULL;
    tomb_sound_output_volume(music_output,settings_music);tomb_sound_output_volume(speech_effect_output,settings_sound);
    tomb_sound_output_volume(sound_output,settings_sound);tomb_sound_output_volume(ring_sound_output,settings_sound);
}
#include "preview_xr.h"
static int autosave_switch_held(size_t index){return play.shooter.mode==TOMB_MODE_VR && (vr.hands[0].lever==(int)index || vr.hands[1].lever==(int)index);}
static int vr_hand_ring(void){return inventory_open && vr.menu_open && !title_mode && !death_menu && !objects.progress->complete;}
static int inventory_pauses(void){return inventory_open && (crystal_pending>=0 || !vr_hand_ring());}
#include "preview_lighting.h"
#include "preview_interpolation.h"
static void mesh(const TombMeshView *m) {
    int interpolated=0;
    if(render_track<render_count){
        int slot=render_node++,identity=(int)(m-visual->meshes);float matrix[16];
        if(render_recording){glGetFloatv(GL_MODELVIEW_MATRIX,matrix);tomb_render_record(render_tracks+render_track,render_tick,slot,render_parent,identity,matrix);return;}
        if(tomb_render_sample(render_tracks+render_track,render_tick,slot,identity,camera_alpha,matrix)){
            glPushMatrix();glLoadMatrixd(lighting_view);glMultMatrixf(matrix);interpolated=1;
        }
    }
    GLdouble matrix[16];glGetDoublev(GL_MODELVIEW_MATRIX,matrix);
    /* DOS polygon winding rejection (e.g. 41981); screen Y is inverted here. */
    glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(GL_CW);
    int32_t rotation[9],local[3];
    for(int i=0;i<3;i++)for(int j=0;j<3;j++)rotation[i*3+j]=(int32_t)lround(matrix[j*4+i]*16384);
    tomb_light_vector(&model_light,rotation,local);
    light_UseProgram(shade_program);lighting_options(0);light_ActiveTexture(0x84c1);glBindTexture(GL_TEXTURE_2D,tiles[visual->tile_count*2]);light_ActiveTexture(0x84c0);
    int room_far=m->vertex_stride==8 && settings_draw_distance==0;
    int room_fog=m->vertex_stride==8 && settings_draw_distance!=2;
    light_Uniform1i(shade_room_far,room_far);
    int warp=settings_water_style!=0 && m->vertex_stride==8 && (water_eye!=water_room);
    light_Uniform1i(shade_water,water_tint(m->vertex_stride==8?water_room:water_object));light_Uniform1i(shade_warp,warp);
    for(int group=0;group<4;group++) {
        int n=(group%2)?3:4;
        for(size_t f=0;f<m->face_count[group];f++) {
            const unsigned char *face=m->faces[group]+f*(n+1)*2;
            if(room_far){
                int beyond=0;
                for(int j=0;j<n;j++){
                    const unsigned char *p=m->vertices+u16(face+2*j)*m->vertex_stride;
                    int depth=(int)floor(-(matrix[2]*s16(p)+matrix[6]*s16(p+2)+matrix[10]*s16(p+4)+matrix[14]));
                    beyond+=depth>20480;
                }
                /* DOS room faces reject only when every vertex is beyond far. */
                if(beyond==n)continue;
            }
            unsigned material=u16(face+2*n);
            const unsigned char *texture=NULL;
            if(group<2) {
                texture=visual->textures+20*tomb_visual_texture(visual,material&32767,effect_ticks);
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D,tiles[(lighting_rgba()?0:visual->tile_count)+(u16(texture+2)&32767)]);
                lighting_filter();
                if(settings_texture_filtering){
                    unsigned lo[2]={65535,65535},hi[2]={0,0};
                    for(int j=0;j<n;j++)for(int c=0;c<2;c++){unsigned v=u16(texture+4+j*4+c*2)>>8;if(v<lo[c])lo[c]=v;if(v>hi[c])hi[c]=v;}
                    light_Uniform4f(shade_bounds,(lo[0]+.5f)/256,(lo[1]+.5f)/256,(hi[0]+.5f)/256,(hi[1]+.5f)/256);
                }
                if(u16(texture)==1) glEnable(GL_ALPHA_TEST); else glDisable(GL_ALPHA_TEST);
            } else { glDisable(GL_TEXTURE_2D); glDisable(GL_ALPHA_TEST); }
            int water_surface=texture && water_draw_room!=SIZE_MAX && m->vertex_stride==8 && tomb_visual_water_face(visual,water_draw_room,face,n);
            int translucent=texture && (water_surface?water_ps1_colour():visual->rgba_tiles && u16(texture)==2);
            /* PS1 ABR 0: half foreground plus half background, after opaque geometry. */
            if(m->vertex_stride==8 && translucent!=room_translucent_pass)continue;
            light_Uniform1i(shade_surface,translucent);
            if(water_surface && !translucent)glEnable(GL_ALPHA_TEST);
            light_Uniform1i(shade_material,crystal_render?-6:texture?(lighting_rgba()?-4:-1):(int)(material&255));
            if(translucent){glDisable(GL_ALPHA_TEST);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);}
            glBegin(n==4?GL_QUADS:GL_TRIANGLES);
            for(int j=0;j<n;j++) {
                unsigned index=u16(face+2*j);
                const unsigned char *p=m->vertices+index*m->vertex_stride;
                int shade=tomb_light_vertex(m,index,&model_light,local);
                if(m->vertex_stride==8){
                    int depth=(int)floor(-(matrix[2]*s16(p)+matrix[6]*s16(p+2)+matrix[10]*s16(p+4)+matrix[14]));
                    shade=room_fog?tomb_light_room_depth(s16(p+6),depth):s16(p+6);
                    if(water_room)shade=tomb_water_effect_shade(&water_effect,m->vertex_count-index,effect_ticks,shade);
                }
                float dx=0,dy=0;
                if(warp){
                    double depth=-(matrix[2]*s16(p)+matrix[6]*s16(p+2)+matrix[10]*s16(p+4)+matrix[14]);
                    if(depth>=10){
                        double scale=height/480.0;int ox,oy;
                        /* Keep wave phase attached to the world, not projected
                           pixels: camera motion must not advance the wave.
                           Shared vertices across room boundaries use the same phase. */
                        const TombRoom *room=level->rooms+water_draw_room;
                        int wx=room->x+s16(p),wy=s16(p+2),wz=room->z+s16(p+4);
                        tomb_water_effect_offset(&water_effect,effect_ticks,
                            tomb_asr(wx,7)+tomb_asr(wz,8),
                            tomb_asr(wy,7)+tomb_asr(wz,7),&ox,&oy);
                        dx=(float)(ox*scale*render_pixel_scale/width);dy=(float)(-oy*scale/height);
                    }
                }
                glColor3f((float)(shade<0?0:shade>8191?8191:shade)/8192.f,.5f+dx,.5f+dy);
                /* PS1 UVs use quad-strip order even though converted faces
                   use perimeter order. Triangles retain their three UVs. */
                int uv=visual->level->asset.edition==2 && n==4 && j>=2?5-j:j;
                if(texture)glTexCoord2f(u16(texture+4+uv*4)/65536.f,u16(texture+6+uv*4)/65536.f);
                glVertex3f((float)s16(p),(float)s16(p+2),(float)s16(p+4));
            }
            glEnd();
            if(translucent){glDisable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_TRUE);}
        }
    }
    light_Uniform1i(shade_surface,0);light_Uniform1i(shade_room_far,0);glDisable(GL_CULL_FACE);light_UseProgram(0);if(interpolated)glPopMatrix();
}
static void rotate_pose(const TombPose *a,const TombPose *b,int joint,float t) {
    GLfloat rotation[16];
    tomb_preview_rotation(a->rotation[joint],b->rotation[joint],t,rotation);
    glMultMatrixf(rotation);
}
static int pistol_pose(int weapon,int frame,TombPose *p) {
    int an=weapon==4?(frame<13?164:frame<47?165:frame<80?166:frame<114?167:168):(frame<5?160:frame<13?161:frame<24?162:163);
    return tomb_visual_key(visual,(size_t)an,(size_t)(frame-level->animations[an].first_frame),p);
}
/* CUT1/2/4 Lara uses the ordinary 15-part skeleton. Original mesh swaps
   remain authoritative for whether each hand/holster contains a weapon. */
static const TombMeshView *cinematic_weapon_mesh(int object,int part,const TombMeshView *original){
    if(!cinematic_equipment || object!=77)return NULL;
    TombModel base,guns,shot;
    if(!tomb_visual_model(cinematic_equipment,0,&base))return NULL;
    if(part==7 && cinematic_shotgun && tomb_visual_model(cinematic_equipment,2,&shot))return cinematic_equipment->meshes+shot.mesh_start+7;
    if(part!=1 && part!=4 && part!=10 && part!=13)return NULL;
    /* Vanilla cutscene bare thighs have 26 vertices, bare hands have 8.
       Gun-bearing meshes have 34 and 24 respectively, including effect 14. */
    int occupied=original->vertex_count==(unsigned)(part==1 || part==4?34:24);
    if(!occupied)return NULL;
    int id=cinematic_sidearm==2?3:cinematic_sidearm==3?4:1;
    if(!tomb_visual_model(cinematic_equipment,id,&guns))return NULL;
    return cinematic_equipment->meshes+guns.mesh_start+part;
}
static void cinematic_mesh(int object,int part,const TombMeshView *original){
    const TombMeshView *replacement=cinematic_weapon_mesh(object,part,original);
    if(!replacement){mesh(original);return;}
    TombVisual *saved=visual;GLuint *saved_tiles=tiles;size_t track=render_track;
    visual=cinematic_equipment;tiles=cinematic_equipment_tiles;render_track=SIZE_MAX;
    mesh(replacement);visual=saved;tiles=saved_tiles;render_track=track;
}
static void gun_flash(void) {
    TombModel flash;if(!tomb_visual_model(visual,166,&flash) || !flash.mesh_count)return;
    glPushMatrix();glTranslatef(0,155,55);glRotatef(-16380*(360.f/65536.f),1,0,0);
    /* Presentation RNG is separate from the original gameplay RNG. */
    int saved_node=render_node,saved_parent=render_parent;render_parent=saved_node-1;render_node=saved_node>11?63:62;
    TombLighting saved=model_light;tomb_light_static(0x1400,lighting_depth(),&model_light);
    glRotatef((float)fmod(elapsed*13871,360),0,0,1);mesh(visual->meshes+flash.mesh_start);model_light=saved;glPopMatrix();render_node=saved_node;render_parent=saved_parent;
}
static GLfloat fps_hand_view[16]; /* Render regression capture, populated only in front-end tests. */
static int fps_overlay_active(void) {
    return !pose_mode && !cinema_mode && play.shooter.mode==TOMB_MODE_FPS &&
        play.lara.health>0 && !(objects.peru && objects.peru->scion_item>=0) &&
        play.lara.actor.current!=50 && play.lara.actor.current!=51 &&
        (render_recording?play.fps.block_view==0:block_view_sample()<=0);
}
static int draw_lara_actor(const TombObject *clone) {
    if(!clone && !pose_mode && play.shooter.mode==TOMB_MODE_VR)return 1;
    int fps=!clone && fps_overlay_active();
    if(fps && (render_recording || !tomb_fps_ready(&play)))return 1;
    int view_weapon=fps && fps_view.weapon?fps_view.weapon:play.weapon_type;
    int view_sidearm=fps?(view_weapon==4?1:view_weapon):play.sidearm_type;
    int view_drawn=fps?3:play.pistols.drawn;
    float focus=fps?(float)fps_view_sample(fps_view.previous_focus,fps_view.focus):0;
    float lower=fps?(float)fps_view_sample(fps_view.previous_lower,fps_view.lower):0;
    int left_frame=play.pistols.left.frame,right_frame=play.pistols.right.frame;
    if(fps && view_weapon!=play.weapon_type)left_frame=right_frame=view_weapon==4?47:4;
    TombModel model; TombPose a,b; uint16_t selected[15];
    if(!tomb_visual_model(visual,0,&model) || !tomb_visual_lara_meshes(visual,!caves,selected)) return 0;
    const TombActor *actor=clone?&clone->actor:pose_mode?&start.actor:&play.lara.actor;
    TombActor view_actor;
    if(fps){
        view_actor=*actor;view_actor.animation=11;view_actor.frame=level->animations[11].first_frame;actor=&view_actor;
        render_track=SIZE_MAX;glClear(GL_DEPTH_BUFFER_BIT);
    }else if(!pose_mode)render_select(clone?render_object_key(clone):0,actor,0);
    int selected_animation=pose_mode?animation:actor->animation;
    const TombAnim *anim=level->animations+selected_animation;
    int rate=visual->animations[32*selected_animation+4];
    double frame=pose_mode?fmod(elapsed*30,anim->last_frame-anim->first_frame+1):actor->frame-anim->first_frame;
    int key=(int)frame/rate;
    float fraction=(float)(frame-key*rate)/rate;
    if(!tomb_visual_key(visual,(size_t)selected_animation,(size_t)key,&a)) return 0;
    if(!tomb_visual_key(visual,(size_t)selected_animation,(size_t)key+1,&b)) b=a;
    if(!fps && !pose_mode && play.door_hit_ticks && play.door_hit_direction>=0 && play.door_hit_direction<4) {
        static const unsigned hits[4]={125,127,126,128};
        if(!tomb_visual_key(visual,hits[play.door_hit_direction],(size_t)play.door_hit_ticks,&a))return 0;
        b=a;fraction=0;
    }
    if(a.count!=15 || b.count!=15) return 0;
    TombPose left,right;int armed=!pose_mode && caves && play.pistols.status>=2 && play.pistols.status<=4;
    if(armed && (!pistol_pose(view_weapon,left_frame,&left) || !pistol_pose(view_weapon,right_frame,&right)))return 0;
    if(caves && !pose_mode) {
        TombModel guns,face;if(!tomb_visual_model(visual,view_sidearm==2?3:view_sidearm==3?4:1,&guns))guns=model;
        if(!play.inventory.counts[view_sidearm==2?2:view_sidearm==3?3:0])guns=model;
        int pistol_drawn=view_weapon==4?0:view_drawn;
        selected[1]=(uint16_t)((pistol_drawn&1?model.mesh_start:guns.mesh_start)+1);
        selected[4]=(uint16_t)((pistol_drawn&2?model.mesh_start:guns.mesh_start)+4);
        if(pistol_drawn&1)selected[13]=(uint16_t)(guns.mesh_start+13);
        if(pistol_drawn&2)selected[10]=(uint16_t)(guns.mesh_start+10);
        TombModel shotgun;
        if(play.inventory.counts[1] && tomb_visual_model(visual,2,&shotgun)) {
            if(view_weapon==4 && view_drawn){selected[10]=(uint16_t)(shotgun.mesh_start+10);selected[13]=(uint16_t)(shotgun.mesh_start+13);}
            else selected[7]=(uint16_t)(shotgun.mesh_start+7);
        }
        if(play.pistols.status==4 && ((play.shooter.mode?play.shooter.input:play.last_input)&64) && tomb_visual_model(visual,4,&face))selected[14]=(uint16_t)(face.mesh_start+14);
    }
    if(!pose_mode && actor->current==46 && actor->animation!=139){
        TombModel extra;if(!tomb_visual_model(visual,5,&extra) || extra.mesh_count!=15)return 0;
        for(int i=0;i<15;i++)selected[i]=(uint16_t)(extra.mesh_start+i);
    }
    if(!pose_mode && actor->current==51){
        TombModel gold;if(!tomb_visual_model(visual,5,&gold))return 0;
        uint32_t mask=tomb_midas_gold(actor->frame-level->animations[actor->animation].first_frame-1);
        for(int j=0;j<15;j++)if(mask&(1u<<j))selected[j]=(uint16_t)(gold.mesh_start+j);
    }
    /* DOS DrawDoppelganger (293d4) substitutes all fifteen meshes, then calls
       DrawLara with the clone actor and the live Lara head/torso/arm state. */
    if(clone){
        TombModel skin;if(!tomb_visual_model(visual,6,&skin) || skin.mesh_count!=15)return 0;
        for(int j=0;j<15;j++)selected[j]=(uint16_t)(skin.mesh_start+j);
        model=skin;
    }
    /* Explicit CPU copy of GL matrices avoids depending on driver stack depth. */
    GLfloat stack[15][16]; int depth=0,parents[15],parent=0;
    glPushMatrix();
    if(fps){
        /* A view model has no world translation or camera interpolation phase. */
        glLoadIdentity();glScalef(1,-1,-1);
        /* Keep forearm cutoffs below the viewport. Focus shifts toward centre,
           retaining an offset so the sight picture stays unobstructed. */
        glTranslatef(0,lower*650.f,0);
        glTranslatef(view_weapon==4?40.f-35.f*focus:-35.f*focus,
            view_weapon==4?960.f-40.f*(left_frame<=50?1.f:left_frame<57?(57.f-left_frame)/7.f:left_frame>74?(left_frame-74.f)/5.f:0.f):735.f,view_weapon==4?200.f:0.f);
        if(view_weapon==4){
            /* Enlarge around the barrel area, rather than moving it toward the near plane. */
            glTranslatef(0,-180,220);glScalef(1.55f,1.55f,1.55f);glTranslatef(0,180,-220);
        }
    }else {
        glTranslatef((float)actor->x,(float)actor->y,(float)actor->z);
        glRotatef((clone || pose_mode?actor->yaw:tomb_shooter_render_yaw(&play))*(360.f/65536.f),0,1,0);
        if(!pose_mode){
            const TombCreature *c=clone?objects.enemies->items+(clone-objects.items):NULL;
            glRotatef((c?c->pitch:play.lara.pitch)*(360.f/65536.f),1,0,0);glRotatef((c?c->roll:play.movement.lean)*(360.f/65536.f),0,0,1);
        }
    }
    int intensity=-1;for(size_t i=0;i<level->item_count;i++)if(level->items[i].object==0){intensity=level->items[i].intensity;break;}
    actor_lighting(actor,&a,intensity,pose_mode?0:play.lara.pitch,pose_mode?0:play.movement.lean);
    GLfloat base[16];glGetFloatv(GL_MODELVIEW_MATRIX,base);
    glTranslatef(a.root[0]+fraction*(b.root[0]-a.root[0]),a.root[1]+fraction*(b.root[1]-a.root[1]),a.root[2]+fraction*(b.root[2]-a.root[2]));
    rotate_pose(&a,&b,0,fraction); if(!fps)mesh(visual->meshes+selected[0]);
    for(int i=1;i<15;i++) {
        const unsigned char *bone=visual->trees+4*model.tree_word+(i-1)*16;
        int flags=s32(bone);
        if(flags&1) { if(!depth) { glPopMatrix(); return 0; } parent=parents[depth-1]; glLoadMatrixf(stack[--depth]); }
        if(flags&2) { if(depth>=15) { glPopMatrix(); return 0; } parents[depth]=parent; glGetFloatv(GL_MODELVIEW_MATRIX,stack[depth++]); }
        glTranslatef((float)s32(bone+4),(float)s32(bone+8),(float)s32(bone+12));
        if(fps && i==11 && view_weapon!=4)glTranslatef(0,focus*650.f,0);
        if(armed && i>=8 && i<=13) {
            const TombGunArm *arm=i<11?&play.pistols.right:&play.pistols.left;
            const TombPose *pose=i<11?&right:&left;
            if(view_weapon!=4 && (i==8 || i==11)) {
                GLfloat matrix[16];glGetFloatv(GL_MODELVIEW_MATRIX,matrix);
                for(int col=0;col<3;col++)for(int row=0;row<3;row++)matrix[4*col+row]=base[4*col+row];
                glLoadMatrixf(matrix);glRotatef((fps?0:arm->yaw)*(360.f/65536.f),0,1,0);glRotatef((fps?0:arm->pitch)*(360.f/65536.f),1,0,0);glRotatef(arm->roll*(360.f/65536.f),0,0,1);
            }
            rotate_pose(pose,pose,i,0);
        } else rotate_pose(&a,&b,i,fraction);
        if(!fps && !pose_mode && (i==7 || i==14)) {
            glRotatef((i==7?play.lara.torso_yaw:play.lara.head_yaw)*(360.f/65536.f),0,1,0);
            glRotatef((i==7?play.lara.torso_pitch:play.lara.head_pitch)*(360.f/65536.f),1,0,0);
        }
        if(fps && front_test && i==10)glGetFloatv(GL_MODELVIEW_MATRIX,fps_hand_view);
        render_parent=parent;
        if(!fps ||
           (i>=9 && i<=10) || ((focus<1 || view_weapon==4) && i>=12 && i<=13))
            mesh(visual->meshes+selected[i]);
        parent=i;
        if(armed && (!fps || (view_weapon==play.weapon_type && lower==0)) && view_weapon!=4 && ((i==10 && play.pistols.right.flash) || (i==13 && play.pistols.left.flash && (!fps || play.shooter.dual))))gun_flash();
    }
    glPopMatrix();render_track=SIZE_MAX; return 1;
}
static int lara(void){return draw_lara_actor(NULL);}
/* Eight-segment bolts and two branches. Drawing randomness is independent of
   gameplay; the controller fixes the target and branch seed for each pulse. */
static void draw_lightning(const TombObject *o){
    if(render_recording || o<objects.items || o>=objects.items+objects.count)return;
    const TombCreature *c=objects.enemies->items+(o-objects.items);if(!(c->flags&1))return;
    float points[8][3];uint32_t seed=c->touch^(uint32_t)c->required*0x9e3779b9u;
    for(int i=0;i<8;i++){
        float t=i/7.f;points[i][0]=o->actor.x+(c->target_x-o->actor.x)*t;points[i][1]=o->actor.y+(c->target_y-o->actor.y)*t;points[i][2]=o->actor.z+(c->target_z-o->actor.z)*t;
        if(i && i<7)for(int j=0;j<3;j++){seed=seed*1664525u+1013904223u;points[i][j]+=((int)(seed>>24)-128)*2.f;}
    }
    glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_LINE_BIT|GL_DEPTH_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D);glDisable(GL_BLEND);glDepthMask(GL_FALSE);glLineWidth(2.f);glColor3f(.65f,.8f,1.f);
    glBegin(GL_LINE_STRIP);for(int i=0;i<8;i++)glVertex3fv(points[i]);glEnd();
    seed=c->touch;
    for(int branch=0;branch<2;branch++){
        int start=tomb_control_random(&seed)*7/32767;float end[3]={(float)c->target_x, (float)c->target_y,(float)c->target_z};
        end[0]+=tomb_control_random(&seed)*1024/32767;end[2]+=tomb_control_random(&seed)*1024/32767;
        glBegin(GL_LINE_STRIP);glVertex3fv(points[start]);for(int j=1;j<=4;j++){float t=j/4.f;glVertex3f(points[start][0]+(end[0]-points[start][0])*t+(j<4?((j&1)?50.f:-50.f):0),points[start][1]+(end[1]-points[start][1])*t,points[start][2]+(end[2]-points[start][2])*t);}glEnd();
    }
    glPopAttrib();
}
static void vr_upright_sprite_matrix(const float position[16],double m[16]){
    double head[3];tomb_vr_point(&vr,vr.input.head.p,head);double dx=head[0]-position[12],dz=head[2]-position[14],n=hypot(dx,dz);
    if(n<1){dx=0;dz=-1;n=1;}
    memset(m,0,16*sizeof *m);m[0]=-dz/n;m[2]=dx/n;m[5]=-1;m[8]=dx/n;m[10]=dz/n;
    for(int j=0;j<3;j++)m[12+j]=position[12+j];m[15]=1;
}
static void draw_billboard(int sprite,const float position[16],int intensity,int pickup_room) {
    const unsigned char *p=visual->sprite_textures+16*sprite;
    GLdouble view[16];glGetDoublev(GL_MODELVIEW_MATRIX,view);double v[3];
    for(int i=0;i<3;i++)v[i]=view[i]*position[12]+view[4+i]*position[13]+view[8+i]*position[14]+view[12+i];
    if(v[2]>=-10 || v[2]<=-world_far_distance)return;
    float left=(float)s16(p+8),top=-(float)s16(p+10),right=(float)s16(p+12),bottom=-(float)s16(p+14);
    float u=p[2]/256.f,t=p[3]/256.f,du=(u16(p+4)+1)/65536.f,dv=(u16(p+6)+1)/65536.f;
    glPushMatrix();
    if(play.shooter.mode==TOMB_MODE_VR && xr.render_eye>=0 && !cinema_mode){
        double m[16];vr_upright_sprite_matrix(position,m);glMultMatrixd(m);
    }else{glLoadIdentity();glTranslatef((float)v[0],(float)v[1],(float)v[2]);}
    glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glBindTexture(GL_TEXTURE_2D,tiles[(lighting_rgba()?0:visual->tile_count)+u16(p)]);lighting_filter();glColor3f(1,1,1);
    int depth=settings_draw_distance==2?0:(int)-v[2];
    int shade=settings_draw_distance==1?fmin(8191,fmax(0,intensity+fmax(0,depth-12288))):tomb_sprite_shade((int16_t)intensity,depth);
    if(shade<0){glPopMatrix();return;}
    /* Keep the image on the original billboard, but fold its depth proxy onto
       a supporting plane. The proxy is evaluated along each viewing ray, so
       image size/UVs stay unchanged and solid foreground objects still occlude. */
    int pickup=pickup_room>=0;
    light_UseProgram(pickup?pickup_program:shade_program);
    light_Uniform4f(pickup?pickup_bounds:shade_bounds,u+.5f/256,t+.5f/256,u+du-.5f/256,t+dv-.5f/256);
    if(pickup){
        double normal[3]={0,1,0};TombSectorRef ref;TombHeights h;
        if(tomb_find_sector(level,(int)position[12],(int)position[13],(int)position[14],pickup_room,&ref) &&
           tomb_static_heights(level,ref,(int)position[12],(int)position[14],0,&h) && abs(h.floor-(int)position[13])<256){
            for(int axis=0;axis<2;axis++){
                int x=(int)position[12],z=(int)position[14];TombHeights lo,hi;
                if(tomb_static_heights(level,ref,x-(axis==0?32:0),z-(axis==1?32:0),0,&lo) &&
                   tomb_static_heights(level,ref,x+(axis==0?32:0),z+(axis==1?32:0),0,&hi) && abs(hi.floor-lo.floor)<=128)
                    normal[axis==0?0:2]=-(hi.floor-lo.floor)/64.0;
            }
        }
        double n[3];for(int j=0;j<3;j++)n[j]=view[j]*normal[0]+view[4+j]*normal[1]+view[8+j]*normal[2];
        light_Uniform4f(pickup_plane,(float)n[0],(float)n[1],(float)n[2],(float)(n[0]*v[0]+n[1]*v[1]+n[2]*v[2]-8));
    }
    light_ActiveTexture(0x84c1);glBindTexture(GL_TEXTURE_2D,tiles[visual->tile_count*2]);light_ActiveTexture(0x84c0);glColor3f(shade/8192.f,0,0);lighting_options(pickup);light_Uniform1i(pickup?pickup_material:shade_material,lighting_rgba()?-4:-1);light_Uniform1i(pickup?pickup_water:shade_water,water_tint(water_object));if(!pickup)light_Uniform1i(shade_warp,0);
    glBegin(GL_QUADS);glTexCoord2f(u,t);glVertex3f(left,top,0);glTexCoord2f(u+du,t);glVertex3f(right,top,0);glTexCoord2f(u+du,t+dv);glVertex3f(right,bottom,0);glTexCoord2f(u,t+dv);glVertex3f(left,bottom,0);glEnd();light_UseProgram(0);glPopMatrix();
}
static int draw_object(const TombObject *o) {
    water_object=(level->rooms[o->actor.room].flags&1)!=0;
    if(o->object==6)return draw_lara_actor(o);
    if(o->object==168){
        const TombActor *a=&o->actor;if(a->animation<0 || (size_t)a->animation>=visual->mesh_count)return 0;
        render_select(render_object_key(o),a,0);glPushMatrix();glTranslatef((float)a->x,(float)a->y,(float)a->z);
        glRotatef(a->yaw*(360.f/65536.f),0,1,0);glRotatef(a->current*(360.f/65536.f),1,0,0);glRotatef(a->goal*(360.f/65536.f),0,0,1);
        TombLighting old=model_light;int32_t point[3]={a->x,a->y,a->z};tomb_light_room(visual->lighting+a->room,point,lighting_depth(),sine_table,&model_light);light_in_view();mesh(visual->meshes+a->animation);model_light=old;glPopMatrix();render_track=SIZE_MAX;return 1;
    }
    /* SetupObjects assigns DrawDummy (1ac80) to these controllers. */
    if(o->object==170 || o->object==183 || o->object==128 || o->object==179 || o->object==177)return 1;
    TombModel m;TombPose a,b;const TombActor *actor=&o->actor;
    int pod_open=0;if(o->object==163 || o->object==181)for(size_t j=0;j<objects.count;j++)if(o==objects.items+j)pod_open=objects.enemies->items[j].flags!=0;
    render_select(render_object_key(o),actor,(o->object>=48 && o->object<=51));
    const TombCreature *creature=NULL;
    if((tomb_enemy_object(o->object) || o->object==24) && objects.enemies)for(size_t i=0;i<objects.count;i++)if(o==objects.items+i){creature=objects.enemies->items+i;break;}
    if(!tomb_visual_model(visual,o->object,&m) || !m.mesh_count || m.mesh_count>64 || actor->animation<0 || (size_t)actor->animation>=level->animation_count)return 0;
    const TombAnim *anim=level->animations+actor->animation;
    int rate=visual->animations[32*actor->animation+4],frame=actor->frame-anim->first_frame;
    if(!rate || (frame<0 && o->object!=29))return 0;
    int key=frame/rate;float t=(float)(frame%rate)/rate;
    if(o->object==29){
        if(!tomb_visual_skateboard_key(visual,actor->animation,key,&a))return 0;
        if(!tomb_visual_skateboard_key(visual,actor->animation,key+1,&b))b=a;
    }else{
        if(!tomb_visual_key(visual,actor->animation,(size_t)key,&a) || a.count!=m.mesh_count)return 0;
        if(!tomb_visual_key(visual,actor->animation,(size_t)key+1,&b))b=a;
    }
    glPushMatrix();glTranslatef((float)actor->x,(float)actor->y,(float)actor->z);
    glRotatef(actor->yaw*(360.f/65536.f),0,1,0);
    if(creature){glRotatef(creature->pitch*(360.f/65536.f),1,0,0);glRotatef(creature->roll*(360.f/65536.f),0,0,1);}
    int intensity=-1;for(size_t i=0;i<objects.count;i++)if(o==objects.items+i){intensity=level->items[i].intensity;break;}
    actor_lighting(actor,&a,intensity,creature?creature->pitch:0,creature?creature->roll:0);
    glTranslatef(a.root[0]+t*(b.root[0]-a.root[0]),a.root[1]+t*(b.root[1]-a.root[1]),a.root[2]+t*(b.root[2]-a.root[2]));
    int door=o->object>=57 && o->object<=64;
    /* Open panels can lie exactly on room walls. Resolve equal-depth surfaces
       in favour of the panel without moving its geometry or collision. */
    if(door){glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(-1,-1);}
    rotate_pose(&a,&b,0,t);crystal_render=o->object==83;mesh(visual->meshes+m.mesh_start);crystal_render=0;
    GLfloat stack[64][16];int depth=0,parents[64],parent=0;
    for(unsigned i=1;i<m.mesh_count;i++) {
        const unsigned char *bone=visual->trees+4*m.tree_word+(i-1)*16;int flags=s32(bone);
        if(flags&1){if(!depth){glPopMatrix();return 0;}parent=parents[depth-1];glLoadMatrixf(stack[--depth]);}
        if(flags&2){if(depth>=64){glPopMatrix();return 0;}parents[depth]=parent;glGetFloatv(GL_MODELVIEW_MATRIX,stack[depth++]);}
        glTranslatef((float)s32(bone+4),(float)s32(bone+8),(float)s32(bone+12));rotate_pose(&a,&b,(int)i,t);
        if(creature && ((o->object==34 && i==2) || ((o->object>=30 && o->object<=32) && i==1) || (o->object==7 && i==3) || (o->object==8 && i==14) || (o->object==18 && (i==11 || i==12)) || (o->object==19 && i==22) || (o->object==24 && i==3) || ((o->object==27 || o->object==28) && i==7) || ((o->object==10 || o->object==11) && i==8) || ((o->object==12 || o->object==13 || o->object==14) && i==20) || (o->object==15 && i==14) || ((o->object==16 || o->object==17) && i==2)))glRotatef(creature->head*(360.f/65536.f),0,1,0);
        if(creature && o->object>=20 && o->object<=22){if(i==1)glRotatef(creature->head*(360.f/65536.f),0,1,0);if(i==3)glRotatef(creature->neck*(360.f/65536.f),0,1,0);}
        if(creature && o->object==33 && i==3){glRotatef(creature->head*(360.f/65536.f),1,0,0);glRotatef(creature->neck*(360.f/65536.f),0,0,1);}
        if(creature && o->object==23 && i==11){glRotatef(creature->head*(360.f/65536.f),0,1,0);glRotatef(creature->neck*(360.f/65536.f),1,0,0);}
        if((o->object==163 || o->object==181) && i>=9 && (pod_open || i<=23) && !render_recording){++render_node;parent=(int)i;continue;}
        render_parent=parent;if(o->object==46){++render_node;}else if(((o->object!=24 || i<11 || i>14) && ((o->object!=21 && o->object!=22) || i<15 || i>20)) || render_recording)cinematic_mesh(o->object,(int)i,visual->meshes+m.mesh_start+i);else ++render_node;parent=(int)i;
    }
    if(door)glDisable(GL_POLYGON_OFFSET_FILL);
    glPopMatrix();render_track=SIZE_MAX;
    if(o->object==30){
        TombModel board,kid;if(!tomb_visual_model(visual,29,&board) || !tomb_visual_model(visual,30,&kid))return 0;
        TombObject skateboard=*o;skateboard.object=29;skateboard.actor.animation=(int16_t)(actor->animation-kid.animation+board.animation);
        render_secondary=1+objects.count+TOMB_DART_CAPACITY+256+(size_t)(o-objects.items);
        int ok=draw_object(&skateboard);render_secondary=SIZE_MAX;if(!ok)return 0;
    }
    if(o->object==46)draw_lightning(o);
    if(o->object==44){TombObject head;if(!tomb_folly_head(&objects,o,&head))return 0;
        render_secondary=1+objects.count+TOMB_DART_CAPACITY+256+(size_t)(o-objects.items);
        int ok=draw_object(&head);render_secondary=SIZE_MAX;return ok;
    }
    return 1;
}
/* Inventory models use their resting pose at native world scale. Centre their
   footprint on the pickup and put the bottom above its authored support. */
static int draw_pickup_model(const TombObject *o,int16_t visual_yaw){
    int id=tomb_pickup_model(o->object);TombModel model;TombPose pose;
    if(id<0 || !tomb_visual_model(visual,(unsigned)id,&model) || !model.mesh_count || model.mesh_count>64 ||
       !tomb_visual_key(visual,model.animation,0,&pose) || pose.count!=model.mesh_count)return 0;
    float cx=(pose.bounds[0]+pose.bounds[1])*.5f,cz=(pose.bounds[4]+pose.bounds[5])*.5f;
    float bottom=pose.bounds[3],support=(float)o->actor.y;
    int held=-1;
    if(play.shooter.mode==TOMB_MODE_VR && vr.ready)for(int h=0;h<2;h++){
        int i=vr.hands[h].pickup;
        if(i>=0 && (size_t)i<objects.count && o==objects.items+i && vr.input.hand[h].valid)held=h;
    }
    /* A nearby slope may rise through the footprint. Use its highest corner,
       but never move floating/held pickups down to a remote floor. */
    TombSectorRef ref;TombHeights floor;
    if(held<0 && tomb_find_sector(level,o->actor.x,o->actor.y,o->actor.z,o->actor.room,&ref) &&
       tomb_static_heights(level,ref,o->actor.x,o->actor.z,0,&floor) && abs(floor.floor-o->actor.y)<32){
        for(int x=0;x<2;x++)for(int z=0;z<2;z++){
            int32_t local[3]={(int32_t)(pose.bounds[x]-cx),0,(int32_t)(pose.bounds[4+z]-cz)},point[3];
            tomb_rotate_vector(visual_yaw,0,0,local,0,sine_table,point);
            int px=o->actor.x+point[0],pz=o->actor.z+point[2];
            if(tomb_find_sector(level,px,o->actor.y,pz,o->actor.room,&ref) &&
               tomb_static_heights(level,ref,px,pz,0,&floor) && abs(floor.floor-o->actor.y)<256 && floor.floor<support)support=(float)floor.floor;
        }
    }
    glPushMatrix();
    if(held>=0){
        double grip[16];tomb_vr_matrix(&vr,&vr.input.hand[held],grip);grip[13]+=vr.view_offset;
        glMultMatrixd(grip);glRotated(180,1,0,0);
        /* Centre the held model on the palm. Every mesh shares this full
           controller transform, including wrist roll and pitch. */
        bottom=(pose.bounds[2]+pose.bounds[3])*.5f;
    }else{glTranslatef((float)o->actor.x,support-2,(float)o->actor.z);glRotatef(visual_yaw*(360.f/65536.f),0,1,0);}
    TombPose light_pose=pose;for(int i=0;i<6;i++)light_pose.bounds[i]=(int16_t)(pose.bounds[i]-(i<2?cx:i<4?bottom:cz));
    TombActor light_actor=o->actor;light_actor.y=(int32_t)support-2;light_actor.yaw=visual_yaw;
    /* Sprite intensity is not a model shade. Room lighting initializes all
       lighting fields, avoiding inherited direction from the previous draw. */
    actor_lighting(&light_actor,&light_pose,-1,0,0);
    glTranslatef(pose.root[0]-cx,pose.root[1]-bottom,pose.root[2]-cz);
    rotate_pose(&pose,&pose,0,0);mesh(visual->meshes+model.mesh_start);
    GLfloat stack[64][16];int depth=0;
    for(unsigned j=1;j<model.mesh_count;j++){
        const unsigned char *bone=visual->trees+4*model.tree_word+(j-1)*16;int flags=s32(bone);
        if(flags&1){if(!depth){glPopMatrix();return 0;}glLoadMatrixf(stack[--depth]);}
        if(flags&2){if(depth>=64){glPopMatrix();return 0;}glGetFloatv(GL_MODELVIEW_MATRIX,stack[depth++]);}
        glTranslatef((float)s32(bone+4),(float)s32(bone+8),(float)s32(bone+12));rotate_pose(&pose,&pose,(int)j,0);
        mesh(visual->meshes+model.mesh_start+j);
    }
    glPopMatrix();return 1;
}
static void draw_hazard_effect(const TombHazardEffect *e,size_t track,int intensity) {
    water_object=e->room>=0 && (size_t)e->room<level->room_count && (level->rooms[e->room].flags&1);
    if(e->id==172 || e->id==173){
        TombModel m;if(!tomb_visual_model(visual,e->id,&m))return;
        float position[16]={0};position[12]=(float)e->x;position[13]=(float)e->y;position[14]=(float)e->z;
        if(track<render_count)tomb_render_sample(render_tracks+track,render_tick,0,e->id,camera_alpha,position);
        glPushMatrix();glTranslatef(position[12],position[13],position[14]);glRotatef(e->yaw*(360.f/65536.f),0,1,0);glRotatef(e->frame*(360.f/65536.f),1,0,0);
        TombLighting old=model_light;tomb_light_static((int16_t)(e->id==172?3584:4096),lighting_depth(),&model_light);mesh(visual->meshes+m.mesh_start);model_light=old;glPopMatrix();return;
    }
    if(e->id==166){TombModel m;if(!tomb_visual_model(visual,166,&m))return;
        glPushMatrix();glTranslatef((float)e->x,(float)e->y,(float)e->z);glRotatef(e->yaw*(360.f/65536.f),0,1,0);glRotatef(e->speed*(360.f/65536.f),0,0,1);
        TombLighting old=model_light;tomb_light_static(4096,lighting_depth(),&model_light);mesh(visual->meshes+m.mesh_start);model_light=old;glPopMatrix();return;
    }
    int sprite=tomb_hazard_sprite(visual,e->id,e->frame,NULL);if(sprite<0)return;
    float position[16]={0};position[12]=(float)e->x;position[13]=(float)e->y;position[14]=(float)e->z;
    if(track<render_count)tomb_render_sample(render_tracks+track,render_tick,0,e->id,camera_alpha,position);
    draw_billboard(sprite,position,intensity,tomb_pickup_object(e->id)?e->room:-1);
}

#include "preview_water.h"
#include "preview_shadow.h"
#ifdef TOMB_BUILD_TESTS
#include "../tests/lighting_render.h"
#endif

static int make_tiles(void) {
    shadow_palette=NULL;
    if(!lighting_program()){MessageBoxA(NULL,"Lighting requires OpenGL 2.0 support. Check the graphics driver. See stderr for shader diagnostics.","Renderer initialization",MB_ICONERROR);return 0;}
    tiles=calloc(visual->tile_count*2+2,sizeof *tiles); unsigned char *rgba=malloc(65536*4);
    if(!tiles || !rgba){free(tiles);free(rgba);tiles=NULL;return 0;}
    glGenTextures((GLsizei)(visual->tile_count*2+2),tiles);
    lighting_textures();
    for(size_t i=0;i<visual->tile_count;i++) {
        for(size_t j=0;j<65536;j++) {
            if(visual->rgba_tiles){memcpy(rgba+j*4,visual->rgba_tiles+(i*65536+j)*4,4);continue;}
            unsigned index=visual->tiles[i*65536+j];
            for(int c=0;c<3;c++) rgba[j*4+c]=(unsigned char)(visual->palette[index*3+c]*255/63);
            rgba[j*4+3]=index?255:0;
        }
        glBindTexture(GL_TEXTURE_2D,tiles[i]); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    }
    font_tiles=calloc(visual->tile_count,sizeof *font_tiles);if(!font_tiles){free(rgba);return 0;}
    /* DOS screen sprites use shade 0x1000: light-map row 16. Index zero
       remains transparent even when the palette's black entry differs. */
    const unsigned char *light=level->file_data+level->item_parsed_bytes+4096;
    for(unsigned tile=0;tile<visual->tile_count;tile++) {
        glGenTextures(1,font_tiles+tile);
        for(size_t j=0;j<65536;j++) {
            if(visual->rgba_tiles){memcpy(rgba+j*4,visual->rgba_tiles+((size_t)tile*65536+j)*4,4);continue;}
            unsigned index=visual->tiles[tile*65536+j],mapped=light[index];
            for(int c=0;c<3;c++)rgba[j*4+c]=(unsigned char)(visual->palette[mapped*3+c]*255/63);
            rgba[j*4+3]=index?255:0;
        }
        glBindTexture(GL_TEXTURE_2D,font_tiles[tile]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    }
    free(rgba);
    return 1;
}
static int load_game_level(int next_number,int carry,const char *saved) {
    crystal_pending=-1;crystal_held=0;
    xr.reset=1;
    free(cinematic_pixels);cinematic_pixels=NULL;
    if(next_number<0 || next_number>=LEVEL_CHOICE_COUNT)return 0;
    char path[MAX_PATH],error[256]={0};game_level_path(path,next_number);
    TombLevel *next=tomb_level_load(path,error,sizeof error);TombVisual *view=NULL;
    if(next)next->music_edition=music_source()==0?2:1;
    TombObjects world={0};TombPlaytest player;TombLaraStart spawn;TombScreenRect *rects=NULL;
    if(!next || !(view=tomb_visual_load(next,error,sizeof error)) || !tomb_lara_start(next,&spawn) ||
       !(rects=calloc(next->room_count,sizeof *rects)) || !tomb_objects_init(&world,next,view) ||
       !tomb_playtest_init(&player,next,sine_table,view)) {
        tomb_objects_free(&world);free(rects);tomb_visual_free(view);tomb_level_free(next);
        MessageBoxA(NULL,"Could not load the next level. Check that its converted level file is installed.","Next level",MB_ICONERROR);return 0;
    }
    TombFollowCamera saved_camera={0};TombInventory saved_baseline={0};
    player.objects=&world;player.movement_only=next_number!=0;
    if(next_number==0){player.inventory.counts[0]=0;player.inventory.ammo[0]=0;}
    if(saved && !tomb_save_read(saved,next_number,&player,&world,&saved_camera,&saved_baseline)){
        tomb_objects_free(&world);free(rects);tomb_visual_free(view);tomb_level_free(next);strcpy(menu_error,"Could not load this save");return 0;
    }
    uint32_t carried_options=carry?play.gameplay_options:0;
    int carried_mode=carry?play.shooter.mode:0;
    int carried_weapon=play.weapon_type,carried_sidearm=play.sidearm_type;
    TombInventory carried=carry?play.inventory:player.inventory;carried.pickups=carried.ticks=0;carried.pickup_ticks=0;carried.last_pickup=-1;memset(carried.quest,0,sizeof carried.quest);carried.counts[8]=0;carried.chosen=-1;
    if(next_number==13 && carry && !saved)tomb_inventory_confiscate(&carried);
    audio_stop();tomb_sound_output_close(ring_sound_output);ring_sound_output=NULL;tomb_sound_output_close(sound_output);sound_output=NULL;tomb_sound_bank_free(&sound_bank);
    glDeleteTextures((GLsizei)visual->tile_count,font_tiles);free(font_tiles);
    glDeleteTextures((GLsizei)(visual->tile_count*2+2),tiles);free(tiles);free(room_rects);
    tomb_objects_free(&objects);tomb_visual_free(visual);tomb_level_free(level);
    level=next;visual=view;objects=world;play=player;start=spawn;room_rects=rects;
    /* init installs callbacks pointing at the player itself, so rebuild after moving. */
    tomb_playtest_init(&play,level,sine_table,visual);play.objects=&objects;play.movement_only=1;
    if(saved){
        /* Re-read only after all level resources are ready. Callback pointers
           remain those installed at the final address by init above. */
        if(!tomb_save_read(saved,next_number,&play,&objects,&saved_camera,&saved_baseline)){running=0;return 0;}
        camera=saved_camera;level_inventory=saved_baseline;
        /* Saved current_track is trigger history, not a playback request.
           Speech has no saved playback position/completion state. Mark the
           restored speech as already handled without restarting its sample;
           retain current_track and one-shot masks to avoid retriggering it. */
        int restored_track=next_number?objects.progress->music.current_track:play.gym.current_track;
        if(music_source()==0 || tomb_music_source(restored_track)==-3)playing_track=restored_track;
        if(music_source()==0){requested_music_track=restored_track;playing_music_serial=next_number?objects.progress->music.audio_serial:play.gym.audio_serial;}
    }else{play.inventory=carried;level_inventory=carried;play.gameplay_options=carried_options;tomb_enemies_configure(&objects,(carried_options&TOMB_NO_ENEMIES)!=0);tomb_shooter_start(&play,carried_mode);
        if(carried_options&TOMB_CONSISTENT_WEAPONS){
            int slot=carried_weapon==4?1:carried_weapon==2?2:carried_weapon==3?3:0;
            if(play.inventory.counts[slot])play.weapon_type=play.requested_weapon=carried_weapon;
            slot=carried_sidearm==2?2:carried_sidearm==3?3:0;
            if(play.inventory.counts[slot])play.sidearm_type=carried_sidearm;
        }
    }
    tomb_pickups_replace_owned(&objects,&play.inventory);
    home_from_level_select=0;cutscene_seen=0;water_particles_reset();
    detail_notice_time=0;
    level_name_time=saved?0:5.0;
    level_number=next_number;if(campaign_selected!=CAMPAIGN_COMBINED)campaign_selected=next_number>=UB_FIRST_LEVEL;caves=next_number!=0;play.movement_only=caves;play.objects=caves?&objects:NULL;
    /* Every load must rebase tracking to the new spawn/save. New Game used to
       do this alone, leaving completion transitions in the previous world. */
    tomb_vr_reset(&vr,&play);xr.panel_open=0;
    title_mode=death_menu=slot_menu=options_ring=new_game_menu=0;
    inventory_open=inventory_stats=inventory_selected=draw_requested=paused=0;
    render_reset();
    if(!saved){camera.ready=0;render_reset();objects.progress->music.current_track=music_source()==0?0:tomb_music_level_track(next_number);}
    camera_bounce=0;pose_mode=0;orbit=0;distance=1536;camera_hold=elapsed=accumulator=0;
    if(!make_tiles()){running=0;return 0;}
    sound_failed=0;if(tomb_sound_bank_load(&sound_bank,level)){tomb_sound_init(&sound_mixer,&sound_bank);sound_output=tomb_sound_output_open(&sound_mixer);
            tomb_sound_init(&ring_sound_mixer,&sound_bank);ring_sound_output=tomb_sound_output_open(&ring_sound_mixer);}
    if(!sound_output || !ring_sound_output)sound_failed=1;if(!saved)update_camera(1.0/30);
    if(!saved && !front_test && !transition_test){
        const char *movie=next_number==0?"MANSION":next_number==1?"SNOW":next_number==5?"LIFT":next_number==10?"VISION":next_number==13?"CANYON":next_number==14?"PYRAMID":NULL;
        if(movie)play_movie(movie,0);
    }autosave_reset(!saved);return 1;
}
static int load_next_level(void){
 if(campaign_selected==CAMPAIGN_COMBINED && level_number>0){
  if(level_number==15)return load_game_level(18,1,NULL);
  if(level_number==19){if(!front_test && !transition_test)play_movie("END",0);return load_game_level(16,1,NULL);}
  if(level_number==17){if(!front_test && !transition_test)play_ending(0);frontend_title();return 1;}
  if(level_number>=UB_FIRST_LEVEL)return load_game_level(level_number+1,1,NULL);
 }
 if(level_number>=UB_FIRST_LEVEL){
  if(level_number<UB_LAST_LEVEL)return load_game_level(level_number+1,1,NULL);
  if(!front_test && !transition_test)play_ending(0);
  frontend_title();return 1;
 }
 if(level_number==13 && !front_test && !transition_test)play_cutscene_sequence(0,3);if(level_number==15 && !front_test && !transition_test)play_ending(0);if(level_number<1 || level_number+1>=UB_FIRST_LEVEL){frontend_title();return 1;}return load_game_level(level_number+1,1,NULL);
}
/* Menu effects have their own output so pausing world audio cannot mute them.
   DOS passes a null position and environment 2 for these SoundEffect calls. */
static void inventory_sound(void) {
    if(!inventory_ring.sound)return;
    const double origin[3]={0,0,0};
    TombSoundEvent event={5,inventory_ring.sound,2,0,0,0,NULL};
    tomb_sound_output_event(ring_sound_output,&event,0,origin);
}
static void inventory_begin(void) {
    crystal_pending=-1;
    vr.menu_open=play.shooter.mode==TOMB_MODE_VR && !title_mode && !death_menu;
    mouse_inventory_begin();
    backdrop_dirty=1;backdrop_shade=32;refresh_slots();
    options_ring=slot_menu=menu_action=binding_wait=new_game_menu=passport_shortcut=passport_save=0;menu_error[0]=0;
    inventory_keys=inventory_key_only=play.quest_request;inventory_ring_current[0]=inventory_ring_current[1]=inventory_ring_current[2]=0;play.quest_request=0;play.inventory.chosen=-1;
    if(death_menu){options_ring=1;inventory_keys=inventory_key_only=0;tomb_ring_init_options(&inventory_ring,2);}
    else if(!(inventory_keys?tomb_ring_init_keys(&inventory_ring,&play.inventory):tomb_ring_init(&inventory_ring,&play.inventory)))return;
    ring_previous=inventory_ring;compass_previous=compass_angle;inventory_sound();inventory_open=1;inventory_stats=inventory_selected=0;inventory_input=0;inventory_accumulator=0;if(!vr_hand_ring())accumulator=0;
}
static int crystal_poll(void){
    int action=(play.shooter.mode==TOMB_MODE_FPS?play.shooter.input:play.last_input)&64;
    if(play.shooter.mode==TOMB_MODE_VR){
        action=0;
        if(!vr.hands[0].weapon && !vr.hands[1].weapon)
            for(int h=0;h<2;h++)if(vr.input.hand[h].valid && vr.input.grip[h]>.6 &&
                vr.hands[h].pickup<0 && !vr.hands[h].anchored && !vr.hands[h].support)action=1;
    }
    if(!action)crystal_held=0;
    if(!action || crystal_held || inventory_open || title_mode || cinema_mode)return 0;
    int item=tomb_save_crystal_find(&play,&objects);if(item<0)return 0;
    crystal_held=1;
    inventory_begin();crystal_pending=item;inventory_keys=inventory_key_only=0;options_ring=1;
    tomb_ring_init_options(&inventory_ring,2);ring_previous=inventory_ring;
    passport_shortcut=2;passport_save=1;accumulator=0;return 1;
}
static void font_glyph(void *user,const TombTextGlyph *g) {
    (void)user;int sprite=tomb_hazard_sprite(visual,190,g->glyph,NULL);if(sprite<0)return;
    const unsigned char *p=visual->sprite_textures+16*sprite;int tile=u16(p);
    float x0=(float)(g->x+((int64_t)s16(p+8)*g->scale_x>>16)),y0=(float)(g->y+((int64_t)s16(p+10)*g->scale_y>>16));
    float x1=(float)(g->x+((int64_t)s16(p+12)*g->scale_x>>16)),y1=(float)(g->y+((int64_t)s16(p+14)*g->scale_y>>16));
    float u=p[2]/256.f,v=p[3]/256.f,du=(u16(p+4)+1)/65536.f,dv=(u16(p+6)+1)/65536.f;
    glBindTexture(GL_TEXTURE_2D,font_tiles[tile]);glBegin(GL_QUADS);
    glTexCoord2f(u,v);glVertex2f(x0,y0);glTexCoord2f(u+du,v);glVertex2f(x1,y0);
    glTexCoord2f(u+du,v+dv);glVertex2f(x1,y1);glTexCoord2f(u,v+dv);glVertex2f(x0,y1);glEnd();
}
static void text_aligned(float x,float y,const char *text,unsigned flags) {
    double scale=ui_scale();
    TombTextStyle style={(int)x,(int)y,(int32_t)(scale*65536),(int32_t)(scale*65536),1,6,(uint16_t)flags};
    glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glColor3f(1,1,1);
    tomb_text_layout(text,&style,width,height,font_glyph,NULL);
    glDisable(GL_ALPHA_TEST);glDisable(GL_TEXTURE_2D);
}
#include "ui_dialog.h"
#include "preview_frontend.h"
static void inventory_tick(unsigned input) {
    ring_previous=inventory_ring;compass_previous=compass_angle;
    if(inventory_ring.motion.status==2 && inventory_ring.motion.target==13){if(backdrop_shade>32)--backdrop_shade;}else if(backdrop_shade<48)++backdrop_shade;
    if(frontend_input(&input))return;
    if(!title_mode && !death_menu && !options_ring && !inventory_key_only && ((input&16 && !inventory_keys) || (input&32 && inventory_keys))){
        int owned=play.inventory.scion+play.inventory.counts[8];for(int q=0;q<8;q++)owned+=play.inventory.quest[q];
        if((inventory_keys || owned) && tomb_ring_change_begin(&inventory_ring,!inventory_keys))inventory_ring_current[inventory_keys]=inventory_ring.current;
    }
    if(!title_mode && !death_menu && !inventory_key_only && !inventory_keys && ((input&32 && !options_ring) || (input&16 && options_ring))){
        if(tomb_ring_change_begin(&inventory_ring,options_ring)){inventory_ring.motion.target=options_ring?6:3;inventory_ring_current[options_ring?2:0]=inventory_ring.current;}
    }
    if((title_mode || death_menu) && inventory_ring.motion.status==1)input&=~TOMB_RING_BACK;
    tomb_ring_tick(&inventory_ring,input);inventory_sound();
    if(inventory_ring.motion.status==3 || inventory_ring.motion.status==6){
        options_ring=inventory_ring.motion.status==3;if(options_ring)tomb_ring_init_options(&inventory_ring,2);else tomb_ring_init(&inventory_ring,&play.inventory);
        tomb_ring_change_end(&inventory_ring,!options_ring,inventory_ring_current[options_ring?2:0]);
    }inventory_selected=inventory_ring.current;
    if(inventory_ring.motion.status==4 || inventory_ring.motion.status==5){
        inventory_keys=inventory_ring.motion.status==4;
        if(inventory_keys)tomb_ring_init_keys(&inventory_ring,&play.inventory);else tomb_ring_init(&inventory_ring,&play.inventory);
        tomb_ring_change_end(&inventory_ring,inventory_keys,inventory_ring_current[inventory_keys]);inventory_selected=inventory_ring.current;
    }
    tomb_ring_compass(inventory_ring.items[0].y,play.lara.actor.yaw,&compass_angle,&compass_velocity);
    inventory_stats=inventory_ring.motion.status==8 && inventory_ring.ready && inventory_ring.items[inventory_selected].object==72;
    if(inventory_ring.motion.status!=13)return;
    if(play.shooter.mode==TOMB_MODE_VR){vr.menu_primary=1;vr.menu_open=0;xr.panel_open=0;inventory_input=0;}
    int id=inventory_ring.chosen;inventory_open=inventory_stats=0;accumulator=0;
    if(id==71 || id==73){frontend_execute(id);return;}
    if(title_mode || death_menu){inventory_begin();return;}
    if(play.shooter.mode==TOMB_MODE_VR && tomb_quest_slot(id)>=0){vr.selected_key=id;play.inventory.chosen=-1;return;}
    if(id>=99 && id<=102){play.requested_weapon=id==100?4:id==101?2:id==102?3:1;if(play.pistols.status==0 && !play.shooter.mode)draw_requested=1;}
    else if(tomb_inventory_use(&play.inventory,id,&play.lara.health)==1) {
        play.water.health=play.lara.health;play.movement.health=play.lara.health;
        play.sound_count=1;play.sounds[0]=(TombSoundEvent){5,116,2,play.lara.actor.x,play.lara.actor.y,play.lara.actor.z,NULL};sound_tick();play.sound_count=0;
    }
}
static void ring_item(const TombRingItem *item,const TombRingItem *previous) {
    TombModel model;TombPose pose;if(!tomb_visual_model(visual,item->object,&model) || !tomb_visual_key(visual,model.animation,(size_t)item->frame,&pose))return;
    glTranslatef(0,(float)item->yoff,(float)item->zoff);glRotatef(item->y*360.f/65536,0,1,0);glRotatef(item->x*360.f/65536,1,0,0);
    TombPose before=pose;if(ring_alpha<1)tomb_visual_key(visual,model.animation,(size_t)previous->frame,&before);
    glTranslatef((float)(before.root[0]+(pose.root[0]-before.root[0])*ring_alpha),(float)(before.root[1]+(pose.root[1]-before.root[1])*ring_alpha),(float)(before.root[2]+(pose.root[2]-before.root[2])*ring_alpha));rotate_pose(&before,&pose,0,(float)ring_alpha);
    if(item->meshes&1)mesh(visual->meshes+model.mesh_start);
    GLfloat stack[64][16];int depth=0;
    for(unsigned j=1;j<model.mesh_count;j++) {
        const unsigned char *bone=visual->trees+4*model.tree_word+(j-1)*16;int flags=s32(bone);
        if(flags&1){if(!depth)return;glLoadMatrixf(stack[--depth]);}
        if(flags&2){if(depth>=64)return;glGetFloatv(GL_MODELVIEW_MATRIX,stack[depth++]);}
        glTranslatef((float)s32(bone+4),(float)s32(bone+8),(float)s32(bone+12));rotate_pose(&before,&pose,(int)j,(float)ring_alpha);
        if(tomb_ring_compass_bone(item->object,model.mesh_count,j))glRotatef((float)(compass_previous+tomb_word(compass_angle-compass_previous)*ring_alpha)*360.f/65536,0,1,0);
        if(item->meshes&(1u<<j))mesh(visual->meshes+model.mesh_start+j);
    }
}
static void draw_inventory_ring(void) {
    TombVisual *saved_visual=visual;GLuint *saved_tiles=tiles;
    if(options_ring && title_visual){visual=title_visual;tiles=title_tiles;}

    TombRing sample=inventory_ring,*r=&sample;
    ring_alpha=ring_interpolate?fmax(0,fmin(1,inventory_accumulator*30)):1;
    if(ring_previous.count!=r->count)ring_alpha=1;
    for(int j=0;j<r->count;j++)if(ring_previous.items[j].object!=r->items[j].object)ring_alpha=1;
#define RING_LINEAR(field) r->field=ring_previous.field+(int)((r->field-ring_previous.field)*ring_alpha)
#define RING_ANGLE(field) r->field=tomb_word(ring_previous.field+(int)(tomb_word(r->field-ring_previous.field)*ring_alpha))
    RING_LINEAR(radius);RING_LINEAR(camera_y);RING_ANGLE(pitch);RING_ANGLE(angle);
    for(int j=0;j<r->count;j++){
        RING_ANGLE(items[j].pivot_now);RING_ANGLE(items[j].x);RING_ANGLE(items[j].y);
        RING_LINEAR(items[j].yoff);RING_LINEAR(items[j].zoff);
    }
#undef RING_LINEAR
#undef RING_ANGLE
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();double focal=tomb_preview_aspect_focal(height);
    glFrustum(-10.0*width/(2*focal),10.0*width/(2*focal),-10.0*height/(2*focal),10.0*height/(2*focal),10,20480);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();glRotatef(-r->pitch*360.f/65536,1,0,0);
    double eye[3]={0,r->camera_y,r->radius+598},target[3]={0,-96,r->radius};GLdouble view[16];
    tomb_preview_view(eye,target,view);glMultMatrixd(view);glGetDoublev(GL_MODELVIEW_MATRIX,lighting_view);
    const int32_t ring_light[3]={-1536,256,1024};model_light.divider=0x6000;tomb_light_direction(ring_light,sine_table,model_light.direction);light_in_view();
    glRotatef(r->angle*360.f/65536,0,1,0);
    glEnable(GL_DEPTH_TEST);glClear(GL_DEPTH_BUFFER_BIT);
    for(int j=0;j<r->count;j++) {
        model_light.adder=j==r->current?0x1000:0x1400;
        glPushMatrix();glRotatef(j*r->step*360.f/65536,0,1,0);glTranslatef(r->radius,0,0);
        glRotatef(90,0,1,0);glRotatef(r->items[j].pivot_now*360.f/65536,1,0,0);ring_item(r->items+j,ring_previous.items+j);glPopMatrix();
    }
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glDisable(GL_DEPTH_TEST);glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);
    visual=saved_visual;tiles=saved_tiles;
}
static double ui_scale(void){
    double base=fmax(.5,fmin(width/640.0,height/480.0));
    double scale=base*(settings_detail==0?2.4:settings_detail==3?2.0:1.0);
    /* Large HUD and labels, but keep complete dialogs inside the display. */
    if(inventory_open || (objects.progress && objects.progress->complete)){
        int object=inventory_ring.count?inventory_ring.items[inventory_ring.current].object:0;
        double panel_width=object==97?640:520;
        scale=fmin(scale,fmin(width/panel_width,height/420.0));
    }
    return scale;
}
static void ui_text(int x,int y,const char *text,unsigned flags){double scale=ui_scale();text_aligned((float)(x*scale),(float)(y*scale),text,flags);}
typedef struct HudTextBounds { int top,right,valid; } HudTextBounds;
static void hud_text_bounds(void *user,const TombTextGlyph *g){
    HudTextBounds *b=user;int sprite=tomb_hazard_sprite(visual,190,g->glyph,NULL);if(sprite<0)return;
    const unsigned char *p=visual->sprite_textures+16*sprite;
    int top=g->y+(int)((int64_t)s16(p+10)*g->scale_y>>16);
    int right=g->x+(int)((int64_t)s16(p+12)*g->scale_x>>16);
    if(!b->valid || top<b->top)b->top=top;
    if(!b->valid || right>b->right)b->right=right;
    b->valid=1;
}
static void hud_ammo_text(const char *text){
    double scale=ui_scale();
    TombTextStyle style={0,0,(int32_t)(scale*65536),(int32_t)(scale*65536),1,6,0};
    HudTextBounds bounds={0};tomb_text_layout(text,&style,width,height,hud_text_bounds,&bounds);
    if(!bounds.valid)return;
    /* Match the health bar's six-unit outer margin, including sprite offsets.
       Text advance widths and baselines do not bound the ammo icon artwork. */
    int margin=(int)(6*scale);style.x=width-margin-bounds.right;style.y=margin-bounds.top;
    glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glColor3f(1,1,1);
    tomb_text_layout(text,&style,width,height,font_glyph,NULL);
    glDisable(GL_ALPHA_TEST);glDisable(GL_TEXTURE_2D);
}
static void bar_line(void *user,int x1,int y1,int x2,int y2,int palette) {
    (void)user;double scale=ui_scale();colour((unsigned)palette,1);
    glBegin(GL_QUADS);glVertex2d(x1*scale,y1*scale);glVertex2d((x2+1)*scale,y1*scale);
    glVertex2d((x2+1)*scale,(y2+1)*scale);glVertex2d(x1*scale,(y2+1)*scale);glEnd();
}
static void pickup_icon(int object,int slot) {
    int sprite=tomb_hazard_sprite(visual,object,0,NULL);if(sprite<0)return;
    const unsigned char *p=visual->sprite_textures+16*sprite;double scale=ui_scale();
    int logical_width=(int)(width/scale),logical_height=(int)(height/scale),spacing=logical_width/10;
    double x=(logical_width-spacing-slot*(spacing*4/3))*scale,y=(logical_height-spacing)*scale;
    double factor=scale*0x3000/65536.0;
    double x0=x+s16(p+8)*factor,y0=y+s16(p+10)*factor,x1=x+s16(p+12)*factor,y1=y+s16(p+14)*factor;
    float u=p[2]/256.f,v=p[3]/256.f,du=(u16(p+4)+1)/65536.f,dv=(u16(p+6)+1)/65536.f;
    glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glColor3f(1,1,1);glBindTexture(GL_TEXTURE_2D,font_tiles[u16(p)]);
    glBegin(GL_QUADS);glTexCoord2f(u,v);glVertex2d(x0,y0);glTexCoord2f(u+du,v);glVertex2d(x1,y0);
    glTexCoord2f(u+du,v+dv);glVertex2d(x1,y1);glTexCoord2f(u,v+dv);glVertex2d(x0,y1);glEnd();
    glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);
}
static void overlay(void) {
    if(vr_hand_ring() && !xr.panel_drawing)return;
    int complete=caves && objects.progress->complete;
    TombVisual *game_visual=visual;GLuint *game_tiles=tiles,*game_font=font_tiles;
    if(title_mode && title_visual){visual=title_visual;tiles=title_tiles;font_tiles=title_font_tiles;complete=0;}
    glDisable(GL_SCISSOR_TEST);glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,width,height,0,-4096,4096);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    int medipack=0;
    if(inventory_open || complete) {
        if(overlay_pass!=2 && !title_mode && !vr_hand_ring())frontend_inventory_backdrop();
        if(overlay_pass!=2 && title_mode)frontend_backdrop();
        if(complete) {
            char lines[4][80];tomb_statistics(play.inventory.ticks,play.pistols.kills,play.inventory.pickups,objects.progress->secrets,tomb_level_secret_total(level_number),lines);
            if(overlay_pass!=1){ui_text(0,-50,level_choices[level_number].name,TOMB_TEXT_CENTRE|TOMB_TEXT_MIDDLE);
            for(int i=0;i<4;i++)ui_text(0,-20+30*i,lines[i],TOMB_TEXT_CENTRE|TOMB_TEXT_MIDDLE);}
        } else {
            if(overlay_pass!=2 && !new_game_menu)draw_inventory_ring();
            if(overlay_pass!=1 && inventory_ring.motion.status!=0 && inventory_ring.motion.status!=13)ui_text(0,26,title_mode?"":options_ring?"OPTIONS":inventory_keys?"ITEMS":"INVENTORY",TOMB_TEXT_CENTRE);
            if(overlay_pass!=1 && !inventory_ring.rotating && inventory_ring.motion.status!=0 && inventory_ring.motion.status!=13) {
                int id=inventory_ring.items[inventory_selected].object;char amount[64];
                if(id!=71)ui_text(0,-16,frontend_name(id),TOMB_TEXT_CENTRE|TOMB_TEXT_BOTTOM);
                if(tomb_inventory_label(&play.inventory,id,amount))ui_text(64,-56,amount,TOMB_TEXT_CENTRE|TOMB_TEXT_BOTTOM);
                medipack=id==108 || id==109;
            }
        }
    }
    if(overlay_pass!=1 && inventory_open)frontend_overlay();
    if(overlay_pass!=1 && !title_mode && !complete && (!inventory_open || medipack)) {
        glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);
        tomb_hud_bar(tomb_hud_health(&play.hud,play.lara.health,play.pistols.status,medipack),0,(int)(width/ui_scale()),bar_line,NULL);
        if(!inventory_open) {
            tomb_hud_bar(tomb_hud_air(play.lara.air,play.lara.water_status),1,(int)(width/ui_scale()),bar_line,NULL);
            /* Shotgun ammunition is stored per pellet; the HUD shows shells. */
            if((play.weapon_type>=2 && play.weapon_type<=4) && play.pistols.status==4){char amount[64];int slot=play.weapon_type==4?1:play.weapon_type;tomb_ui_ammo(play.inventory.ammo[slot],slot,amount);hud_ammo_text(amount);}
            for(int i=0;i<3;i++)if(play.hud.pickup_time[i]>0)pickup_icon(play.hud.pickup_id[i],i);
        }
    }
    if(overlay_pass!=1 && !title_mode && !cinema_mode && !complete && !inventory_open &&
       level_name_time>0 && detail_notice_time<=0 && (play.gameplay_options&TOMB_SHOW_LEVEL_NAME))
        ui_text(0,-24,level_choices[level_number].name,TOMB_TEXT_CENTRE|TOMB_TEXT_BOTTOM);
    if(overlay_pass!=1 && !title_mode && !cinema_mode && detail_notice_time>0){
        double scale=ui_scale();
        TombTextStyle style={0,0,65536,65536,1,6,TOMB_TEXT_CENTRE|TOMB_TEXT_BOTTOM};
        int text_width=tomb_text_width(detail_notice,&style);
        if(text_width>0)scale=fmin(scale,width*.9/text_width);
        style.scale_x=style.scale_y=(int32_t)(scale*65536);style.y=-(int)(24*ui_scale());
        glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glColor3f(1,1,1);
        tomb_text_layout(detail_notice,&style,width,height,font_glyph,NULL);
        glDisable(GL_ALPHA_TEST);glDisable(GL_TEXTURE_2D);
    }
    if(overlay_pass!=1 && play.shooter.mode && play.pistols.drawn && play.lara.health>0 && !inventory_open && !title_mode && !complete){
        float x=width*.5f,y=height*.5f;glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);
        glColor3f(0,0,0);glLineWidth(3);glBegin(GL_LINES);glVertex2f(x-7,y);glVertex2f(x+7,y);glVertex2f(x,y-7);glVertex2f(x,y+7);glEnd();
        glColor3f(1,1,1);glLineWidth(1);glBegin(GL_LINES);glVertex2f(x-6,y);glVertex2f(x+6,y);glVertex2f(x,y-6);glVertex2f(x,y+6);glEnd();
    }
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glEnable(GL_DEPTH_TEST);
    visual=game_visual;tiles=game_tiles;font_tiles=game_font;
}
static int draw_frame_composite(int);
#include "preview_vr.h"
static int draw(void) {
    render_track=SIZE_MAX;
    /* Every world frame owns its depth state, including after 2D presentation. */
    glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDisable(GL_SCISSOR_TEST);glDisable(GL_POLYGON_OFFSET_FILL);
    glViewport(0,0,width,height);glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    double focal=tomb_preview_aspect_focal(height),shot_roll=0;
    if(objects.peru && objects.peru->scion_item>=0){
        TombDosCamera shot;int fov,roll;
        if(tomb_scion_camera(&objects,sine_table,&shot,&fov,&roll) && fov>0 && fov<32768){
            int half=fov/2,si=tomb_hazard_sine(sine_table,half),co=tomb_hazard_sine(sine_table,half+16384);
            if(si)focal=height*((320*co/si)/480.0);shot_roll=roll*(6.283185307179586/65536.0);
        }
    }
    if(cinema_mode && cinema_fov>0 && cinema_fov<32768){int half=cinema_fov/2,si=tomb_hazard_sine(sine_table,half),co=tomb_hazard_sine(sine_table,half+16384);if(si)focal=height*((320*co/si)/480.0);shot_roll=cinema_roll*(6.283185307179586/65536.0);}
    effect_focal=focal;
    double half_x=5.0*width/(focal*render_pixel_scale),half_y=5.0*height/focal;
    double vr_fx=0,vr_fy=0,vr_cx=0,vr_cy=0;
    world_far_distance=world_far_plane();
    if(xr.render_eye>=0){XrFovf f=xr.views[xr.render_eye].fov;double l=tan(f.angleLeft),r=tan(f.angleRight),b=tan(f.angleDown),t=tan(f.angleUp);glFrustum(l*4,r*4,b*4,t*4,4,world_far_distance);vr_fx=width/(r-l);vr_fy=height/(t-b);vr_cx=-l*vr_fx;vr_cy=-b*vr_fy;focal=effect_focal=vr_fy;}
    else glFrustum(-half_x,half_x,-half_y,half_y,10,world_far_distance);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    const TombActor *actor=pose_mode?&start.actor:&play.lara.actor;
    double target[3]={actor->x,actor->y-450,actor->z};
    double eye[3]={target[0]+cos(orbit)*distance,target[1]-550,target[2]+sin(orbit)*distance};
    if(!pose_mode && !manual_camera && camera.ready) { for(int i=0;i<3;i++){double t=camera_alpha;target[i]=camera.previous_target[i]+t*(camera.target[i]-camera.previous_target[i]);eye[i]=camera.previous_eye[i]+t*(camera.eye[i]-camera.previous_eye[i]);} }
    if(play.shooter.mode && !pose_mode && !cinema_mode && play.lara.health>0 && !(objects.peru && objects.peru->scion_item>=0) && play.lara.actor.current!=50 && play.lara.actor.current!=51){
        double pos[3];tomb_shooter_anchor(&play,pos);
        if(render_count && (play.shooter.mode==TOMB_MODE_FPS || render_tracks[0].tick==render_tick))for(int j=0;j<3;j++)pos[j]=shooter_previous[j]+camera_alpha*(shooter_current[j]-shooter_previous[j]);
        int room;tomb_shooter_view_blend(&play,pos,block_view_sample(),eye,target,&room);camera.room=room;
    }
    GLdouble view[16];
    if(play.shooter.mode==TOMB_MODE_VR && vr.ready && !cinema_mode){TombVrPose pose=xr.render_eye>=0?xr_pose(xr.views[xr.render_eye].pose,1):vr.input.head;vr_view(&pose,eye,view);}
    else{
        if(!tomb_preview_view(eye,target,view)){fprintf(stderr,"invalid view eye=%f,%f,%f target=%f,%f,%f\n",eye[0],eye[1],eye[2],target[0],target[1],target[2]);return 0;}
        if(shot_roll){double cs=cos(shot_roll),sn=sin(shot_roll);for(int j=0;j<4;j++){double x=view[j*4],y=view[j*4+1];view[j*4]=cs*x-sn*y;view[j*4+1]=sn*x+cs*y;}}
    }
    glMultMatrixd(view);memcpy(lighting_view,view,sizeof view);
    int eye_room=(!manual_camera && camera.ready)?camera.room:actor->room;
    if(play.shooter.mode==TOMB_MODE_VR)eye_room=actor->room;
    TombSectorRef eye_ref;
    if(tomb_find_sector(level,(int32_t)eye[0],(int32_t)eye[1],(int32_t)eye[2],eye_room,&eye_ref))eye_room=eye_ref.room;
    water_eye=eye_room>=0 && (size_t)eye_room<level->room_count && (level->rooms[eye_room].flags&1);
    if(xr.render_eye>=0){if(!tomb_room_visibility_projection(visual,eye_room,eye,view,width,height,vr_fx,vr_fy,vr_cx,vr_cy,room_rects))return 0;}
    else if(!cinema_mode && !tomb_room_visibility_projection(visual,eye_room,eye,view,width,height,focal*render_pixel_scale,focal,width*.5,height*.5,room_rects))return 0;
    if(cinema_mode){for(size_t i=0;i<level->room_count;i++)room_rects[i]=(TombScreenRect){0,0,width,height};for(size_t i=0;i<level->room_count;i++)if(level->rooms[i].alternate>=0)room_rects[level->rooms[i].alternate]=(TombScreenRect){0};}
    for(size_t i=0;i<level->room_count;i++) {
        TombScreenRect clip=room_rects[i];
        if(clip.left>=clip.right || clip.bottom>=clip.top)continue;
        glEnable(GL_SCISSOR_TEST);
        glScissor(clip.left,clip.bottom,clip.right-clip.left,clip.top-clip.bottom);
        const TombRoom *room=level->rooms+i;
        glPushMatrix(); glTranslatef((float)room->x,0,(float)room->z);
        water_draw_room=i;water_object=water_room=(room->flags&1)!=0;mesh(visual->rooms+i);water_draw_room=SIZE_MAX; glPopMatrix();
        const TombMeshView *room_mesh=visual->rooms+i;
        for(size_t j=0;j<room_mesh->sprite_count;j++){
            const unsigned char *entry=room_mesh->sprites+j*4;
            const unsigned char *v=room_mesh->vertices+u16(entry)*room_mesh->vertex_stride;
            float position[16]={0};position[12]=(float)(room->x+s16(v));position[13]=(float)s16(v+2);position[14]=(float)(room->z+s16(v+4));
            draw_billboard(u16(entry+2),position,s16(v+6),-1);
        }
        water_room=0;
        glDisable(GL_SCISSOR_TEST); /* Static objects use the full viewport in DOS. */
        for(size_t j=0;j<room->static_count;j++) {
            const TombStaticPlacement *p=room->statics+j;
            for(size_t k=0;k<level->static_def_count;k++) {
                const TombStaticDef *def=level->static_defs+k;
                if(def->id!=p->id) continue;
                if(!(def->flags&2))break;
                glPushMatrix(); glTranslatef((float)p->x,(float)p->y,(float)p->z);
                glRotatef(p->rotation*(360.f/65536.f),0,1,0);
                GLdouble object_view[16];glGetDoublev(GL_MODELVIEW_MATRIX,object_view);
                tomb_light_static(tomb_word(p->intensity),lighting_depth(),&model_light);
                if(tomb_static_visible_range(def,object_view,width,height,xr.render_eye>=0?vr_fx:tomb_preview_aspect_focal(height)*render_pixel_scale,xr.render_eye>=0?vr_fy:tomb_preview_aspect_focal(height),xr.render_eye>=0?vr_cx:width*.5,xr.render_eye>=0?vr_cy:height*.5,clip,world_far_distance))mesh(visual->meshes+def->mesh);
                glPopMatrix(); break;
            }
        }
    }
    glDisable(GL_SCISSOR_TEST);
    if(!cinema_mode)draw_shadows();
    if(caves)for(size_t i=0;i<objects.count;i++) {
        const TombObject *o=objects.items+i;
        if((o->object==83 && !(play.gameplay_options&TOMB_SAVE_CRYSTALS)) || o->object<0 || (!cinema_mode && !tomb_object_supported(o->object)) || (o->actor.flags&6)==6 || (tomb_pickup_object(o->object) && (o->actor.flags&4)))continue;
        if(render_object_visible(i)) {
            if(tomb_pickup_object(o->object)){if((play.gameplay_options&TOMB_3D_PICKUPS) && draw_pickup_model(o,i<level->item_count?level->items[i].yaw:0))continue;TombHazardEffect e={0};e.id=o->object;e.x=o->actor.x;e.y=o->actor.y;e.z=o->actor.z;e.room=o->actor.room;draw_hazard_effect(&e,SIZE_MAX,i<level->item_count?level->items[i].intensity:4096);}
            else if(!draw_object(o)){fprintf(stderr,"object draw failed: %d animation %d frame %d\n",o->object,o->actor.animation,o->actor.frame);return 0;}
        }
    }
    if(caves && objects.hazards)for(int i=0;i<TOMB_DART_CAPACITY;i++) {
        const TombObject *o=&objects.hazards->darts[i].item;if(!o->active)continue;
        if(render_visible(1+objects.count+(size_t)i,o->actor.room)) {
            if(tomb_pickup_object(o->object)){if((play.gameplay_options&TOMB_3D_PICKUPS) && draw_pickup_model(o,0))continue;TombHazardEffect e={0};e.id=o->object;e.x=o->actor.x;e.y=o->actor.y;e.z=o->actor.z;e.room=o->actor.room;draw_hazard_effect(&e,SIZE_MAX,4096);}
            else if(!draw_object(o)){fprintf(stderr,"object draw failed: %d animation %d frame %d\n",o->object,o->actor.animation,o->actor.frame);return 0;}
        }
    }
    if(caves && objects.hazards)for(int i=0;i<256;i++){const TombHazardEffect *e=objects.hazards->effects+i;if(e->active){if(render_visible(1+objects.count+TOMB_DART_CAPACITY+(size_t)i,e->room))draw_hazard_effect(e,1+objects.count+TOMB_DART_CAPACITY+(size_t)i,4096);}}
    if(!cinema_mode)water_particles_draw();
    /* FPS hands clear world depth. Keep them after the PS1 water pass so
       opaque walls still occlude water; world-space Lara stays before it. */
    int defer_fps=(visual->rgba_tiles || water_ps1_colour()) && fps_overlay_active();
    int lara_ok=cinema_mode || defer_fps?1:lara();if(!cinema_mode)vr_hands_draw();
    if(visual->rgba_tiles || water_ps1_colour()){
        room_translucent_pass=1;glPushMatrix();glLoadMatrixd(view);
        for(size_t i=0;i<level->room_count;i++){
            TombScreenRect clip=room_rects[i];if(clip.left>=clip.right || clip.bottom>=clip.top)continue;
            glEnable(GL_SCISSOR_TEST);glScissor(clip.left,clip.bottom,clip.right-clip.left,clip.top-clip.bottom);
            const TombRoom *room=level->rooms+i;
            glPushMatrix();glTranslatef((float)room->x,0,(float)room->z);
            water_draw_room=i;water_object=water_room=(room->flags&1)!=0;mesh(visual->rooms+i);water_draw_room=SIZE_MAX;glPopMatrix();
        }
        glPopMatrix();glDisable(GL_SCISSOR_TEST);room_translucent_pass=0;
    }
    water_room=0;
    if(defer_fps)lara_ok=lara();
    water_eye=water_room=water_object=0; GLenum gl_error=glGetError();
    if(!lara_ok || gl_error)fprintf(stderr,"draw error: Lara=%d GL=%u state=%d animation=%d frame=%d\n",lara_ok,(unsigned)gl_error,play.lara.actor.current,play.lara.actor.animation,play.lara.actor.frame);
    return lara_ok && gl_error==GL_NO_ERROR;
}

#ifdef TOMB_BUILD_TESTS
#include "../tests/interpolation_render.h"
#endif
static int capture(const char *);
#ifdef TOMB_BUILD_TESTS
#include "../tests/vr_render.h"
#endif
#ifdef TOMB_BUILD_TESTS
static int draw_frame(void);
#include "../tests/block_render.h"
#include "../tests/door_render.h"
#include "../tests/remaining_render.h"
#endif
/* Legacy saved values: DOS Low=0, DOS High=1, Full=2; PS1=3.
   Extend horizontal resolution for widescreen and correct non-square pixels. */
static int draw_frame_composite(int include_ui){
 if(settings_detail==2){
  int ok=1;if(include_ui==2 || (include_ui && title_mode)){glViewport(0,0,width,height);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}else ok=draw();
  if(ok && include_ui)overlay();return ok;
 }
 static GLuint texture;int full_w=width,full_h=height;
 int base_w=settings_detail==3?384:settings_detail==1?640:320;
 int target=settings_detail==3?240:settings_detail==1?480:200;
 width=(int)(((int64_t)full_w*base_w*3+full_h*2)/((int64_t)full_h*4));if(width<1)width=1;
 height=target;
 /* The back-buffer render path cannot exceed the drawable dimensions. */
 if(width>full_w || height>full_h){double fit=fmin((double)full_w/width,(double)full_h/height);width=(int)(width*fit);height=(int)(height*fit);if(width<1)width=1;if(height<1)height=1;}
 render_pixel_scale=(double)width*full_h/(height*(double)full_w);
 int ok=1;
 if(include_ui==2 || (include_ui && title_mode)){glViewport(0,0,width,height);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}else ok=draw();
 int w=width,h=height,pw=1,ph=1;while(pw<w)pw*=2;while(ph<h)ph*=2;
 render_pixel_scale=1.0;
 /* Only backgrounds and inventory meshes share the world's raster.
    Text and HUD are drawn sharply at display resolution after enlargement. */
 if(ok && include_ui){width=full_w;height=full_h;overlay_pass=1;overlay();overlay_pass=0;width=w;height=h;}
 if(!texture)glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,pw,ph,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,w,h);
 width=full_w;height=full_h;glViewport(0,0,width,height);glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_ALPHA_TEST);glEnable(GL_TEXTURE_2D);glColor3f(1,1,1);
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,width,0,height,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2i(0,0);glTexCoord2f((float)w/pw,0);glVertex2i(width,0);glTexCoord2f((float)w/pw,(float)h/ph);glVertex2i(width,height);glTexCoord2f(0,(float)h/ph);glVertex2i(0,height);glEnd();glDisable(GL_TEXTURE_2D);glEnable(GL_DEPTH_TEST);
 if(ok && include_ui){overlay_pass=2;overlay();overlay_pass=0;}return ok;
}
#ifdef TOMB_BUILD_TESTS
static int draw_frame(void){return draw_frame_composite(0);}
#include "../tests/water_render.h"
#include "../tests/jaw_render.h"
#endif

static int capture(const char *path) {
    size_t bytes=(size_t)width*height*3;
    unsigned char *pixels=malloc(bytes); if(!pixels) return 0;
    glPixelStorei(GL_PACK_ALIGNMENT,1); glReadBuffer(GL_BACK);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels);
    FILE *f=fopen(path,"wb"); if(!f) { free(pixels); return 0; }
    /* PPM keeps deterministic captures independent of image libraries. */
    fprintf(f,"P6\n%d %d\n255\n",width,height);
    int ok=1;
    for(int y=height-1;y>=0;y--) if(fwrite(pixels+(size_t)y*width*3,1,(size_t)width*3,f)!=(size_t)width*3) ok=0;
    if(fclose(f)) ok=0;
    free(pixels); return ok;
}
#ifdef TOMB_BUILD_TESTS
#include "../tests/peru_render.h"
#include "../tests/folly_render.h"
#include "../tests/midas_render.h"
#endif
static unsigned menu_entries=0;
static unsigned char held_keys[256],stats_consumed_keys[256];
/* Desktop presentation limits never throttle the headset compositor. */
static void display_size(HWND window){
    RECT client;GetClientRect(window,&client);width=client.right>0?client.right:1;height=client.bottom>0?client.bottom:1;
    if(settings_force43 && !xr.instance){
        if((int64_t)width*3>(int64_t)height*4)width=height*4/3;
        else height=width*3/4;
        if(width<1)width=1;if(height<1)height=1;
    }
}
static void display_borders(HDC dc){
    if(!settings_force43 || xr.instance)return;
    RECT client;GetClientRect(WindowFromDC(dc),&client);
    if(client.right==width && client.bottom==height)return;
    glPushAttrib(GL_ENABLE_BIT|GL_DEPTH_BUFFER_BIT);
    static GLuint image;static int tw,th;
    if(!image)glGenTextures(1,&image);
    light_UseProgram(0);glBindTexture(GL_TEXTURE_2D,image);
    if(tw!=width || th!=height){tw=width;th=height;glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,tw,th,0,GL_RGB,GL_UNSIGNED_BYTE,NULL);}
    glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,width,height);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    glViewport((client.right-width)/2,(client.bottom-height)/2,width,height);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    glEnable(GL_TEXTURE_2D);glColor3f(1,1,1);glBegin(GL_QUADS);
    glTexCoord2f(0,0);glVertex2f(-1,-1);glTexCoord2f(1,0);glVertex2f(1,-1);
    glTexCoord2f(1,1);glVertex2f(1,1);glTexCoord2f(0,1);glVertex2f(-1,1);glEnd();glDisable(GL_TEXTURE_2D);
    glPopAttrib();
}
static void display_present(HDC dc){
    typedef BOOL (WINAPI *SwapInterval)(int);
    static SwapInterval interval;static int resolved,applied=-1;
    static LARGE_INTEGER previous;
    LARGE_INTEGER frequency,now;QueryPerformanceFrequency(&frequency);
    if(!resolved){PROC proc=wglGetProcAddress("wglSwapIntervalEXT");if(proc && (INT_PTR)proc!=-1 && (UINT_PTR)proc>3)memcpy(&interval,&proc,sizeof interval);resolved=1;}
    int sync=xr.instance?0:settings_vsync;
    if(interval && applied!=sync){interval(sync);applied=sync;}
    QueryPerformanceCounter(&now);
    if(!xr.instance && settings_framerate && previous.QuadPart){
        double period=1.0/(settings_framerate==1?60:30);
        /* Leave a small lead for the driver to queue the next synced swap. */
        double lead=sync && interval?.001:0;
        while((double)(now.QuadPart-previous.QuadPart)/frequency.QuadPart<period-lead){
            double remaining=period-lead-(double)(now.QuadPart-previous.QuadPart)/frequency.QuadPart;
            Sleep(remaining>.002?1:0);QueryPerformanceCounter(&now);
        }
    }
    display_borders(dc);SwapBuffers(dc);QueryPerformanceCounter(&previous);
}
#include "preview_media.h"
/* Use keypad scan codes so Num Lock does not change the movement bindings.
   Extended navigation keys remain distinct from their keypad equivalents. */
static unsigned input_key(WPARAM key,LPARAM flags) {
    if(!(flags&(1L<<24))) {
        switch((flags>>16)&255) {
        case 0x52:return VK_NUMPAD0;case 0x48:return VK_NUMPAD8;case 0x50:return VK_NUMPAD2;
        case 0x4b:return VK_NUMPAD4;case 0x4d:return VK_NUMPAD6;
        case 0x47:return VK_NUMPAD7;case 0x49:return VK_NUMPAD9;
        }
    }
    return (unsigned)key;
}
static unsigned game_input(void) {
 if(play.shooter.mode==TOMB_MODE_FPS){static const unsigned f[10]={1,2,4,8,128,16,64,TOMB_INPUT_SHOOT,TOMB_INPUT_AIM,0};unsigned in=0;for(int j=0;j<10;j++)if(held_keys[fps_bindings[j]])in|=f[j];return in;}
 if(play.shooter.mode){static const unsigned f[9]={1,2,4,8,128,16,64,TOMB_INPUT_AIM,0};unsigned in=0;for(int j=0;j<9;j++)if(held_keys[shooter_bindings[j]])in|=f[j];return in;}
 static const unsigned flags[13]={1,2,4,8,1024,2048,128,16,64,0,4096,512,0};unsigned input=held_keys[VK_RETURN]?64u:0;
 for(int i=0;i<13;i++)if(held_keys[bindings[i]])input|=flags[i];return input;
}
static int fullscreen;
static int mouse_captured,mouse_camera_active;
static void mouse_release(void){if(mouse_captured){ClipCursor(NULL);ReleaseCapture();ShowCursor(TRUE);mouse_captured=0;}}
static void mouse_inventory_begin(void){
    mouse_camera_active=0;
    /* Quest rings can open after this frame's mouse_update. Fullscreen keeps
       capture throughout menus, so do not expose the cursor between frames. */
    if(!fullscreen)mouse_release();
}
static void mouse_update(HWND window,int suspended){
    if(xr.instance){mouse_release();mouse_camera_active=0;return;}
    mouse_camera_active=play.shooter.mode && !suspended && play.lara.health>0 && !media_busy;
    if(GetActiveWindow()!=window || (!fullscreen && (!play.shooter.mode || suspended || play.lara.health<=0 || media_busy))){mouse_release();return;}
    if(!mouse_captured){SetCapture(window);ShowCursor(FALSE);mouse_captured=1;}
    RECT rect;GetClientRect(window,&rect);POINT corner={rect.left,rect.top},end={rect.right,rect.bottom};ClientToScreen(window,&corner);ClientToScreen(window,&end);
    rect=(RECT){corner.x,corner.y,end.x,end.y};ClipCursor(&rect);
}
static int fullscreen_enter_held;
static WINDOWPLACEMENT windowed_placement;
static LONG_PTR windowed_style;
static void toggle_fullscreen(HWND w) {
    if(!fullscreen) {
        MONITORINFO monitor={0};monitor.cbSize=sizeof monitor;
        windowed_placement.length=sizeof windowed_placement;
        if(!GetWindowPlacement(w,&windowed_placement) ||
           !GetMonitorInfoA(MonitorFromWindow(w,MONITOR_DEFAULTTONEAREST),&monitor))return;
        windowed_style=GetWindowLongPtrA(w,GWL_STYLE);
        fullscreen=1;
        SetWindowLongPtrA(w,GWL_STYLE,windowed_style & ~(LONG_PTR)WS_OVERLAPPEDWINDOW);
        SetWindowPos(w,NULL,monitor.rcMonitor.left,monitor.rcMonitor.top,
            monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top,
            SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_FRAMECHANGED);
    } else {
        fullscreen=0;
        SetWindowLongPtrA(w,GWL_STYLE,windowed_style);
        SetWindowPlacement(w,&windowed_placement);
        SetWindowPos(w,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_FRAMECHANGED);
    }
}
static LRESULT CALLBACK window_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    /* Handle system close before media/menu input can consume the chord.
       The main loop performs the same orderly cleanup as WM_CLOSE. */
    if(msg==WM_SYSKEYDOWN && wp==VK_F4 && (lp&(1L<<29))){running=0;return 0;}
    if(msg==WM_SETCURSOR && media_busy && LOWORD(lp)==HTCLIENT){SetCursor(NULL);return TRUE;}
    /* Consume the chord before Enter can select a menu item or fire. */
    if(msg==WM_SYSKEYDOWN && wp==VK_RETURN && (lp&(1L<<29))) {
        if(!(lp&(1L<<30)))toggle_fullscreen(w);
        fullscreen_enter_held=1;held_keys[VK_RETURN]=0;return 0;
    }
    if(fullscreen_enter_held && wp==VK_RETURN && (msg==WM_KEYUP || msg==WM_SYSKEYUP)) {
        fullscreen_enter_held=0;held_keys[VK_RETURN]=0;return 0;
    }

    if(msg==WM_CREATE){RAWINPUTDEVICE raw={1,2,0,w};RegisterRawInputDevices(&raw,1,sizeof raw);}
    if(msg==WM_INPUT){
        RAWINPUT raw;UINT bytes=sizeof raw;
        if(GetRawInputData((HRAWINPUT)lp,RID_INPUT,&raw,&bytes,sizeof(RAWINPUTHEADER))==sizeof raw && raw.header.dwType==RIM_TYPEMOUSE && mouse_captured && mouse_camera_active && !(raw.data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE))tomb_shooter_mouse(&play,raw.data.mouse.lLastX,raw.data.mouse.lLastY);
    }
    if(msg==WM_KILLFOCUS){memset(held_keys,0,sizeof held_keys);memset(stats_consumed_keys,0,sizeof stats_consumed_keys);mouse_release();}
    if(msg==WM_LBUTTONDOWN || msg==WM_LBUTTONUP || msg==WM_RBUTTONDOWN || msg==WM_RBUTTONUP || msg==WM_MBUTTONDOWN || msg==WM_MBUTTONUP){
        unsigned key=(msg==WM_LBUTTONDOWN || msg==WM_LBUTTONUP)?VK_LBUTTON:(msg==WM_RBUTTONDOWN || msg==WM_RBUTTONUP)?VK_RBUTTON:VK_MBUTTON;
        int down=msg==WM_LBUTTONDOWN || msg==WM_RBUTTONDOWN || msg==WM_MBUTTONDOWN;wp=key;lp=0;msg=down?WM_KEYDOWN:WM_KEYUP;
    }
    if(msg==WM_KEYDOWN || msg==WM_KEYUP || msg==WM_SYSKEYDOWN || msg==WM_SYSKEYUP) {
        wp=input_key(wp,lp);
        if(wp<256 && stats_consumed_keys[wp]){
            held_keys[wp]=0;
            if(msg==WM_KEYUP || msg==WM_SYSKEYUP)stats_consumed_keys[wp]=0;
            return 0;
        }
        if(wp<256)held_keys[wp]=(msg==WM_KEYDOWN || msg==WM_SYSKEYDOWN);
    }
    if(msg==WM_ENTERMENULOOP) { ++menu_entries; return 0; }
    /* Alt is a gameplay key. Do not enter Windows' modal keyboard menu. */
    if(msg==WM_SYSCOMMAND && (wp&0xfff0)==SC_KEYMENU) return 0;
    if((msg==WM_SYSKEYDOWN || msg==WM_SYSKEYUP) && wp==VK_MENU) return 0;
    /* Keep bare F10 out of the Windows modal menu; retain Alt+F4. */
    if(wp==VK_F10 && msg==WM_SYSKEYUP)return 0;
    if(wp==VK_F10 && msg==WM_SYSKEYDOWN)msg=WM_KEYDOWN;
    if(msg==WM_CLOSE) { running=0; return 0; }
    if(msg==WM_SIZE) { display_size(w); return 0; }
    if(media_busy && (msg==WM_KEYDOWN || msg==WM_KEYUP || msg==WM_SYSKEYDOWN || msg==WM_SYSKEYUP)){if(msg==WM_KEYDOWN && wp==VK_ESCAPE && !(lp&(1L<<30)))media_skip=1;return 0;}
    if(msg==WM_KEYDOWN) {
        if(binding_wait && !(lp&(1L<<30))){if(wp!=VK_ESCAPE && wp>0 && wp<256){control_bindings()[option_row]=(unsigned)wp;settings_write();}binding_wait=0;return 0;}
        int detail_row=wp==VK_F1?1:wp==VK_F2?2:wp==VK_F3?3:wp==VK_F4?4:wp==VK_F7?5:wp==VK_F8?6:wp==VK_F9?7:0;
        if(detail_row){if(wp<256)held_keys[wp]=0;if(!(lp&(1L<<30))){frontend_detail_cycle(detail_row,1);if(!title_mode){frontend_detail_notice(detail_row);detail_notice_time=3.0;}settings_write();}return 0;}
        if(wp==VK_F5 || wp==VK_F6){
            if(wp==VK_F5 && (play.gameplay_options&TOMB_SAVE_CRYSTALS))return 0;
            if(!(lp&(1L<<30)) && !title_mode && !cinema_mode && !paused && !inventory_open &&
               play.lara.health>0 && !objects.progress->complete){
                inventory_begin();inventory_keys=inventory_key_only=0;options_ring=1;
                tomb_ring_init_options(&inventory_ring,2);ring_previous=inventory_ring;
                passport_shortcut=wp==VK_F5?2:1;passport_save=wp==VK_F5;
            }
            return 0;
        }
        if(caves && objects.progress->complete && !title_mode) {
            if(wp==VK_RETURN || wp==(play.shooter.mode?active_mode_bindings()[6]:bindings[8])){if(!(lp&(1L<<30))){if(wp<256){stats_consumed_keys[wp]=1;held_keys[wp]=0;}load_next_level();}return 0;}
            if(wp!='R')return 0;
        }
        if(wp==(play.shooter.mode?active_mode_bindings()[play.shooter.mode==TOMB_MODE_FPS?9:8]:bindings[12]) || wp==VK_ESCAPE) {
            if(!(lp&(1L<<30)) && (play.lara.health>0 || play.dead_ticks>60 || title_mode)){if(inventory_open)inventory_input|=TOMB_RING_BACK;else {death_menu=play.lara.health<=0;inventory_begin();}}
            return 0;
        }
        if(inventory_open) {
            if(!(lp&(1L<<30))) {
                if(wp==VK_UP || wp==VK_NUMPAD8 || (play.shooter.mode && wp==active_mode_bindings()[0]))inventory_input|=16;
                if(wp==VK_DOWN || wp==VK_NUMPAD2 || (play.shooter.mode && wp==active_mode_bindings()[1]))inventory_input|=32;
                if(wp==VK_LEFT || wp==VK_NUMPAD4 || (play.shooter.mode && wp==active_mode_bindings()[2]))inventory_input|=TOMB_RING_LEFT;
                if(wp==VK_RIGHT || wp==VK_NUMPAD6 || (play.shooter.mode && wp==active_mode_bindings()[3]))inventory_input|=TOMB_RING_RIGHT;
                if(wp==VK_RETURN || wp==(play.shooter.mode?active_mode_bindings()[6]:bindings[8]))inventory_input|=TOMB_RING_SELECT;
            }
            return 0;
        }
        if(wp>='1' && wp<='4'){
            static const int weapons[4]={1,4,2,3};int slot=(int)(wp-'1');
            if(!(lp&(1L<<30)) && !paused && !title_mode && play.lara.health>0 && play.inventory.counts[slot]){
                play.requested_weapon=weapons[slot];
                if(play.pistols.status==0 && !play.shooter.mode)draw_requested=1;
            }
            return 0;
        }
        if(wp==VK_ESCAPE) running=0;
        if(wp=='P') paused=!paused;
        if(!play.shooter.mode && wp==bindings[9] && !(lp&(1L<<30)))draw_requested=1;
        if(wp=='R') { xr.reset=1;int mode=play.shooter.mode;uint32_t custom_options=play.gameplay_options;cutscene_seen=0;water_particles_reset();tomb_objects_reset(&objects); audio_stop();tomb_sound_output_reset(sound_output);paused=0;camera_hold=0;camera.ready=0;render_reset(); orbit=0; distance=1536; elapsed=0; accumulator=0; pose_mode=0; tomb_playtest_init(&play,level,sine_table,visual); play.movement_only=caves;play.objects=caves?&objects:NULL;if(level_number==0){play.inventory.counts[0]=0;play.inventory.ammo[0]=0;}tomb_gameplay_start(&play,custom_options);tomb_shooter_start(&play,mode);level_inventory=play.inventory; }
        if(wp=='R'){objects.progress->music.current_track=music_source()==0?0:tomb_music_level_track(level_number);draw_requested=0;inventory_open=death_menu=slot_menu=0;play.inventory=level_inventory;}
        return 0;
    }
    return DefWindowProcA(w,msg,wp,lp);
}
#ifdef TOMB_BUILD_TESTS
#include "../tests/frontend_test.h"
#include "../tests/details_menu.h"
#include "../tests/campaign_test.h"
#include "../tests/colour_options_render.h"
#include "../tests/autosave_switch_test.h"
#include "../tests/render_options_test.h"
#include "../tests/pickup_depth_render.h"
#include "../tests/pickup3d_render.h"
#include "../tests/original_review_test.h"
#include "../tests/consistent_weapons_test.h"
#endif
static int jaw_diagnostic_tick(TombPlaytest *p,uint32_t input){
 if(!jaw_log)return tomb_playtest_tick(p,input);
 TombActor before=p->lara.actor;int health=p->lara.health;
 int f2=objects.items[2].actor.frame,f3=objects.items[3].actor.frame;
 unsigned touch2=objects.enemies->items[2].touch,touch3=objects.enemies->items[3].touch;
 int floor_state=objects.items[5].actor.current;
 int ok=tomb_playtest_tick(p,input);TombActor *a=&p->lara.actor;
 fprintf(jaw_log,"%u,%d,%d,%d,%d,%d,%d,%d,%d,%u,%d,%d,%d,%d,%d,%d,%d,%d,%u,%d,%d,%u,%u,%d\n",input,health,p->lara.health,before.x,before.y,before.z,before.yaw,before.animation,before.frame,before.flags,a->x,a->y,a->z,a->yaw,a->current,a->animation,a->frame,a->fall_speed,a->flags,f2,f3,touch2,touch3,floor_state);
 return ok;
}
int main(int argc,char **argv) {
#ifndef TOMB_BUILD_TESTS
    /* Test switches must not silently run an ordinary game build. */
    for(int i=1;i<argc;i++)if(!strncmp(argv[i],"--",2) && strstr(argv[i],"-test")) {
        fprintf(stderr,"Test switches require a build made with -RunTests.\n");
        return 2;
    }
#endif
    int shotgun_test=0,city_test=0,combat_test=0,pickup_test=0,finish_test=0,select_level=0;
    char path[MAX_PATH],error[256]; const char *shot=NULL,*trace=NULL; double shot_time=0; int interest_test=0,quest_ui_test=0, simulate=0, ledge_test=0, fast_fall_test=0, jump_test=0, standing_test=0, grab_test=0, climb_test=0, window_input_test=0, pool_test=0, forward_test=0, vault_test=0,gym_test=0,render_test=0,slide_test=0,switch_test=0,hazard_test=0; uint32_t scripted_input=0;
    title_mode=argc==1;if(title_mode)caves=1;
    source_root_init();source_scan();asset_path(path,"GYM.PHD");
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--title")){title_mode=1;caves=1;}
        else if(!strcmp(argv[i],"--autosave-switch-test")){autosave_switch_test=front_test=caves=1;}
        else if(!strcmp(argv[i],"--frontend-test")){front_test=1;caves=1;}
        else if(!strcmp(argv[i],"--campaign-test"))lighting_test=1;
        else if(!strcmp(argv[i],"--colour-options-test") || !strcmp(argv[i],"--details-menu-test"))lighting_test=1;
        else if(!strcmp(argv[i],"--pickup3d-test") || !strcmp(argv[i],"--pickup-depth-test") || !strcmp(argv[i],"--lighting-test") || !strcmp(argv[i],"--render-options-test") || !strcmp(argv[i],"--original-review-test") || !strcmp(argv[i],"--consistent-weapons-test"))lighting_test=1;
        else if(!strcmp(argv[i],"--cutscene-resolution-test"))lighting_test=1;
        else if(!strcmp(argv[i],"--water-render-test"))water_render_test=1;
        else if(!strcmp(argv[i],"--interpolation-test"))interpolation_test=1;
        else if(!strcmp(argv[i],"--frontend-capture") && i+1<argc){front_capture=atoi(argv[++i]);title_mode=1;caves=1;}
        else if(!strcmp(argv[i],"--jaw-diagnostic")){jaw_diagnostic=1;level_number=7;caves=1;}
        else if(!strcmp(argv[i],"--jaw-render-test")){jaw_render_test=1;level_number=7;caves=1;}
        else if(!strcmp(argv[i],"--door-render-test"))door_render_test=1;
        else if(!strcmp(argv[i],"--block-render-test"))block_render_test=1;
        else if(!strcmp(argv[i],"--remaining-render-test"))remaining_render_test=1;
        else if(!strcmp(argv[i],"--media-test"))media_test=1;
        else if(!strcmp(argv[i],"--cutscene"))cutscene_only=1;
        else if(!strcmp(argv[i],"--fmv") && i+1<argc)movie_only=argv[++i];
        else if(!strcmp(argv[i],"--capture") && i+1<argc) shot=argv[++i];
        else if(!strcmp(argv[i],"--level-menu-test"))return level_picker(1)==UB_LAST_LEVEL && level_picker(2)==-1?0:2;
        else if(!strcmp(argv[i],"--level-menu"))select_level=1;
        else if(!strcmp(argv[i],"--level") && i+1<argc){
            char *end=NULL;long n=strtol(argv[++i],&end,10);
            if(!end || *end || n<0 || n>=LEVEL_CHOICE_COUNT)return 1;
            level_number=(int)n;caves=n!=0;
        }
        else if(!strcmp(argv[i],"--caves")){caves=1;level_number=1;}
        else if(!strcmp(argv[i],"--xr-probe") || !strcmp(argv[i],"--vr-render-test") || !strcmp(argv[i],"--vr-movie-test")){caves=1;level_number=1;}
        else if(!strcmp(argv[i],"--interest-test"))interest_test=1;
        else if(!strcmp(argv[i],"--quest-ui-test"))quest_ui_test=1;
        else if(!strcmp(argv[i],"--city")){caves=1;level_number=2;}
        else if(!strcmp(argv[i],"--city-test") && i+1<argc)city_test=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--shotgun-test"))shotgun_test=1;
        else if(!strcmp(argv[i],"--pickup-test"))pickup_test=1;
        else if(!strcmp(argv[i],"--finish-test"))finish_test=1;
        else if(!strcmp(argv[i],"--combat-test") && i+1<argc) combat_test=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--hazard-test") && i+1<argc) hazard_test=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--switch-test") && i+1<argc) switch_test=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--slide-test") && i+1<argc) slide_test=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--render-test") && i+1<argc) render_test=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--gym-test") && i+1<argc) gym_test=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--pool-test")) pool_test=1;
        else if(!strcmp(argv[i],"--forward-test")) forward_test=1;
        else if(!strcmp(argv[i],"--vault-test") && i+1<argc) vault_test=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--window-input-test")) window_input_test=1;
        else if(!strcmp(argv[i],"--stats-input-test")) window_input_test=4;
        else if(!strcmp(argv[i],"--combined-audio-test")) window_input_test=5;
        else if(!strcmp(argv[i],"--inventory-cursor-test"))window_input_test=3;
        else if(!strcmp(argv[i],"--default-bindings-test")) window_input_test=2;
        else if(!strcmp(argv[i],"--media-audio-test"))media_test=2;
        else if(!strcmp(argv[i],"--climb-test")) { climb_test=1; grab_test=1; }
        else if(!strcmp(argv[i],"--grab-test")) grab_test=1;
        else if(!strcmp(argv[i],"--standing-test")) standing_test=1;
        else if(!strcmp(argv[i],"--jump-test")) jump_test=1;
        else if(!strcmp(argv[i],"--fast-fall-test")) fast_fall_test=1;
        else if(!strcmp(argv[i],"--ledge-test")) ledge_test=1;
        else if(!strcmp(argv[i],"--simulate") && i+1<argc) simulate=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--input") && i+1<argc) scripted_input=(uint32_t)strtoul(argv[++i],NULL,0);
        else if(!strcmp(argv[i],"--inventory-screen"))inventory_capture=1;
        else if(!strcmp(argv[i],"--ring-object") && i+1<argc)ring_object=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--ring-test") && i+1<argc)ring_capture=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--peru-test"))peru_test=1;
        else if(!strcmp(argv[i],"--folly-test"))folly_test=1;
        else if(!strcmp(argv[i],"--midas-test"))midas_test=1;
        else if(!strcmp(argv[i],"--transition-test"))transition_test=1;
        else if(!strcmp(argv[i],"--trace") && i+1<argc) trace=argv[++i];
        else if(!strcmp(argv[i],"--time") && i+1<argc) shot_time=atof(argv[++i]);
        else if(!strcmp(argv[i],"--distance") && i+1<argc) distance=atof(argv[++i]);
        else if(!strcmp(argv[i],"--orbit") && i+1<argc) { orbit=atof(argv[++i]); manual_camera=1; }
        else if(!strcmp(argv[i],"--follow-camera")) manual_camera=0;
        else if(!strcmp(argv[i],"--animation") && i+1<argc) animation=atoi(argv[++i]);
        else { MessageBoxA(NULL,"Usage: tomb_preview [--caves] [--capture image.ppm] [--time seconds] [--animation 0|1|11]","Preview",MB_ICONERROR); return 1; }
    }
    if(select_level){level_number=level_picker(0);if(level_number<0)return 0;caves=level_number!=0;home_from_level_select=level_number==0;}
    if((animation!=0 && animation!=1 && animation!=11) || !isfinite(shot_time) || shot_time<0 || !isfinite(orbit) || !isfinite(distance) || distance<600 || distance>5000) return 1;
    if(slide_test && (!caves || slide_test<1 || slide_test>2))return 1;
    if(caves) {
        if(render_test || gym_test || pool_test || forward_test || vault_test || climb_test || grab_test || standing_test || jump_test || fast_fall_test || ledge_test)return 1;
        game_level_path(path,level_number);campaign_selected=level_number>=UB_FIRST_LEVEL;
    }
    level=tomb_level_load(path,error,sizeof error);
    if(level)level->music_edition=music_source()==0?2:1;
    if(!level || !(visual=tomb_visual_load(level,error,sizeof error)) || !tomb_lara_start(level,&start)) {
        if(shot){FILE *failure=fopen("build/level-load-error.txt","w");if(failure){fprintf(failure,"%s: %s (level=%d visual=%d)\n",path,error,level!=NULL,visual!=NULL);fclose(failure);}}
        else MessageBoxA(NULL,error[0]?error:"Lara start data is unavailable.","Cannot load level",MB_ICONERROR); return 1;
    }
    snprintf(path,sizeof path,"%s/sine.bin",install_root);
    if(GetFileAttributesA(path)==INVALID_FILE_ATTRIBUTES)snprintf(path,sizeof path,"%s/work/reference-assets/sine.bin",install_root);
    FILE *sine_file=fopen(path,"rb");
    if(!sine_file || fread(sine_table,sizeof sine_table,1,sine_file)!=1) return 1;
    tomb_water_effect_init(&water_effect,sine_table,UINT32_C(0xd371f947));
    fclose(sine_file);
    room_rects=calloc(level->room_count,sizeof *room_rects);if(!room_rects)return 1;
    if(!tomb_objects_init(&objects,level,visual))return 1;
    if(!tomb_playtest_init(&play,level,sine_table,visual)) return 1;
    if(level_number==0){play.inventory.counts[0]=0;play.inventory.ammo[0]=0;}
    play.movement_only=caves;play.objects=caves?&objects:NULL;level_inventory=play.inventory;
    objects.progress->music.current_track=music_source()==0?0:tomb_music_level_track(level_number);
    if(interest_test){if(level_number!=1)return 1;interest_start();}
    if(city_test){if(level_number!=2 || city_test<1 || city_test>8 || !shot)return 1;city_start(city_test-1);}
    if(shotgun_test){if(!caves || !shot)return 1;tomb_inventory_add(&play.inventory,85);play.inventory.ammo[1]=120;play.requested_weapon=4;}
    if(pickup_test)pickup_start(0);
    if(finish_test)finish_start();
    if(combat_test){
        if(!caves || combat_test<1 || combat_test>6 || !shot)return 1;
        combat_start(combat_test<=3?combat_test+6:combat_test==4?19:18);
        if(combat_test==6){
            TombModel m;if(!tomb_visual_model(visual,18,&m))return 1;
            for(size_t j=0;j<objects.count;j++)if(objects.items[j].object==18 && objects.items[j].active){
                TombActor *a=&objects.items[j].actor;a->animation=(int16_t)(m.animation+6);a->frame=level->animations[a->animation].first_frame;a->current=a->goal=7;objects.enemies->items[j].touch=0x3000;
            }
        }
    }
    if(slide_test){if(!caves || slide_test<1 || slide_test>2 || !shot)return 1;slope_start(slide_test==2);}
    if(hazard_test){if(!caves || hazard_test<1 || hazard_test>3 || !shot)return 1;hazard_start(hazard_test-1);}
    if(switch_test){if(!caves || switch_test<1 || switch_test>3 || !shot)return 1;switch_start(switch_test-1);}
    if(render_test) {
        if(!shot || simulate<=0 || render_test<1 || render_test>2)return 1;
        play.lara.actor.x=render_test==1?44544:51712;
        play.lara.actor.y=render_test==1?-1280:1280;
        play.lara.actor.z=render_test==1?51000:40448;
        play.lara.actor.room=render_test==1?7:11;
        play.lara.actor.yaw=0;
    }
    if(ledge_test) {
        if(!shot || simulate<=0) return 1;
        play.lara.actor.x=36600; play.lara.actor.y=-1536;
    }
    if(fast_fall_test) {
        if(!shot || simulate<=0 || ledge_test) return 1;
        play.lara.actor.x=46592; play.lara.actor.y=-3000; play.lara.actor.z=41472; play.lara.actor.room=9;
        play.lara.actor.current=play.lara.actor.goal=29; play.lara.actor.animation=93; play.lara.actor.frame=1473;
        play.lara.actor.flags|=8;
    }
    if(jump_test || standing_test) {
        if(!shot || simulate<=0 || ledge_test || fast_fall_test) return 1;
        play.lara.actor.x=46592; play.lara.actor.y=0; play.lara.actor.z=37000; play.lara.actor.room=9; play.lara.actor.yaw=0;
    }
    if(standing_test) {
        /* Point each directional test into the same open corridor. */
        if(scripted_input==18) play.lara.actor.yaw=-32768;
        else if(scripted_input==24) play.lara.actor.yaw=-16384;
        else if(scripted_input==20) play.lara.actor.yaw=16384;
    }
    if(grab_test) {
        if(!shot || simulate<=0 || ledge_test || fast_fall_test || jump_test || standing_test) return 1;
        play.lara.actor.x=47616; play.lara.actor.y=1024; play.lara.actor.z=35940;
        play.lara.actor.room=9; play.lara.actor.yaw=-32768;
        if(climb_test) { play.lara.actor.x=50076; play.lara.actor.y=2560; play.lara.actor.z=39424; play.lara.actor.yaw=16384; }
    }
    if(pool_test) pool_start();
    if(forward_test) {
        play.lara.actor.x=47616;play.lara.actor.y=768;play.lara.actor.z=38140;play.lara.actor.room=9;play.lara.actor.yaw=-32768;
    }
    if(vault_test) {
        if(vault_test<1 || vault_test>3) return 1;
        const int pos[3][5]={{42908,-1280,52736,7,16384},{49664,2560,40860,9,0},{47616,1024,35940,9,-32768}};
        const int *v=pos[vault_test-1];play.lara.actor.x=v[0];play.lara.actor.y=v[1];play.lara.actor.z=v[2];play.lara.actor.room=(int16_t)v[3];play.lara.actor.yaw=(int16_t)v[4];
    }
    if(gym_test) {
        if(!shot || simulate<=0 || gym_test<1 || gym_test>3)return 1;
        TombActor *a=&play.lara.actor;
        if(gym_test==1){a->x=46592;a->y=-5400;a->z=41472;a->room=9;a->current=a->goal=29;a->animation=93;a->frame=1473;a->flags|=8;}
        if(gym_test==2){a->x=40448;a->y=4800;a->z=60928;a->room=14;a->current=a->goal=13;a->animation=108;a->frame=1736;play.lara.water_status=1;play.lara.air=0;play.lara.health=5;}
        if(gym_test==3){a->x=50688;a->y=0;a->z=46592;a->room=9;}
    }
    pose_mode=shot && !simulate && !interpolation_test;
    if(simulate<0 || simulate>100000) return 1;
    FILE *log=trace?fopen(trace,"w"):NULL;
    if(trace && !log) return 1;
    if(log) fprintf(log,"tick,x,y,z,yaw,state,animation,frame,blocked,flags,fall_speed,health,water,pitch,air\n");
    int climb_done=0,vault_started=0;
    if(!manual_camera && !pose_mode) update_camera(1.0/30);
    for(int tick=0;tick<simulate;tick++) {
        unsigned new_input=city_test==1 && tick<45?64:combat_test?(tick==0?32:scripted_input):switch_test?(tick==0?64:scripted_input):pool_test?(play.lara.water_status==0?(tick<35?1:0):play.lara.water_status==1?18:65):forward_test?(tick<20?1:tick<40?81:64):vault_test?(vault_started?64:65):scripted_input;
        tomb_playtest_tick(&play,grab_test?(tick<15?80:tick<75?64:climb_test && !climb_done && tick<200?scripted_input:0):standing_test?(tick<15?scripted_input:0):jump_test?(tick<20?1:tick<40?17:0):new_input);
        water_particles_tick();
        if(!manual_camera) update_camera(1.0/30);
        const TombActor *a=&play.lara.actor;
        if(vault_test && (a->current==19 || a->current==28)) vault_started=1;
        if(climb_test && tick>75 && a->current==2) climb_done=1;
        if(log) fprintf(log,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%u,%d,%d,%d,%d,%d\n",tick,a->x,a->y,a->z,a->yaw,a->current,a->animation,a->frame,play.blocked,a->flags,a->fall_speed,play.lara.health,play.lara.water_status,play.lara.pitch,play.lara.air);
    }
    if(log) fclose(log);
    HINSTANCE instance=GetModuleHandleA(NULL);
    WNDCLASSA wc={0}; wc.style=CS_OWNDC; wc.lpfnWndProc=window_proc; wc.hInstance=instance;
    wc.lpszClassName="TombReconstructionPreview"; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    if(!RegisterClassA(&wc)) return 1;
    /* width/height describe the client area, excluding the title and borders. */
    RECT initial_window={0,0,width,height};
    if(!AdjustWindowRectEx(&initial_window,WS_OVERLAPPEDWINDOW,FALSE,0))return 1;
    HWND window=CreateWindowA(wc.lpszClassName,"Tomb Raider Beyond",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,initial_window.right-initial_window.left,initial_window.bottom-initial_window.top,NULL,NULL,instance,NULL);
    if(!window) return 1;
    if(window_input_test) {
        HCURSOR saved_cursor=GetCursor();
        int cursor_ok=1;
        for(int repeat=0;repeat<3;repeat++){
            media_busy=1;SetCursor(LoadCursor(NULL,IDC_ARROW));
            SendMessageA(window,WM_SETCURSOR,(WPARAM)window,MAKELPARAM(HTCLIENT,WM_MOUSEMOVE));
            cursor_ok=cursor_ok && GetCursor()==NULL;
            SendMessageA(window,WM_SETCURSOR,(WPARAM)window,MAKELPARAM(HTCAPTION,WM_MOUSEMOVE));
            cursor_ok=cursor_ok && GetCursor()!=NULL;
            media_busy=0;
            SendMessageA(window,WM_SETCURSOR,(WPARAM)window,MAKELPARAM(HTCLIENT,WM_MOUSEMOVE));
            cursor_ok=cursor_ok && GetCursor()!=NULL;
        }
        /* Both keyboard and quest inventory entry use the same transition. */
        for(int full=0;full<2;full++){
            fullscreen=full;SetCapture(window);ShowCursor(FALSE);mouse_captured=mouse_camera_active=1;
            mouse_inventory_begin();
            cursor_ok=cursor_ok && !mouse_camera_active && mouse_captured==full;
            cursor_ok=cursor_ok && (GetCapture()==window)==full;
            mouse_release();
        }
        fullscreen=0;
        SetCursor(saved_cursor);
        if(window_input_test==3){DestroyWindow(window);tomb_visual_free(visual);tomb_level_free(level);return cursor_ok?0:2;}
        if(window_input_test==5){
            front_test=1;
            int passed=source_choose_media(11,3)==3 && source_choose_media(2,3)==1;
            passed=passed && source_cycle_mask(0,1,11)==3 && source_cycle_mask(3,1,11)==1;
            const unsigned keys[]={VK_F1,VK_F2,VK_F3,VK_F4,VK_F7,VK_F8,VK_F9};
            int *values[]={&settings_detail,&settings_force43,&settings_framerate,&settings_full_colour,&settings_texture_filtering,&settings_draw_distance,&settings_water_style};
            for(int i=0;i<7;i++){
                int old=*values[i];SendMessageA(window,WM_KEYDOWN,keys[i],0);
                passed=passed && *values[i]!=old && settings_preset==-1;
                passed=passed && detail_notice_time==3.0 && strstr(detail_notice,": ");
                int next=*values[i];SendMessageA(window,WM_KEYDOWN,keys[i],(LPARAM)(1u<<30));
                passed=passed && *values[i]==next;SendMessageA(window,WM_KEYUP,keys[i],0);
            }
            snprintf(install_root,sizeof install_root,"work/audio-fixture");source_legacy=0;
            music_selected=3;title_mode=0;caves=1;level_number=16;
            audio_stop();objects.progress->music.current_track=0;objects.progress->music.audio_serial=0;
            audio_update(0,.25);passed=passed && music_output && combined_background && playing_track==59 && combined_background_gain==.25;
            audio_update(1,.5);passed=passed && combined_background_gain==.25;
            objects.progress->music.current_track=13;++objects.progress->music.audio_serial;
            audio_update(0,.01);passed=passed && music_output && !combined_background && playing_track==13;
            objects.progress->music.current_track=14;++objects.progress->music.audio_serial;
            audio_update(0,.01);passed=passed && music_output && !combined_background && playing_track==14;
            music_sample.frames=0;audio_update(0,.1);
            passed=passed && music_output && combined_background && playing_track==59 && combined_background_gain==.1;
            audio_update(0,2);passed=passed && combined_background_gain==1;
            audio_stop();title_mode=1;audio_update(0,.2);
            passed=passed && music_output && !combined_background && playing_track==2;
            audio_stop();fprintf(stderr,"Combined audio and detail shortcuts: %s\n",passed?"PASS":"FAIL");
            DestroyWindow(window);tomb_visual_free(visual);tomb_level_free(level);return passed?0:2;
        }
        if(window_input_test==4){
            int passed=1;
            const unsigned keys[]={VK_CONTROL,VK_RETURN};
            for(int i=0;i<2;i++){
                memset(held_keys,0,sizeof held_keys);stats_consumed_keys[keys[i]]=1;
                SendMessageA(window,WM_KEYDOWN,keys[i],(LPARAM)(1u<<30));passed=passed && !game_input();
                SendMessageA(window,WM_KEYUP,keys[i],0);passed=passed && !stats_consumed_keys[keys[i]] && !game_input();
                SendMessageA(window,WM_KEYDOWN,keys[i],0);passed=passed && game_input()==64;
                SendMessageA(window,WM_KEYUP,keys[i],0);
            }
            fprintf(stderr,"Stats confirmation release test: %s\n",passed?"PASS":"FAIL");
            DestroyWindow(window);tomb_visual_free(visual);tomb_level_free(level);return passed?0:2;
        }
        SendMessageA(window,WM_SYSKEYDOWN,VK_MENU,0x20380001);
        SendMessageA(window,WM_SYSKEYUP,VK_MENU,(LPARAM)0xc0380001u);
        SendMessageA(window,WM_SYSCOMMAND,SC_KEYMENU,0);
        SendMessageA(window,WM_SYSCOMMAND,SC_KEYMENU|3,'w');
        char window_title[64];GetWindowTextA(window,window_title,sizeof window_title);
        int passed=cursor_ok && menu_entries==0 && !paused && running && !strcmp(window_title,"Tomb Raider Beyond");
        RECT before,after,client_rect;GetWindowRect(window,&before);GetClientRect(window,&client_rect);
        passed=passed && client_rect.right*3==client_rect.bottom*4;
        MONITORINFO monitor={0};monitor.cbSize=sizeof monitor;
        GetMonitorInfoA(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor);
        unsigned saved_inventory_input=inventory_input;
        SendMessageA(window,WM_SYSKEYDOWN,VK_RETURN,(LPARAM)(1u<<29));
        GetWindowRect(window,&after);GetClientRect(window,&client_rect);
        passed=passed && fullscreen && EqualRect(&after,&monitor.rcMonitor) &&
            client_rect.right==monitor.rcMonitor.right-monitor.rcMonitor.left &&
            client_rect.bottom==monitor.rcMonitor.bottom-monitor.rcMonitor.top &&
            !held_keys[VK_RETURN] && inventory_input==saved_inventory_input;
        SendMessageA(window,WM_SYSKEYDOWN,VK_RETURN,(LPARAM)((1u<<29)|(1u<<30)));
        passed=passed && fullscreen; /* Auto-repeat must not toggle again. */
        SendMessageA(window,WM_SYSKEYUP,VK_RETURN,(LPARAM)(1u<<29));
        SendMessageA(window,WM_SYSKEYDOWN,VK_RETURN,(LPARAM)(1u<<29));
        SendMessageA(window,WM_KEYUP,VK_RETURN,0); /* Alt may be released first. */
        GetWindowRect(window,&after);
        passed=passed && !fullscreen && EqualRect(&before,&after) && !held_keys[VK_RETURN];
        SendMessageA(window,WM_KEYDOWN,'P',1); passed=passed && paused;
        SendMessageA(window,WM_KEYDOWN,'P',1); passed=passed && !paused;
        orbit=1;camera.ready=1;camera_hold=1;
        SendMessageA(window,WM_KEYDOWN,'C',1);
        passed=passed && orbit==1 && camera_hold==1 && camera.ready;
        {
            const unsigned keys[]={VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_DELETE,VK_NEXT,VK_SHIFT,VK_MENU,VK_CONTROL,VK_RETURN,VK_END,VK_NUMPAD0};
            const unsigned bits[]={1,2,4,8,1024,2048,128,16,64,64,4096,512};
            memset(held_keys,0,sizeof held_keys);
            for(unsigned k=0;k<sizeof keys/sizeof *keys;k++) {
                SendMessageA(window,WM_KEYDOWN,keys[k],1);passed=passed && game_input()==bits[k];
                SendMessageA(window,WM_KEYUP,keys[k],1);passed=passed && !game_input();
            }
            /* A stats-confirmation key stays consumed through repeat events. */
            stats_consumed_keys[VK_CONTROL]=1;
            SendMessageA(window,WM_KEYDOWN,VK_CONTROL,(LPARAM)(1u<<30));passed=passed && !game_input();
            SendMessageA(window,WM_KEYUP,VK_CONTROL,0);passed=passed && !stats_consumed_keys[VK_CONTROL] && !game_input();
            SendMessageA(window,WM_KEYDOWN,VK_CONTROL,0);passed=passed && game_input()==64;
            SendMessageA(window,WM_KEYUP,VK_CONTROL,0);
            const unsigned nav[]={VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT};
            const unsigned scans[]={0x48,0x50,0x4b,0x4d,0x47,0x49};
            for(unsigned k=0;k<4;k++) {
                SendMessageA(window,WM_KEYDOWN,nav[k],(LPARAM)(scans[k]<<16));passed=passed && !game_input();
                SendMessageA(window,WM_KEYUP,nav[k],(LPARAM)(scans[k]<<16));
                SendMessageA(window,WM_KEYDOWN,nav[k],(LPARAM)((scans[k]<<16)|(1u<<24)));passed=passed && game_input()==bits[k];
                SendMessageA(window,WM_KEYUP,nav[k],(LPARAM)((scans[k]<<16)|(1u<<24)));
            }
            SendMessageA(window,WM_KEYDOWN,VK_INSERT,(LPARAM)(0x52u<<16));passed=passed && game_input()==512;
            SendMessageA(window,WM_KEYUP,VK_INSERT,(LPARAM)(0x52u<<16));passed=passed && !game_input();
            const unsigned removed[]={'Q','J','A','D','S','W','E',VK_TAB,'C','1','2','3'};
            for(unsigned k=0;k<sizeof removed/sizeof *removed;k++) {
                SendMessageA(window,WM_KEYDOWN,removed[k],1);passed=passed && !game_input() && !pose_mode && !inventory_open;
                SendMessageA(window,WM_KEYUP,removed[k],1);
            }
            SendMessageA(window,WM_KEYDOWN,VK_SHIFT,1);SendMessageA(window,WM_KEYDOWN,VK_UP,1);
            passed=passed && game_input()==129;SendMessageA(window,WM_KEYUP,VK_SHIFT,1);
            SendMessageA(window,WM_KEYDOWN,VK_UP,1);SendMessageA(window,WM_KILLFOCUS,0,0);passed=passed && !game_input();
            unsigned previous=bindings[0];bindings[0]='T';
            SendMessageA(window,WM_KEYDOWN,'T',1);passed=passed && game_input()==1;
            SendMessageA(window,WM_KEYUP,'T',1);bindings[0]=previous;
            SendMessageA(window,WM_KEYDOWN,VK_ESCAPE,1);passed=passed && inventory_open;
            SendMessageA(window,WM_KEYUP,VK_ESCAPE,1);inventory_open=0;inventory_input=0;
        }
        if(window_input_test==2){
            /* Binding verification is independent of the legacy scene fixtures. */
            DestroyWindow(window);tomb_visual_free(visual);tomb_level_free(level);
            return passed?0:2;
        }
        for(unsigned key=VK_F1;key<=VK_F12;key++){
            TombActor before=play.lara.actor;
            SendMessageA(window,WM_KEYDOWN,key,1);SendMessageA(window,WM_KEYUP,key,1);
            passed=passed && !memcmp(&before,&play.lara.actor,sizeof before);
        }
        if(caves) {

            for(int fixture=0;fixture<3;fixture++) {
                combat_start(fixture+7);
                passed=passed && objects.enemies->head>=0 && objects.items[objects.enemies->head].object==fixture+7;
                SendMessageA(window,WM_KEYDOWN,VK_SPACE,1);passed=passed && draw_requested && !paused;
                passed=passed && tomb_playtest_tick(&play,draw_requested?32:0);draw_requested=0;
                for(int t=0;t<24;t++)passed=passed && tomb_playtest_tick(&play,64);
                passed=passed && play.pistols.status==4 && play.pistols.shots>0;
            }
            for(int fixture=0;fixture<3;fixture++) {
                hazard_start(fixture);
                static const int rooms[3]={14,32,6};passed=passed && play.lara.actor.room==rooms[fixture] && !paused && !camera.ready;
            }
            static const unsigned ids[3]={10,42,52};
            for(int i=0;i<3;i++) {
                switch_start(i);
                const TombActor *s=&objects.items[ids[i]].actor;
                passed=passed && play.lara.actor.room==s->room && play.lara.actor.yaw==s->yaw;
                int saw_camera=0;
                for(int tick=0;tick<100;tick++) {
                    passed=passed && tomb_playtest_tick(&play,64);
                    update_camera(1.0/30);
                    if(objects.camera.active)saw_camera=1;
                }
                passed=passed && (saw_camera==(i!=0));
                SendMessageA(window,i==1?WM_SYSKEYUP:WM_KEYUP,VK_F9+i,(LPARAM)0xc0000001u);
            }
            passed=passed && !menu_entries;
            memset(held_keys,0,sizeof held_keys);
            pickup_start(0);
            for(int t=0;t<110;t++)passed=passed && tomb_playtest_tick(&play,64);
            SendMessageA(window,WM_KEYDOWN,VK_ESCAPE,1);passed=passed && inventory_open;
            SendMessageA(window,WM_KEYUP,VK_ESCAPE,1);
            for(int t=0;t<20;t++)inventory_tick(0);
            SendMessageA(window,WM_KEYDOWN,VK_RETURN,1);passed=passed && inventory_input==TOMB_RING_SELECT;
            SendMessageA(window,WM_KEYUP,VK_RETURN,1);inventory_input=0;
            /* Right moves to the previous entry, wrapping compass to medipack. */
            SendMessageA(window,WM_KEYDOWN,VK_RIGHT,1);inventory_tick(inventory_input);inventory_input=0;
            for(int t=0;t<14;t++)inventory_tick(0);
            play.lara.health=200;SendMessageA(window,WM_KEYDOWN,VK_CONTROL,1);inventory_tick(inventory_input);inventory_input=0;
            passed=passed && inventory_open && play.lara.health==200;
            for(int t=0;t<180 && inventory_open;t++)inventory_tick(0);
            passed=passed && !inventory_open && play.lara.health==700 && !play.inventory.counts[9];
            SendMessageA(window,WM_KEYUP,VK_CONTROL,1);
            SendMessageA(window,WM_KEYDOWN,VK_ESCAPE,1);passed=passed && inventory_open && running;
            for(int t=0;t<20;t++)inventory_tick(0);
            SendMessageA(window,WM_KEYDOWN,VK_ESCAPE,1);inventory_tick(inventory_input);inventory_input=0;
            passed=passed && inventory_open;
            for(int t=0;t<18;t++)inventory_tick(0);passed=passed && !inventory_open;

        } else {
            pool_start();
            passed=passed && play.lara.actor.room==13 && play.lara.actor.x==40448 && play.lara.actor.z==57800 && !play.lara.water_status;
        }
        SendMessageA(window,WM_KEYDOWN,'R',1);
        passed=passed && play.lara.actor.room==start.actor.room && play.lara.actor.x==start.actor.x;
        SendMessageA(window,WM_SYSCOMMAND,SC_CLOSE,0); passed=passed && !running;
        DestroyWindow(window); tomb_visual_free(visual); tomb_level_free(level);
        return passed?0:2;
    }
    if(quest_ui_test){
        if(level_number!=2)return 2;
        for(int variant=0;variant<2;variant++){
            city_start(5+variant);
            if(!tomb_playtest_tick(&play,64) || !play.quest_request)return 3;
            inventory_begin();for(int t=0;t<20;t++)inventory_tick(0);
            if(!inventory_keys || !inventory_open || inventory_ring.count!=1)return 4;
            /* Exercise the configured Action key as well as Enter. */
            SendMessageA(window,WM_KEYDOWN,variant?VK_RETURN:bindings[8],1);inventory_tick(inventory_input);inventory_input=0;
            for(int t=0;t<100 && inventory_open;t++)inventory_tick(0);
            if(inventory_open || play.inventory.chosen!=(variant?114:133))return 5;
            for(int t=0;t<180;t++)if(!tomb_playtest_tick(&play,0))return 6;
            if(!objects.doors[variant?78:8].open)return 7;
        }
        city_start(5);inventory_begin();for(int t=0;t<20;t++)inventory_tick(0);
        SendMessageA(window,WM_KEYDOWN,VK_NUMPAD8,1);inventory_tick(inventory_input);inventory_input=0;
        for(int t=0;t<30;t++)inventory_tick(0);
        if(!inventory_keys || inventory_ring.motion.status!=1 || inventory_ring.radius!=688 || inventory_ring.pitch)return 8;
        SendMessageA(window,WM_KEYDOWN,VK_NUMPAD2,1);inventory_tick(inventory_input);inventory_input=0;
        for(int t=0;t<30;t++)inventory_tick(0);
        if(inventory_keys || inventory_ring.motion.status!=1 || inventory_ring.radius!=688 || inventory_ring.pitch)return 9;
        DestroyWindow(window);return 0;
    }
    display_size(window);
    HDC dc=GetDC(window); PIXELFORMATDESCRIPTOR pfd={0}; pfd.nSize=sizeof pfd; pfd.nVersion=1;
    pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER; pfd.iPixelType=PFD_TYPE_RGBA;
    pfd.cColorBits=24; pfd.cDepthBits=24;
    int format=ChoosePixelFormat(dc,&pfd);
    if(!format || !SetPixelFormat(dc,format,&pfd)) return 1;
    HGLRC context=wglCreateContext(dc);
    if(!context || !wglMakeCurrent(dc,context)) return 1;
    glEnable(GL_DEPTH_TEST); glAlphaFunc(GL_GREATER,.5f); glClearColor(0,0,0,1);
    if(!make_tiles())return 1;
    if(strstr(GetCommandLineA(),"--xr-probe")){int available=xr_open();FILE *report=fopen("build/xr-probe.txt","w");if(report){fprintf(report,"OpenGL: %s\nOpenXR session: %s\n%s\n",glGetString(GL_VERSION),available?"created":"unavailable",xr.error);fclose(report);}xr_close();return available?0:2;}
#ifdef TOMB_BUILD_TESTS
    if(strstr(GetCommandLineA(),"--vr-render-test"))return vr_render_check()?0:2;
    if(jaw_render_test)return jaw_render_check()?0:2;
    if(door_render_test)return door_render_check()?0:2;
    if(block_render_test)return block_render_check()?0:2;
    if(remaining_render_test)return remaining_render_check()?0:2;
#endif
    if(strstr(GetCommandLineA(),"--vr-movie-test"))return play_movie("LIFT",1)?0:2;
    if(media_test){
        if(!media_audio_check())return 7;
        if(media_test==2)return 0;
        static const char *names[]={"CAFE","CANYON","CORE","END","ESCAPE","LIFT","MANSION","PRISON","PYRAMID","SNOW","VISION"};
        for(int i=0;i<11;i++)if(!play_movie(names[i],1))return 2;
        objects.progress->complete=1;
        for(int sequence=1;sequence<=4;sequence++){
            if(!play_cutscene_sequence(1,sequence) || !cinematic_pixels)return 3;
            backdrop_dirty=1;overlay();unsigned char pixel[3];glReadPixels(0,0,1,1,GL_RGB,GL_UNSIGNED_BYTE,pixel);
            int best=0,dist=INT_MAX;
            for(int i=0;i<256;i++){int d=0;for(int c=0;c<3;c++){int delta=(cinematic_pixels[c]>>3)*255/31-cinematic_palette[3*i+c]*255/63;d+=delta*delta;}if(d<dist){dist=d;best=i;}}
            for(int c=0;c<3;c++)if(pixel[c]!=cinematic_palette[3*frontend_dark_index(cinematic_palette,cinematic_light,best)+c]*255/63)return 8;
        }
        objects.progress->complete=0;free(cinematic_pixels);cinematic_pixels=NULL;
        if(!play_ending(1)){FILE *f=fopen("build/late-media-failure.txt","w");if(f){fprintf(f,"credits failed\n");fclose(f);}return 3;}
        int old_pause=paused,old_inventory=inventory_open;media_busy=1;media_skip=0;
        window_proc(window,WM_KEYDOWN,'P',1);window_proc(window,WM_KEYDOWN,'R',1);window_proc(window,WM_KEYDOWN,'E',1);
        if(paused!=old_pause || inventory_open!=old_inventory || media_skip)return 5;
        window_proc(window,WM_KEYDOWN,VK_ESCAPE,1);if(!media_skip)return 6;media_end();
        return draw()?0:4;
    }
#ifdef TOMB_BUILD_TESTS
    if(peru_test)return peru_render_check()?0:2;
    if(folly_test)return folly_render_check()?0:2;
    if(midas_test)return midas_render_check()?0:2;
    if(strstr(GetCommandLineA(),"--consistent-weapons-test"))return consistent_weapons_check()?0:2;
    if(strstr(GetCommandLineA(),"--original-review-test"))return original_review_check()?0:2;
    if(strstr(GetCommandLineA(),"--render-options-test"))return render_options_check()?0:2;
    if(strstr(GetCommandLineA(),"--pickup3d-test"))return pickup3d_check()?0:2;
    if(strstr(GetCommandLineA(),"--pickup-depth-test"))return pickup_depth_check()?0:2;
    if(strstr(GetCommandLineA(),"--details-menu-test"))return details_menu_check()?0:2;
    if(strstr(GetCommandLineA(),"--colour-options-test"))return colour_options_check()?0:2;
    if(strstr(GetCommandLineA(),"--cutscene-resolution-test")){
        objects.progress->complete=1;
        for(int detail=0;detail<4;detail++){
            settings_detail=detail;
            for(int sequence=1;sequence<=4;sequence++){
                if(!play_cutscene_sequence(1,sequence) || !cinematic_pixels)return 2;
            }
        }
        fprintf(stderr,"PASS: all four cutscenes at all four resolutions, final backdrop matches displayed frame\n");return 0;
    }
    if(strstr(GetCommandLineA(),"--campaign-test"))return campaign_check()?0:2;
    if(lighting_test)return lighting_render_check()?0:2;
    if(water_render_test)return water_render_check()?0:2;
    if(interpolation_test){int passed=interpolation_render_check();if(passed && shot)passed=capture(shot);return passed?0:2;}
#endif
    if(!frontend_assets())return 1;
    if(!shot && !front_test){settings_read();settings_write();}
    display_size(window);
#ifdef TOMB_BUILD_TESTS
    if(autosave_switch_test)return autosave_switch_check()?0:2;
    if(front_test)return frontend_check()?0:2;
#endif
    if(title_mode)frontend_title();
    if(front_capture){
        for(int t=0;t<32;t++)inventory_tick(0);
        if(front_capture>=3 && front_capture<=5){int object=front_capture==3?95:front_capture==4?96:97;while(inventory_ring.items[inventory_ring.current].object!=object){inventory_tick(TOMB_RING_RIGHT);for(int t=0;t<32;t++)inventory_tick(0);}}
        if(front_capture>1){inventory_tick(TOMB_RING_SELECT);for(int t=0;t<90;t++)inventory_tick(0);}
        if(front_capture==7){
            if(inventory_ring.items[0].frame==14){inventory_tick(TOMB_RING_RIGHT);for(int t=0;t<30;t++)inventory_tick(0);}
            inventory_tick(TOMB_RING_SELECT);
        }
        if(front_capture==6){
            if(!load_game_level(1,0,NULL))return 2;
            inventory_begin();for(int t=0;t<32;t++)inventory_tick(0);inventory_tick(32);for(int t=0;t<32;t++)inventory_tick(0);
            inventory_tick(TOMB_RING_SELECT);for(int t=0;t<90;t++)inventory_tick(0);
            inventory_tick(TOMB_RING_SELECT);slot_current=15;
        }
    }
    if(transition_test) {
        int expected_level=level_number+1;
        if(level_number==2 || level_number==3){finish_start();if(!tomb_playtest_tick(&play,0) || !objects.progress->complete)return 2;}
        else if(level_number>=4)objects.progress->complete=1;
        tomb_inventory_add(&play.inventory,93);tomb_inventory_add(&play.inventory,89);play.lara.health=400;
        play.inventory.quest[4]=1;play.inventory.ticks=900;play.inventory.pickups=3;
        if(expected_level>=3)SendMessageA(window,WM_KEYDOWN,VK_RETURN,1);
        else if(!load_next_level())return 2;
        if(level_number!=expected_level || play.inventory.quest[4] || play.inventory.ticks || play.inventory.pickups || play.inventory.counts[9]!=1 || play.inventory.counts[5]!=1 || play.lara.health!=1000 || objects.progress->secrets)return 2;
        for(int t=0;t<120;t++)if(!tomb_playtest_tick(&play,t<60?1:0))return 2;
        update_camera(1.0/30);
    }
    if(inventory_capture || ring_capture>=0){
        inventory_begin();int frames=ring_capture>=0?ring_capture:32;
        for(int t=0;t<frames;t++)inventory_tick(t==32?(ring_object==72?0:ring_object==108?TOMB_RING_RIGHT:TOMB_RING_LEFT):t==50?TOMB_RING_SELECT:0);
    }

    if(!shot) {
        if(!sound_output && tomb_sound_bank_load(&sound_bank,level)) {
            tomb_sound_init(&sound_mixer,&sound_bank);
            sound_output=tomb_sound_output_open(&sound_mixer);
            tomb_sound_init(&ring_sound_mixer,&sound_bank);ring_sound_output=tomb_sound_output_open(&ring_sound_mixer);
        }
        if(!sound_output || !ring_sound_output)sound_failed=1;
        toggle_fullscreen(window);
        ShowWindow(window,SW_SHOW);
    }
    if(cutscene_only)return play_cutscene(0)?0:2;
    if(movie_only)return play_movie(movie_only,0)?0:2;
    if(title_mode && !shot && !front_test){play_movie("CORE",0);if(running)play_movie("ESCAPE",0);if(running)play_movie("CAFE",0);}
    if(jaw_diagnostic){
        if(!load_game_level(7,0,NULL) || !tomb_save_read("dist/caves-playtest-92/saves/slot02.tsv",7,&play,&objects,&camera,&level_inventory))return 2;
        play.lara.health=play.water.health=play.movement.health=1000;
        play.gameplay_options&=~(uint32_t)TOMB_AUTOSAVE;
        title_mode=inventory_open=paused=0;camera.ready=0;render_reset();update_camera(1.0/30);if(!render_capture())return 2;
        char log_path[128];snprintf(log_path,sizeof log_path,"build/jaw-live-%lu.csv",(unsigned long)GetTickCount());jaw_log=fopen(log_path,"w");if(!jaw_log)return 2;
        fputs("input,hp_before,hp_after,x_before,y_before,z_before,yaw_before,anim_before,frame_before,flags_before,x_after,y_after,z_after,yaw_after,state_after,anim_after,frame_after,fall_speed_after,flags_after,gate2_frame,gate3_frame,gate2_touch,gate3_touch,floor_state\n",jaw_log);
    }
    LARGE_INTEGER frequency,last,now; QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&last);
    int ok=1,was_suspended=1;ring_previous=inventory_ring;ring_interpolate=1;
    while(running) {
        display_size(window);
        autosave_poll();
        MSG msg;
        while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        QueryPerformanceCounter(&now); double dt=(double)(now.QuadPart-last.QuadPart)/frequency.QuadPart; last=now;
        if(dt>.1) dt=.1;
        if(!paused) elapsed+=dt;
        if(play.shooter.mode==TOMB_MODE_VR && !xr.instance){
            if(!xr_open()){MessageBoxA(window,xr.error,"Virtual Reality unavailable",MB_OK|MB_ICONWARNING);tomb_shooter_start(&play,0);frontend_title();}
        }
        TombVrInput vr_input={0};
        if(xr.instance){
            xr_begin(&vr_input);
            if(xr.reset){tomb_vr_reset(&vr,&play);xr.reset=0;}
            vr_ring_release(&vr_input);
            vr_menu_input(&vr_input,dt);
            int hold=paused || inventory_pauses() || title_mode || objects.progress->complete || xr.state!=XR_SESSION_STATE_FOCUSED;
            int old_hover[2]={vr.hands[0].hover,vr.hands[1].hover};
            vr.deadzone=settings_vr_deadzone/100.0;
            tomb_vr_frame(&vr,&play,&vr_input,dt,hold);
            for(int h=0;h<2;h++)if(vr.hands[h].hover>=0 && vr.hands[h].hover!=old_hover[h])xr_feedback(h,.25f);
            if(vr.menu_request && !inventory_open)inventory_begin();
        }
        int focused=xr.instance?xr.state==XR_SESSION_STATE_FOCUSED:GetActiveWindow()==window;
        int suspended=paused || inventory_pauses() || title_mode || objects.progress->complete || !focused;
        if(!shot && !pose_mode && !suspended){
            if(was_suspended)render_reset();
            if(!render_tick){
                if(camera.ready){memcpy(camera.previous_eye,camera.eye,sizeof camera.eye);memcpy(camera.previous_target,camera.target,sizeof camera.target);}
                if(!render_capture()){ok=0;break;}
            }
        }
        if(!suspended && !title_mode && level_name_time>0)level_name_time=fmax(0,level_name_time-dt);
        if(focused && detail_notice_time>0)detail_notice_time=fmax(0,detail_notice_time-dt);
        mouse_update(window,suspended);
        was_suspended=suspended;
        if(!shot && !pose_mode && !paused && !inventory_pauses() && !objects.progress->complete && focused) {
            uint32_t input=game_input();
            accumulator+=dt;
            while(accumulator>=1.0/30.0) { if(play.lara.health<=0 && tomb_death_menu_due(play.dead_ticks,input)){death_menu=1;inventory_begin();break;} if(play.shooter.mode==TOMB_MODE_VR?tomb_vr_tick(&vr,&play):jaw_diagnostic_tick(&play,input|(draw_requested?32:0))){water_particles_tick();if(sound_output)sound_tick();}draw_requested=0;if(frontend_home_complete())break;if(objects.progress->complete)backdrop_dirty=1; update_camera(1.0/30); if(!render_capture()){ok=0;running=0;break;} autosave_poll(); accumulator-=1.0/30.0; if(crystal_poll())break; if(play.quest_request){death_menu=play.lara.health<=0;inventory_begin();break;} }
        } else accumulator=0;
        if(!shot && inventory_open && focused) {
            inventory_accumulator+=dt;
            while(inventory_accumulator>=1.0/30.0 && inventory_open) {
                unsigned input=inventory_input;inventory_input=0;
                if(inventory_ring.motion.status==1 && (held_keys[VK_LEFT] || held_keys[VK_NUMPAD4]))input|=TOMB_RING_LEFT;
                if(inventory_ring.motion.status==1 && (held_keys[VK_RIGHT] || held_keys[VK_NUMPAD6]))input|=TOMB_RING_RIGHT;
                inventory_tick(input);inventory_accumulator-=1.0/30.0;
            }
        } else inventory_accumulator=0;
        if(!shot && !pose_mode && !paused && !inventory_pauses() && !objects.progress->complete && focused)camera_alpha=accumulator*30;
        if(!shot) {
            int suspended=paused || inventory_pauses() || objects.progress->complete || !focused;
            audio_update(suspended,dt);
            if(sound_output && !tomb_sound_output_update(sound_output,suspended))sound_failed=1;
            if(ring_sound_output && !tomb_sound_output_update(ring_sound_output,!focused))sound_failed=1;
        }
        if(!shot && (level_number==4 || level_number==9 || level_number==14) && objects.progress->complete && !cutscene_seen){
            cutscene_seen=1;if(level_number==14)play_movie("PRISON",0);if(!play_cutscene_sequence(0,level_number==4?1:level_number==9?2:level_number==13?3:4))MessageBoxA(window,"Could not play cinematic. Check converted CUT1-CUT4 levels and audio assets.","Tomb Raider Beyond",MB_OK|MB_ICONWARNING);
            backdrop_dirty=1;QueryPerformanceCounter(&last);
        }
        /* Preserve completion cinematics, then advance without drawing statistics. */
        if(!shot && running && !title_mode && !paused && focused && objects.progress->complete &&
           (play.gameplay_options&TOMB_SKIP_STATS)){
            if(xr.begun)xr_end(0,0);
            if(!load_next_level()){ok=0;break;}
            QueryPerformanceCounter(&last);continue;
        }
        if(shot) elapsed=shot_time;
        if(!draw_frame_composite(1)) { ok=0; break; }
        for(int h=0;h<2;h++)if(vr.fired[h]){xr_feedback(h,.65f);vr.fired[h]=0;}
        if(!vr_render()){ok=0;break;}
        if(shot) { ok=capture(shot); break; }
        display_present(dc);
        /* Active rendering is paced by VSync, the chosen cap or the XR runtime.
           Sleep(1) can last a full scheduler tick and impose an unintended cap. */
        if(!focused && !xr.instance)Sleep(16);
    }
    if(jaw_log){fclose(jaw_log);jaw_log=NULL;}
    xr_close();mouse_release();render_reset();frontend_free();audio_stop();
    tomb_sound_output_close(ring_sound_output);tomb_sound_output_close(sound_output);tomb_sound_bank_free(&sound_bank);
    glDeleteTextures((GLsizei)visual->tile_count,font_tiles);free(font_tiles);glDeleteTextures((GLsizei)(visual->tile_count*2+2),tiles); free(tiles);
    glDeleteTextures(2,shadow_textures);
    light_DeleteProgram(shade_program);
    wglMakeCurrent(NULL,NULL); wglDeleteContext(context); ReleaseDC(window,dc); DestroyWindow(window);
    free(room_rects);tomb_objects_free(&objects);tomb_visual_free(visual); tomb_level_free(level);
    return ok?0:1;
}
