/* Title state machine. The 60 Hz item rules are in tr2_menu_motion.h. */
#include "tr2_menu_audio.h"
static int intro_active,intro_skip;
static unsigned pad_previous;
#include "new_game_options.h"
static int movie_play(const char *relative,int test);
enum NewGamePage { PASSPORT, CAMPAIGN, MODE, GAMEPLAY, STARTING_LEVEL, ENHANCEMENTS };
static enum NewGamePage new_game_page;
enum { CHEATS_PAGE=10, CLOSE_FOR_SKIP=8 };
static int gameplay_row,enhancement_row;
static unsigned selected_options,launch_options;
static unsigned level_count;
static char level_names[256][128],level_files[256][128];
static char item_labels[256][10][96],cut_files[32][128],game_strings[96][128],pc_strings[41][128];
enum FlowCommand { FLOW_MOVIE=3, FLOW_CUT=5, FLOW_RESULTS=6, FLOW_END=9, FLOW_TRACK=10, FLOW_FINALE=15, FLOW_ANGLE=16 };
/* Script byte offsets are 16-bit. Keep the validated continuation after each START_LEVEL. */
static uint16_t flow_words[32768],flow_after_level[256];
static unsigned flow_cursor;
static int flow_track,flow_angle,flow_advance;
static int credits_present(void);
static int rewards[256][27];
static int level_movie[256],level_music[256],level_available[256],entry_flags[256],entry_sunset[256],entry_item[256],entry_extra[256],launch_campaign_home,selected_level,launch_level,movie_test;
static char flow_movies[32][128];
static unsigned intro_movies[32],intro_count,title_track;
static const char *ring_name(int index){
 if(home_session && game_ring==OPTION_RING && ring_id(index)==122)return "Customisation";
 if(!home_session || game_ring==OPTION_RING)return title_names[index];
 int id=ring_pickup(index);
 if(id==121)return game_strings[35];if(id==151)return "Flares";
 if(id>=135 && id<=150){static const int names[]={36,37,38,39,40,41,42,44,45,46,47,48,49,50,51,52};return game_strings[names[id-135]];}
 int slot=id>=193 && id<=196?id-187:id>=205?id-201:id-174;
 return item_labels[current_level][slot];
}
static int item_quantity(int id){
 const Tr2World *w=tr2_level_world();int weapon=tr2_inventory_weapon(id),count=w->inventory->count[id];
 if(weapon>1)return w->combat->ammo[weapon]/(id==136?6:1);
 if(id>=143 && id<=147)return count*2;
 return id==151 || id==148 || count>1?count:-1;
}
enum CampaignScreen { PLAY, LOAD_SLOTS, SAVE_SLOTS, RESULTS, HOME_RESULTS, FINAL_RESULTS, CUTSCENE };
static enum CampaignScreen campaign_screen;
#include "save_slots.h"
static int slots_available,remembered_slot;
static int slot_levels[TOMB_MANUAL_SLOTS+1],slot_cursor,slot_error;
static void slot_path(char path[MAX_PATH],int slot){
 char folder[MAX_PATH];if(movie_test)snprintf(folder,sizeof folder,"build/tr2-slot-test");else snprintf(folder,sizeof folder,"%s/SAVES",asset_root);
 if(slot==TOMB_MANUAL_SLOTS){snprintf(path,MAX_PATH,"%s/autosave.sav",folder);return;}
 tomb_manual_slot_path(path,MAX_PATH,folder,slot,"sav");
}
static int autosave_visible(void){
 return !home_session || (tr2_level_world()->level->custom.options&TR2_AUTOSAVE);
}
static int write_slot(int slot){
 char path[MAX_PATH],folder[MAX_PATH];slot_path(path,slot);
 snprintf(folder,sizeof folder,"%s",path);*strrchr(folder,'/')=0;CreateDirectoryA(folder,NULL);
 return tr2_level_save(path,current_level);
}
static void write_autosave(void){
 if(!write_slot(TOMB_MANUAL_SLOTS))fprintf(stderr,"TR2B could not write autosave\n");
}
static int scan_slots(void){
 int first=-1;for(int i=0;i<=TOMB_MANUAL_SLOTS;i++){char path[MAX_PATH];slot_path(path,i);slot_levels[i]=tr2_slot_level(path);if(slot_levels[i]>=(int)level_count)slot_levels[i]=-1;if(slot_levels[i]>=0 && (i==TOMB_MANUAL_SLOTS?autosave_visible():first<0))first=i;}
 slots_available=first>=0;return first;
}
static void open_slots(int saving){
 int first=scan_slots();if(!saving && first<0)return;
 campaign_screen=saving?SAVE_SLOTS:LOAD_SLOTS;slot_cursor=saving?(remembered_slot<TOMB_MANUAL_SLOTS?remembered_slot:0):(slot_levels[remembered_slot]>=0 && (remembered_slot<TOMB_MANUAL_SLOTS || autosave_visible())?remembered_slot:first);slot_error=0;
}
static void show_statistics(void){
 const Tr2World *w=tr2_level_world();Tr2Inventory saved;Tr2Combat combat;const Tr2Inventory *v=w->inventory;const Tr2Combat *g=w->combat;
 if(campaign_screen==RESULTS){tr2_level_completed_stats(&saved,&combat);v=&saved;g=&combat;}
 int final=campaign_screen==FINAL_RESULTS;
 Tr2CampaignTotals totals=final?tr2_campaign_totals():(Tr2CampaignTotals){
  .ticks=v->ticks,.distance=v->distance,.kills=v->kills,.shots=g->shots,.hits=g->hits,.medipacks=v->medipacks
 };
 char values[7][64];unsigned ticks=totals.ticks,metres=totals.distance/445;
 snprintf(values[0],sizeof values[0],"%02u:%02u:%02u",ticks/108000,ticks/1800%60,ticks/30%60);
 if(final)snprintf(values[1],sizeof values[1],"%u %.40s %u",totals.secrets,game_strings[76],3*(level_count-1));
 else if(v->secrets&7){
  char *out=values[1];
  for(int i=0;i<3;i++)if(v->secrets&(1<<i))*out++=(char)(127+i);else{memset(out,' ',3);out+=3;}
  *out=0;
 }else snprintf(values[1],sizeof values[1],"%.63s",game_strings[69]);
 snprintf(values[2],sizeof values[2],"%u",totals.kills);
 snprintf(values[3],sizeof values[3],"%u",totals.shots);
 snprintf(values[4],sizeof values[4],"%u",totals.hits);
 snprintf(values[5],sizeof values[5],"%u.%u",totals.medipacks/2,totals.medipacks&1?5:0);
 if(metres<1000)snprintf(values[6],sizeof values[6],"%um",metres);
 else snprintf(values[6],sizeof values[6],"%u.%02ukm",metres/1000,metres%1000/10);
 /* 426500 uses the same 304-wide, seven-row requester as Best Times. */
 menu_frame(title_level,title_textures,168,268,304,180,1);
 menu_frame(title_level,title_textures,170,270,300,24,1);
 title_text(284,campaign_screen==FINAL_RESULTS?game_strings[75]:level_names[current_level],0);
 static const int labels[]={59,60,62,63,64,67,66};
 for(int row=0;row<7;row++){
  int y=312+row*18;menu_text_at(176,y,game_strings[labels[row]],0,0);
  menu_text_at(464,y,values[row],0,-1);
 }
}
static void open_level_results(void){
 campaign_screen=RESULTS;home_playing=0;menu_reset();
 /* 41fa60 sets the completion cue; 44c827 plays it once. */
 tr2_audio_cut_music(41);
}
static void show_best_times(void){
  /* 426320 / requester 4257a0: seven 18-pixel rows, width 304,
     bottom anchor -32. Original game strings 70, 71, 72. */
  const Tr2AssaultScores *scores=tr2_home_scores();int y=312;
  menu_frame(level,textures,168,268,304,180,1);menu_frame(level,textures,170,270,300,24,1);
  title_text(284,"BEST TIMES",0);
  if(!scores->times[0])title_text((float)y,"No Times Set",0);
  for(int i=title_choice;i<10 && i<title_choice+7 && scores->times[i];i++,y+=18){
   char label[64],time[32];unsigned ticks=scores->times[i];
   snprintf(label,sizeof label,"%2d: Finish %u",i+1,scores->finishes[i]);
   snprintf(time,sizeof time,"%02u:%02u.%-2u",ticks/1800,(ticks/30)%60,(ticks%30)/3);
   menu_text_at(176,(float)y,label,0,0);menu_text_at(464,(float)y,time,0,-1);
  }
}
static int slot_row(int slot,int with_auto){
 return with_auto?(slot==TOMB_MANUAL_SLOTS?0:slot+1):slot;
}
static int row_slot(int row,int with_auto){
 return with_auto?(row?row-1:TOMB_MANUAL_SLOTS):row;
}
static void campaign_overlay(void){
 if(campaign_screen==HOME_RESULTS){show_best_times();return;}
 if(campaign_screen==RESULTS || campaign_screen==FINAL_RESULTS){show_statistics();return;}
 int saving=campaign_screen==SAVE_SLOTS;
 int rows=10,top=135;
 menu_frame(level,textures,155,top,330,40+18*rows,1);
 title_text(top+18,game_strings[saving?5:4],0);
 int with_auto=!saving && autosave_visible(),row=slot_row(slot_cursor,with_auto);
 int first=row>=10?row-9:0;
 for(int i=0;i<rows;i++){
  int slot=row_slot(first+i,with_auto),selected=slot==slot_cursor;
  int available=saving || slot_levels[slot]>=0;

  if(selected)menu_frame(level,textures,162,top+25+i*18,316,17,1);
  const char *name=slot_levels[slot]>=0?level_names[slot_levels[slot]]:pc_strings[15];char label[180];
  if(slot==TOMB_MANUAL_SLOTS){snprintf(label,sizeof label,"Autosave - %s",name);name=label;}
  title_text(top+39+i*18,name,available?0:-1);
 }
 if(slot_error)title_text(386,pc_strings[saving?7:13],0);
}
static int campaign_input(int key){
 if(campaign_screen==PLAY)return 0;
 int confirm=key==VK_RETURN || control_matches(8,key),back=key==VK_ESCAPE || key==VK_DECIMAL;
 int step=control_matches(0,key)?-1:control_matches(1,key)?1:0;
 if(campaign_screen==CUTSCENE || campaign_screen==FINAL_RESULTS || campaign_screen==RESULTS){if(back || confirm)pending_transition=6;return 1;}
 if(campaign_screen==HOME_RESULTS){
  if(back || confirm){launch_level=1;pending_transition=1;}
  else if(step && (step<0 || (title_choice+7<10 && tr2_home_scores()->times[title_choice+7])))title_choice=clamp(title_choice+step,0,3);
  return 1;
 }
 if(death_passport){
  if(back)return 1;
  if(control_matches(3,key)){campaign_screen=PLAY;title_choice=2;menu_sound(115);return 1;}
 }
 if(back){campaign_screen=PLAY;return 1;}
 if(step){
  int with_auto=campaign_screen==LOAD_SLOTS && autosave_visible();
  for(int row=slot_row(slot_cursor,with_auto)+step;row>=0 && row<TOMB_MANUAL_SLOTS+with_auto;row+=step){
   int slot=row_slot(row,with_auto);
   if(campaign_screen==SAVE_SLOTS || slot_levels[slot]>=0){slot_cursor=slot;break;}
  }
 }
 if(confirm){
  remembered_slot=slot_cursor;
  if(campaign_screen==LOAD_SLOTS){
   if(death_passport){campaign_screen=PLAY;title_page=return_frame=0;return_item=1;ring_exit=7;menu_sound(111);}
   else pending_transition=4;
   return 1;
  }
  if(write_slot(slot_cursor)){campaign_screen=PLAY;title_page=0;return_frame=0;return_item=1;ring_exit=4;scan_slots();}else slot_error=1;
 }
 return 1;
}
static void filtering_apply(void){
 if(title_level)for(uint32_t i=0;i<title_level->tile_count;i++)tr2_texture_filter(title_textures[i],title_filtering);
 tr2_home_texture_filtering(title_filtering);
}
static void settings_save(void){
 if(!config_path[0])return;
 char value[32],name[32];snprintf(value,sizeof value,"%d",title_music);WritePrivateProfileStringA("Sound","Music",value,config_path);
 snprintf(value,sizeof value,"%d",title_effects);WritePrivateProfileStringA("Sound","Effects",value,config_path);
 snprintf(value,sizeof value,"%d",title_framerate);WritePrivateProfileStringA("Display","FramerateLimit",value,config_path);
 snprintf(value,sizeof value,"%d",title_resolution);WritePrivateProfileStringA("Display","Resolution",value,config_path);
 snprintf(value,sizeof value,"%d",title_force43);WritePrivateProfileStringA("Display","Force43",value,config_path);
 snprintf(value,sizeof value,"%d",title_draw_distance);WritePrivateProfileStringA("Display","DrawDistance",value,config_path);
 snprintf(value,sizeof value,"%d",title_water_style);WritePrivateProfileStringA("Display","WaterStyle",value,config_path);
 snprintf(value,sizeof value,"%d",title_filtering);WritePrivateProfileStringA("Display","TextureFiltering",value,config_path);
 snprintf(value,sizeof value,"%d",control_layout);WritePrivateProfileStringA("Controls","Layout",value,config_path);
 for(int i=0;i<14;i++){snprintf(name,sizeof name,"Key%d",i);snprintf(value,sizeof value,"%d",control_keys[i]);WritePrivateProfileStringA("Controls",name,value,config_path);}
}
static void settings_load(void){
 title_music=clamp(GetPrivateProfileIntA("Sound","Music",10,config_path),0,10);
 title_effects=clamp(GetPrivateProfileIntA("Sound","Effects",10,config_path),0,10);
 title_framerate=GetPrivateProfileIntA("Display","FramerateLimit",0,config_path);if(title_framerate<0 || title_framerate>2)title_framerate=0;
 title_resolution=GetPrivateProfileIntA("Display","Resolution",0,config_path);if(title_resolution<0 || title_resolution>3)title_resolution=0;
 tr2_home_resolution(title_resolution);
 title_force43=GetPrivateProfileIntA("Display","Force43",0,config_path);if(title_force43<0 || title_force43>1)title_force43=0;
 tr2_home_force43(title_force43);
 title_filtering=GetPrivateProfileIntA("Display","TextureFiltering",1,config_path);if(title_filtering<0 || title_filtering>1)title_filtering=1;
 filtering_apply();
 title_water_style=GetPrivateProfileIntA("Display","WaterStyle",0,config_path);if(title_water_style<0 || title_water_style>1)title_water_style=0;
 tr2_home_water_style(title_water_style);
 title_draw_distance=GetPrivateProfileIntA("Display","DrawDistance",0,config_path);if(title_draw_distance<0 || title_draw_distance>=DRAW_DISTANCE_COUNT)title_draw_distance=0;
 tr2_home_draw_distance(title_draw_distance);
 control_layout=clamp(GetPrivateProfileIntA("Controls","Layout",0,config_path),0,1);
 int valid=1;for(int i=0;i<14;i++){char name[32];snprintf(name,sizeof name,"Key%d",i);control_keys[i]=GetPrivateProfileIntA("Controls",name,control_alternate[i],config_path);if(!control_valid(control_keys[i]))valid=0;for(int j=0;j<i;j++)if(control_keys[j]==control_keys[i])valid=0;}
 if(!valid)memcpy(control_keys,control_alternate,sizeof control_keys);
}
static int read_labels(const unsigned char *p,size_t size,size_t *pos,unsigned count,int cypher,char *out,size_t stride,size_t limit){
 if(count>1024 || *pos+count*2+2>size)return 0;const unsigned char *offset=p+*pos;*pos+=count*2;
 unsigned bytes=tr2_u16(p+*pos);*pos+=2;if(bytes>size-*pos)return 0;
 for(unsigned i=0;i<count;i++){unsigned at=tr2_u16(offset+2*i),j=0;if(at>=bytes)return 0;
  while(at+j<bytes && (p[*pos+at+j]^cypher)){if(j+1>=limit)return 0;out[i*stride+j]=(char)(p[*pos+at+j]^cypher);j++;}
  if(at+j==bytes)return 0;out[i*stride+j]=0;
 }*pos+=bytes;return 1;
}
static int flow_read(void){
 size_t size;unsigned char *p=asset_read("DATA/TOMBPC.DAT",&size);if(!p)return 0;
 int ok=0;intro_count=0;memset(flow_movies,0,sizeof flow_movies);
 if(size<390 || tr2_u32(p)!=3 || tr2_u16(p+260)!=128)goto done;
 const unsigned char *h=p+262;unsigned levels=tr2_u16(h+64),movies=tr2_u16(h+70);size_t pos=390;
 unsigned demos=tr2_u16(h+74);
 if(levels<3 || tr2_u16(h+72)>32 || levels>256 || demos>levels-2 || movies>32)goto done;
 level_count=levels-demos;title_track=tr2_u16(h+76);tr2_audio_set_secret_track(h[122]);
 unsigned counts[]={levels,tr2_u16(h+66),tr2_u16(h+68),movies,levels,tr2_u16(h+72)};
 for(int table=0;table<6;table++){
  unsigned n=counts[table];if(n>1024 || pos+n*2+2>size)goto done;const unsigned char *offsets=p+pos;pos+=n*2;unsigned bytes=tr2_u16(p+pos);pos+=2;if(bytes>size-pos)goto done;
  for(unsigned i=0;i<n;i++){unsigned off=tr2_u16(offsets+i*2);if(off>=bytes)goto done;
   if(table==0 || table==3 || table==4 || table==5){
    char *dest=table==0?level_names[i]:table==3?flow_movies[i]:table==5?cut_files[i]:level_files[i];unsigned j=0;
    for(;j<127 && off+j<bytes;j++){unsigned char c=p[pos+off+j]^h[120];if(!c)break;dest[j]=c=='\\'?'/':(char)c;}
    if(j==127 || off+j==bytes)goto done;dest[j]=0;
    if(table!=0 && (strstr(dest,"..") || strchr(dest,':') || dest[0]=='/'))goto done;
    if(table==3){char *ext=strrchr(dest,'.');if(strncmp(dest,"FMV/",4) || !ext || strlen(ext)!=4)goto done;strcpy(ext,".bmv");}
   }
  }pos+=bytes;
 }
 if(pos+(levels+1)*2+2>size)goto done;
 const unsigned char *scripts=p+pos;unsigned start=tr2_u16(p+pos),stop=tr2_u16(p+pos+2);pos+=(levels+1)*2;unsigned bytes=tr2_u16(p+pos);pos+=2;
 if(bytes>size-pos || start>stop || stop>bytes || ((start|stop)&1))goto done;
 if(bytes&1)goto done;
 for(unsigned i=0;i<bytes/2;i++)flow_words[i]=tr2_u16(p+pos+2*i);
 memset(rewards,0,sizeof rewards);
 for(unsigned level_id=0;level_id<level_count;level_id++){
  unsigned at=tr2_u16(scripts+2*(level_id+1)),end=level_id+2<=levels?tr2_u16(scripts+2*(level_id+2)):bytes;
  if(at>end || end>bytes || ((at|end)&1))goto done;
  level_movie[level_id]=-1;level_music[level_id]=0;int found=0;
  entry_sunset[level_id]=entry_flags[level_id]=entry_item[level_id]=entry_extra[level_id]=0;
  while(end-at>=2){
   unsigned op=tr2_u16(p+pos+at);at+=2;
   if(op==14 || op==22){entry_flags[level_id]|=op==14?1:2;continue;}
   if(op==11){entry_sunset[level_id]=1;continue;}
   if(op==13 || op==21)continue;
   if(end-at<2)goto done;
   unsigned value=tr2_u16(p+pos+at);at+=2;
   if(op==4){found=value==(unsigned)level_id;break;}
   if(op==3){if(value>=movies || level_movie[level_id]>=0)goto done;level_movie[level_id]=(int)value;}
   else if(op==10)level_music[level_id]=(int)value;
   else if(op==18 && value<27)rewards[level_id][value]++;
   else if(op==18 && value>=1019 && value<=1026)entry_item[level_id]=value<1023?174+(int)value-1019:193+(int)value-1023;
   else if(op==19)entry_extra[level_id]=value;
   else if(op!=17 && op!=20)goto done;
  }
  if(!found)goto done;
  flow_after_level[level_id]=(uint16_t)(at/2);
  int ended=0;
  while(at<end){
   unsigned op=flow_words[at/2];at+=2;
   if(op==FLOW_END){ended=at==end;break;}
   if(op==FLOW_RESULTS || op==FLOW_FINALE)continue;
   if(end-at<2)goto done;
   unsigned value=flow_words[at/2];at+=2;
   if(op==FLOW_CUT){if(value>=counts[5])goto done;}
   else if(op==FLOW_MOVIE){if(value>=movies)goto done;}
   else if(op!=FLOW_TRACK && op!=FLOW_ANGLE)goto done;
  }
  if(!ended)goto done;
 }
 /* Original frontend sequence: opcode 3 plays an FMV; opcode 9 ends. */
 for(unsigned i=start;i<stop;i+=2){unsigned op=tr2_u16(p+pos+i);if(op==9){ok=1;break;}if(op!=3 || i+4>stop || intro_count==32)goto done;i+=2;unsigned movie=tr2_u16(p+pos+i);if(movie>=movies)goto done;intro_movies[intro_count++]=movie;}
 if(ok){
  pos+=bytes+demos*2;if(pos+2>size)goto done_bad;unsigned strings=tr2_u16(p+pos);pos+=2;
  if(strings<77 || strings>96 || !read_labels(p,size,&pos,strings,h[120],game_strings[0],128,128) || !read_labels(p,size,&pos,41,h[120],pc_strings[0],128,128))goto done_bad;
  for(int slot=0;slot<10;slot++)if(!read_labels(p,size,&pos,levels,h[120],item_labels[0][slot],sizeof item_labels[0],96))goto done_bad;
 }
done:free(p);return ok;
done_bad:free(p);return 0;
}
static int title_init(void){
 if(title_level)return 1;char path[MAX_PATH],error[256];asset_path(path,"DATA/TITLE.TR2");
 level=title_level=tr2_load(path,error,sizeof error);if(!level){fprintf(stderr,"TR2: %s\n",error);return 0;}
 init_geometry();memcpy(title_textures,textures,sizeof textures);
 if(!title_picture())return 0;return 1;
}
static void title_close(void){
 tomb_render_surface_free(&title_surface);
 tomb_render_surface_free(&title_aspect_surface);
 if(title_level){glDeleteTextures((GLsizei)title_level->tile_count,title_textures);tr2_free(title_level);}
 if(title_background)glDeleteTextures(1,&title_background);
 title_background=0;level=title_level=NULL;memset(textures,0,sizeof textures);memset(title_textures,0,sizeof title_textures);
}
static int page_frame(void){return ring_id(title_selection)==133?14+5*title_choice:menu_descriptor(ring_id(title_selection))->open;}
static int page_ready(void){const RingItem *i=ring_items+ring_id(title_selection);return i->lift==16 && i->yaw==menu_descriptor(ring_id(title_selection))->yaw && i->frame==page_frame();}
static int item_busy(void){return ring_switch || turn_ticks || turning || return_item || ring_exit || ring_open<1 || (title_page && !page_ready());}
static int level_enabled(int index){return index==0?home_available:level_available[index];}
static int default_level(void){
 if(level_available[1])return 1;
 for(int i=0;i<(int)level_count;i++)if(level_enabled(i))return i;
 return -1;
}
static void new_game_input(int back,int up,int down,int sideways,int confirm){
 if(back){
  if(new_game_page==ENHANCEMENTS)new_game_page=GAMEPLAY;
  else if(new_game_page==STARTING_LEVEL && gameplay_row)new_game_page=ENHANCEMENTS;
  else new_game_page--;menu_sound(112);return;}
 if(new_game_page==GAMEPLAY){
  if(up || down)gameplay_row=clamp(gameplay_row+down-up,0,1);
  if(confirm){new_game_page=gameplay_row?ENHANCEMENTS:STARTING_LEVEL;menu_sound(111);}
 }else if(new_game_page==ENHANCEMENTS){
  enhancement_row=clamp(enhancement_row+down-up,0,2);
  if(enhancement_row==2 && confirm){new_game_page=STARTING_LEVEL;menu_sound(111);}
  else if(enhancement_row<2 && (sideways || confirm)){selected_options^=enhancement_row?TR2_CONSISTENT_WEAPONS:TR2_AUTOSAVE;menu_sound(108);}
 }else if(new_game_page==STARTING_LEVEL){
  int step=down-up;
  if(step)for(int i=selected_level+step;i>=0 && i<(int)level_count;i+=step){
   if(level_enabled(i)){selected_level=i;menu_sound(108);break;}
  }
  if(confirm && level_enabled(selected_level)){
   launch_options=gameplay_row?selected_options:0;launch_level=selected_level;launch_campaign_home=selected_level==0;new_game_page=PASSPORT;title_page=0;return_frame=0;
   return_item=1;ring_exit=3;ring_held_key=0;menu_sound(111);
  }
 }else if(confirm){new_game_page++;menu_sound(111);}
}
static void new_game_draw(void){
 static const char *const campaigns[]={"Tomb Raider II","Golden Mask","Combined","Custom"};
 static const char *const headings[]={"","Select Campaign","Select Mode","Select Gameplay","Choose Starting Level","Enhancements"};
 int rows=new_game_page==ENHANCEMENTS?3:new_game_page==STARTING_LEVEL?(int)level_count:new_game_page==GAMEPLAY?2:4;
 int role=new_game_page==CAMPAIGN,visible=rows>20?20:rows;
 int top=(480-40-18*(visible+role))/2;
 menu_frame(level,textures,130,top,380,40+18*(visible+role),1);
 menu_frame(level,textures,132,top+2,376,20,1);title_text(top+18,headings[new_game_page],0);
 if(role)title_text(top+38,"Singleplayer",0);
 for(int row=0;row<visible;row++){
  int y=top+38+18*(row+role);
  int enabled=new_game_page==STARTING_LEVEL?level_enabled(row):new_game_page==GAMEPLAY || new_game_page==ENHANCEMENTS || row==0;
  int selected=new_game_page==STARTING_LEVEL?selected_level:new_game_page==GAMEPLAY?gameplay_row:new_game_page==ENHANCEMENTS?enhancement_row:0;
  const char *name=new_game_page==CAMPAIGN?campaigns[row]:new_game_page==MODE?new_game_modes[row]:new_game_page==GAMEPLAY?new_game_gameplay[row]:level_names[row];
  char option[48];
  if(new_game_page==ENHANCEMENTS){
   if(row==2)name="Start Game";
   else{snprintf(option,sizeof option,"%s: %s",row?"Consistent Weapons":"Autosave",selected_options&(row?TR2_CONSISTENT_WEAPONS:TR2_AUTOSAVE)?"On":"Off");name=option;}
  }
  if(row==selected && enabled)menu_frame(level,textures,140,y-14,360,16,1);
  title_text(y,name,enabled?0:-1);
 }
}
static void open_key_ring(int request){
 inventory_refresh();if(!owned_count[1])return;home_playing=0;tr2_audio_inventory(1);menu_reset();game_ring=KEY_RING;lock_ring=request;memset(home_keys,0,sizeof home_keys);
}
static void title_key(int key){
 if(intro_active){if(key==VK_ESCAPE || key==VK_RETURN || key==VK_SPACE)intro_skip=1;if(intro_active==2)for(int i=0;i<14;i++)if(control_matches(i,key))intro_skip=1;return;}
 if(campaign_input(key))return;
 if(home_playing){
  if(tr2_home_dead()){if(key==VK_ESCAPE || key==VK_RETURN)tr2_home_death_input();for(int i=0;i<14;i++)if(control_matches(i,key))tr2_home_death_input();return;}
  if(control_matches(13,key) || key==VK_ESCAPE){home_playing=0;tr2_audio_inventory(1);memset(home_keys,0,sizeof home_keys);menu_reset();scan_slots();tr2_audio_pause(0);menu_sound(111);}return;
 }
 if(item_busy())return;
 if(title_page==4 && control_capture>=0){if(key==VK_ESCAPE)control_capture=-1;else if(controls_bind(control_capture,key)){control_capture=-1;menu_sound(111);}return;}
 int left=control_matches(2,key),right=control_matches(3,key),up=control_matches(0,key),down=control_matches(1,key),confirm=key==VK_RETURN || control_matches(8,key);
 if(death_passport){
  if(title_choice==2 && left && slots_available){title_choice=0;menu_sound(115);}
  else if(title_choice==2 && confirm){title_page=0;return_frame=29;return_item=1;ring_exit=2;menu_sound(111);}
  return;
 }
 if(new_game_page){new_game_input(key==VK_ESCAPE || key==VK_DECIMAL,up,down,left || right,confirm);return;}
 if(key==VK_ESCAPE || key==VK_DECIMAL || (home_session && control_matches(13,key))){if(title_page){return_frame=title_page==1 && title_choice==2?29:0;title_page=0;return_item=1;ring_held_key=0;menu_sound(112);}else if(home_session){ring_exit=4;ring_held_key=0;}return;}
 if(!title_page){
  if(home_session && !lock_ring && (up || down)){
   int next=game_ring==MAIN_RING?(up?KEY_RING:OPTION_RING):((game_ring==KEY_RING && down) || (game_ring==OPTION_RING && up)?MAIN_RING:game_ring);
   if(next!=game_ring && (next!=KEY_RING || owned_count[1])){ring_memory[game_ring]=title_selection;ring_target=next;ring_switch=1;switch_pitch=up?1:-1;ring_held_key=0;menu_sound(112);return;}
  }
  if(left || right){if(ring_count()<2)return;title_selection=(title_selection+(right?ring_count()-1:1))%ring_count();turn_ticks=24;turning=1;ring_held_key=key;menu_sound(108);}
  else if(confirm && !title_disabled(ring_id(title_selection))){title_choice=0;title_page=ring_id(title_selection)==153?7:ring_id(title_selection)==121?8:home_session && game_ring!=OPTION_RING?9:title_selection==0?1:title_selection==1?4:title_selection==4?(home_session?CHEATS_PAGE:5):3;menu_sound(111);}
 }else if(title_page==1){
  if((left || up) && title_choice>0){title_choice--;menu_sound(115);}else if((right || down) && title_choice<2){title_choice++;menu_sound(115);}
  else if(confirm && title_choice==0)open_slots(0);
  else if(confirm && title_choice==1 && home_session && current_level>0)open_slots(1);
  else if(confirm && title_choice==1 && !home_session && default_level()>=0){selected_options=0;gameplay_row=enhancement_row=0;selected_level=default_level();new_game_page=CAMPAIGN;menu_sound(111);}
  else if(confirm && title_choice==2){title_page=0;return_frame=29;return_item=1;ring_exit=2;menu_sound(111);}
 }else if(title_page==8){
  if(confirm){title_page=0;return_item=1;menu_sound(112);}
  else {const Tr2AssaultScores *scores=tr2_home_scores();int count=0;while(count<10 && scores->times[count])count++;if(up && title_choice>0)title_choice--;if(down && title_choice+7<count)title_choice++;}
 }else if(title_page==CHEATS_PAGE){
  if(confirm){title_page=0;return_item=1;ring_exit=CLOSE_FOR_SKIP;ring_held_key=0;menu_sound(111);}
 }else if(title_page==5){
  if(confirm){launch_options=0;launch_level=0;launch_campaign_home=0;title_page=0;return_item=1;ring_exit=3;ring_held_key=0;menu_sound(111);}
 }else if(title_page==7){
  /* Unavailable rows never participate in navigation. */
  if(up || down){title_choice=(title_choice+(down?1:5))%6;menu_sound(108);}
  if(left || right){
   if(title_choice==5){title_water_style=!title_water_style;tr2_home_water_style(title_water_style);}
   else if(title_choice==4){title_draw_distance=(title_draw_distance+(right?1:DRAW_DISTANCE_COUNT-1))%DRAW_DISTANCE_COUNT;tr2_home_draw_distance(title_draw_distance);}
   else if(title_choice==3){title_filtering=!title_filtering;filtering_apply();}
   else if(title_choice==2)title_framerate=(title_framerate+(right?1:2))%3;
   else if(title_choice==1){title_force43=!title_force43;tr2_home_force43(title_force43);}
   else{title_resolution=(title_resolution+(right?1:3))%4;tr2_home_resolution(title_resolution);}
   settings_save();menu_sound(108);
  }
 }else if(title_page==3){
  if(up || down){title_choice=1-title_choice;menu_sound(108);}
  if(left || right){int *value=title_choice?&title_effects:&title_music;*value=clamp(*value+(right?1:-1),0,10);if(home_session)tr2_audio_volume(title_music,title_effects);settings_save();menu_sound(108);}
 }else if(title_page==4){
  if(up && title_choice>0)title_choice--;if(down && title_choice<13)title_choice++;
  if(left || right){control_layout=1-control_layout;settings_save();}
  if(confirm && control_layout)control_capture=title_choice;
 }
}
static void menu_tick(double dt){
 if(campaign_screen!=PLAY)return;
 ring_fraction+=dt*60;
 while(ring_fraction>=1 && running){ring_fraction-=1;
  if(ring_switch){
   ring_open=fmax(0,ring_open-1.0/24);
   if(ring_open<1e-8){ring_open=0;}
   if(ring_open==0){game_ring=ring_target;title_selection=ring_memory[game_ring];title_angle=title_selection*2*PI/ring_count();ring_switch=0;switch_pitch=-switch_pitch;}
   continue;
  }
  if(!return_item && (ring_exit==1 || ring_exit==4 || ring_exit==5 || ring_exit==6 || ring_exit==7 || ring_exit==CLOSE_FOR_SKIP)){
   ring_open=fmax(0,ring_open-1.0/32);if(ring_open<=0){
    if(ring_exit==1 && !home_session)running=0;
    else if(ring_exit==CLOSE_FOR_SKIP){tr2_level_skip();pending_transition=2;}
    else pending_transition=ring_exit==7?4:ring_exit==6?1:ring_exit==4?2:3;
    break;
   }continue;
  }
  ring_open=fmin(1,ring_open+1.0/(switch_pitch?24:32));
  if(ring_open>1-1e-8)ring_open=1;
  if(ring_open==1)switch_pitch=0;
  /* Death inventory (mode 5) selects the passport once the option ring opens. */
  if(death_passport && ring_open==1 && !title_page && !return_item && !ring_exit){
   title_page=1;title_choice=slots_available?0:2;menu_sound(111);
  }
  int count=ring_count();
  double delta=remainder(title_selection*2*PI/count-title_angle,2*PI);
  if(count>1 && !turn_ticks && ring_held_key && !title_page && !return_item && !ring_exit && ring_open==1){int direction=control_matches(2,ring_held_key)?1:-1;title_selection=(title_selection+direction+count)%count;delta=direction*2*PI/count;turn_ticks=24;menu_sound(108);}
  if(turn_ticks){turn_ticks--;title_angle+=turn_ticks?copysign((65536/count/24)*(2*PI/65536),delta):delta;}
  turning=turn_ticks!=0;
  for(int i=0;i<count;i++){int id=ring_id(i),selected=i==title_selection,inspect=selected && title_page;const MenuDescriptor *d=menu_descriptor(id);RingItem *item=ring_items+id;
   int frame=inspect?(id==133?page_frame():d->open):0;
   if(!inspect && id==133 && return_item){frame=return_frame;if(item->frame==frame){item->closed=1;item->frame=frame=return_frame=0;}}
   move_item(item,d,selected,inspect,frame,turning);
   if(death_passport && title_page==1 && !title_choice && page_ready()){open_slots(0);return;}
   if(title_page==9 && page_ready()){title_page=0;return_item=flare_pick=1;}
   if(selected && !inspect && !item->lift && item->frame==frame){return_item=0;if(flare_pick){int pickup=ring_pickup(title_selection);flare_pick=0;
    if(pickup<142 || pickup>148){tr2_level_use(pickup,lock_ring);ring_exit=4;}}if(ring_exit==2)ring_exit=home_session?5:1;else if(ring_exit==3)ring_exit=6;}
  }
 }
}
static void title_scene(int full_w,int full_h){
 glViewport(0,0,width,height);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 if(home_session)inventory_background();
 int vw=full_w,vh=full_w*3/4;if(vh>full_h){vh=full_h;vw=full_h*4/3;}
 vw=(int)((int64_t)vw*width/full_w);vh=(int)((int64_t)vh*height/full_h);glViewport((width-vw)/2,(height-vh)/2,vw,vh);
 if(!home_session){menu_flat();glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,title_background);glColor3f(1,1,1);
 glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2i(0,0);glTexCoord2f(1,0);glVertex2i(640,0);glTexCoord2f(1,1);glVertex2i(640,480);glTexCoord2f(0,1);glVertex2i(0,480);glEnd();}
 if(new_game_page || (campaign_screen!=PLAY && !(death_passport && campaign_screen==LOAD_SLOTS)))return;
 if(home_session && game_ring!=OPTION_RING)level=tr2_home_menu_assets(textures);
 menu_projection();for(int i=0;i<ring_count();i++)draw_ring_item(ring_id(i),i*2*PI/ring_count()-title_angle);
 level=title_level;memcpy(textures,title_textures,sizeof textures);
}
static void title_ui(void){
 int vw=width,vh=width*3/4;if(vh>height){vh=height;vw=height*4/3;}glViewport((width-vw)/2,(height-vh)/2,vw,vh);menu_flat();
 if(campaign_screen!=PLAY){campaign_overlay();glViewport(0,0,width,height);return;}
 if(new_game_page){new_game_draw();glViewport(0,0,width,height);return;}
 if(home_session && ring_open==1 && !turning && !return_item && !ring_exit && !title_page){
  if(game_ring!=OPTION_RING){int quantity=item_quantity(ring_pickup(title_selection));if(quantity>=0){char text[32];snprintf(text,sizeof text,"%d",quantity);menu_text_at(384,424,text,0,1);}}
  title_text(26,game_strings[game_ring==KEY_RING?2:game_ring==OPTION_RING?1:0],0);
  if(!lock_ring && (game_ring==OPTION_RING || (game_ring==MAIN_RING && owned_count[1]))){menu_text_at(20,28,"[",0,0);menu_text_at(620,28,"[",0,-1);}
  if(!lock_ring && game_ring!=OPTION_RING){menu_text_at(20,465,"]",0,0);menu_text_at(620,465,"]",0,-1);}
 }
 /* The photograph retains its caption while lifting and while inspected.
    Confirmation clears title_page as the item returns and the ring closes. */
 if(title_page==5 && !ring_exit){title_text(464,ring_name(title_selection),0);glViewport(0,0,width,height);return;}
 if(title_page && !page_ready())return;
 if(!title_page){if(ring_open==1 && !turning && !return_item && !ring_exit)title_text(464,ring_name(title_selection),title_disabled(ring_id(title_selection))?-1:0);}
 else if(title_page==1)title_text(464,title_choice==0?"Load Game":title_choice==1?(home_session?"Save Game":"New Game"):(home_session?"Exit to Title":"Exit Game"),(title_choice==0 && !slots_available) || (title_choice==1 && (home_session?!current_level:(default_level()<0)))?-1:0);
 else if(title_page==CHEATS_PAGE){
  menu_frame(level,textures,130,204,380,60,1);title_text(222,"Cheats",0);
  menu_frame(level,textures,140,235,360,16,1);title_text(249,"Skip Level",0);
 }
 else if(title_page==8 && current_level){show_statistics();}
 else if(title_page==8){
  show_best_times();
 }else if(title_page==3){
  char text[32];menu_frame(level,textures,248,194,144,85,1);menu_frame(level,textures,250,196,140,16,1);title_text(210,"Sound",0);
  menu_frame(level,textures,254,226+25*title_choice,132,16,1);snprintf(text,sizeof text,"| %2d",title_music);title_text(240,text,0);snprintf(text,sizeof text,"} %2d",title_effects);title_text(265,text,0);
 }else if(title_page==7){
  /* Other rows remain disabled placeholders, without values. */
  /* TR2B has no Colour Depth option; keep TR1B's shared label indices. */
  static const int settings[]={0,1,2,3,4,6,7,8,9,10,11};
  int rows=home_session?7:11,first=home_session?1:0,top=home_session?100:42;
  menu_frame(level,textures,90,(float)top,460,(float)(42+26*rows),1);
  menu_frame(level,textures,92,(float)(top+2),456,22,1);
  title_text((float)(top+18),"Detail Levels",0);
  for(int row=0;row<rows;row++){
   int setting=settings[first+row];float y=(float)(top+46+26*row);
   if(setting==2 || setting==3 || setting==4 || setting==6 || setting==7 || setting==8){static const int available[]={2,3,4,6,7,8};char line[96];if(setting==available[title_choice])menu_frame(level,textures,96,y-14,448,20,1);const char *value=setting==7?draw_distance_names[title_draw_distance]:setting==8?(title_water_style?"PS1":"PC"):setting==2?resolution_labels[title_resolution]:setting==3?(title_force43?"On":"Off"):setting==6?(title_filtering?"On":"Off"):title_framerate==0?"Off":title_framerate==1?"60":"30";snprintf(line,sizeof line,"%s: %s",detail_option_labels[setting],value);menu_text_at(110,y,line,0,0);}
   else menu_text_at(110,y,detail_option_labels[setting],-1,0);
  }
 }else if(title_page==4){
  title_text(190,control_layout?"User Keys":"Default Keys",0);
  for(int i=0;i<14;i++){char name[48];control_key_name(control_layout?control_keys[i]:control_defaults[i],name);float y=215+(i%7)*15;
   menu_text_at(i<7?180:400,y,name,control_capture==i?1:0,-1);menu_text_at(i<7?190:410,y,control_names[i],control_layout && title_choice==i,0);}
 }
 glViewport(0,0,width,height);
}
static void title_render(void){
 int window_w=width,window_h=height;
 if(width<1 || height<1)return;
 if(title_force43)tomb_render_fit43(&width,&height);
 int full_w=width,full_h=height;
 tr2_resolution_extent(title_resolution,full_w,full_h,&width,&height);
 title_scene(full_w,full_h);
 if(width!=full_w || height!=full_h)tomb_render_enlarge(&title_surface,width,height,full_w,full_h);
 width=full_w;height=full_h;title_ui();glViewport(0,0,width,height);
 tomb_render_center(&title_aspect_surface,width,height,window_w,window_h);
 width=window_w;height=window_h;
}
static void selector_draw(int current,unsigned available,int w,int h){
 const char *names[]={"Tomb Raider","Tomb Raider II","Tomb Raider III","Tomb Raider: The Last Revelation","Tomb Raider Chronicles"};
 glViewport(0,0,w,h);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);menu_flat();glDisable(GL_BLEND);
 int vw=w,vh=w*3/4;if(vh>h){vh=h;vw=h*4/3;}glViewport((w-vw)/2,(h-vh)/2,vw,vh);
 menu_frame(level,textures,110,175,420,142,1);title_text(194,"Select Game",0);
 for(int i=0;i<5;i++)title_text((float)(219+i*18),names[i],available&(1u<<i)?i==current:-1);
}
void tr2_menu_root(const char *root){snprintf(asset_root,sizeof asset_root,"%s/Source/TR2/PC",root);snprintf(config_path,sizeof config_path,"%s/Config/TR2/settings.ini",root);}
int tr2_menu_available(void){
 const char *files[]={"DATA/TITLE.TR2","DATA/TITLE.PCX","DATA/LEGAL.PCX","DATA/TOMBPC.DAT","DATA/MAIN.SFX","AUDIO/track60.wav","FMV/LOGO.bmv","FMV/ANCIENT.bmv"};
 for(unsigned i=0;i<sizeof files/sizeof *files;i++){FILE *f=asset_open(files[i]);if(!f)return 0;fclose(f);}return 1;
}
int tr2_menu_style(TombLauncherStyle *style){if(!title_init()){title_close();return 0;}style->context=NULL;style->draw_selector=selector_draw;style->vr_menu_threshold=.672;return 1;}
void tr2_menu_style_free(TombLauncherStyle *style){title_close();memset(style,0,sizeof *style);}
static LRESULT CALLBACK title_window(HWND w,UINT msg,WPARAM key,LPARAM info){
 if(msg==WM_CLOSE){running=0;return 0;}if(msg==WM_SETCURSOR && LOWORD(info)==HTCLIENT){SetCursor(NULL);return TRUE;}
 if(msg==WM_ERASEBKGND)return 1;
 if(msg==WM_PAINT){PAINTSTRUCT p;BeginPaint(w,&p);EndPaint(w,&p);return 0;}
 if(msg==WM_KILLFOCUS){memset(home_keys,0,sizeof home_keys);ring_held_key=0;pad_previous=~0u;return 0;}
 if(msg==WM_SYSKEYDOWN && key==VK_F4){running=0;return 0;}
 if(msg==WM_SYSKEYDOWN && key==VK_RETURN && (info&(1L<<29))){if(!(info&(1L<<30)))host->fullscreen(w);return 0;}
 if(msg==WM_KEYDOWN || msg==WM_SYSKEYDOWN){key=(WPARAM)tr2_message_key((int)key,(uint32_t)info);if(key<256)home_keys[key]=1;if(!(info&(1L<<30)))title_key((int)key);return 0;}
 if(msg==WM_KEYUP || msg==WM_SYSKEYUP){key=(WPARAM)tr2_message_key((int)key,(uint32_t)info);if(key<256)home_keys[key]=0;if((int)key==ring_held_key)ring_held_key=0;return 0;}
 return DefWindowProcA(w,msg,key,info);
}
static int pump(void){
 MSG msg;while(PeekMessageA(&msg,NULL,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)running=0;TranslateMessage(&msg);DispatchMessageA(&msg);}
 RECT r;GetClientRect(host->window,&r);width=r.right>0?r.right:1;height=r.bottom>0?r.bottom:1;
 unsigned buttons=host->buttons?host->buttons():0;unsigned pressed=buttons&~pad_previous;pad_previous=buttons;
 if(GetActiveWindow()!=host->window || IsIconic(host->window))return 0;
 const unsigned bits[]={16,32,1,2,4,8};const int keys[]={control_keys[0],control_keys[1],control_keys[2],control_keys[3],VK_RETURN,VK_ESCAPE};
 if(home_playing && campaign_screen==PLAY){if(pressed&8)title_key(VK_ESCAPE);return 1;}
 for(int i=0;i<6;i++)if(pressed&bits[i]){title_key(keys[i]);ring_held_key=0;}return 1;
}
static int capture_image(const char *path){
 unsigned char *p=malloc((size_t)width*height*3);if(!p)return 0;glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,p);
 FILE *f=fopen(path,"wb");if(!f){free(p);return 0;}fprintf(f,"P6\n%d %d\n255\n",width,height);
 for(int y=height-1;y>=0;y--)fwrite(p+(size_t)y*width*3,3,(size_t)width,f);int ok=!ferror(f);fclose(f);free(p);return ok;
}

static uint32_t home_input(void){
 const uint32_t bits[]={TR2_FORWARD,TR2_BACK,TR2_LEFT,TR2_RIGHT,TR2_STEP_LEFT,TR2_STEP_RIGHT,TR2_WALK,TR2_JUMP,TR2_ACTION,32,0x80000,TR2_LOOK,TR2_ROLL,tr2_home_dead()?256u:0u};
 uint32_t input=0;for(int action=0;action<14;action++)for(int key=0;key<256;key++)if(home_keys[key] && control_matches(action,key)){input|=bits[action];break;}
 /* Shared launcher pad provides the same held directional and action buttons. */
 if(pad_previous&16)input|=TR2_FORWARD;if(pad_previous&32)input|=TR2_BACK;
 if(pad_previous&1)input|=TR2_LEFT;if(pad_previous&2)input|=TR2_RIGHT;if(pad_previous&4)input|=TR2_ACTION;
 if(tr2_home_dead() && (home_keys[VK_ESCAPE] || home_keys[VK_RETURN]))input|=256;
 return input;
}
static void begin_level_flow(int index){
 tr2_level_sunset(entry_sunset[index]);
 death_passport=0;current_level=index;flow_cursor=flow_after_level[index];flow_track=level_music[index];flow_angle=0x4000;flow_advance=0;
 campaign_screen=PLAY;home_session=home_playing=1;ring_exit=0;
 tr2_audio_volume(title_music,title_effects);tr2_audio_level_music(level_music[index]);
}
static int start_campaign_level(int index,int carry,int campaign_home){
 int movie=level_movie[index];
 if(movie>=0){
  tr2_audio_stop_music();
  if(!movie_play(flow_movies[movie],movie_test))return 0;
  if(!running)return 1;
 }
 memset(home_keys,0,sizeof home_keys);
 if(!tr2_level_start(asset_root,level_files[index],index,entry_flags[index],entry_item[index],entry_extra[index],campaign_home,carry,launch_options))return 0;
 tr2_level_rewards(rewards[index]);begin_level_flow(index);if(launch_options&TR2_AUTOSAVE)write_autosave();return 1;
}
static int return_to_title(void){
 tr2_home_close();home_session=home_playing=current_level=0;campaign_screen=PLAY;scan_slots();menu_reset();
 return audio_open() && audio_title((int)title_track);
}
/* Resume the same script after gameplay, a cinematic, or a statistics screen.
   COMPLETE selects the following campaign entry only when this script reaches END. */
static int resume_level_flow(void){
 while(running){
  switch(flow_words[flow_cursor++]){
   case FLOW_TRACK:flow_track=flow_words[flow_cursor++];break;
   case FLOW_ANGLE:flow_angle=flow_words[flow_cursor++];break;
   case FLOW_MOVIE:
    tr2_audio_stop_music();
    if(!movie_play(flow_movies[flow_words[flow_cursor++]],movie_test))return 0;
    break;
   case FLOW_CUT:{
    int cut=flow_words[flow_cursor++];tr2_level_cut_angle(flow_angle);
    if(!tr2_level_open(asset_root,cut_files[cut],-1))return 0;
    campaign_screen=CUTSCENE;home_playing=1;tr2_audio_volume(title_music,title_effects);tr2_audio_cut_music(flow_track);return 1;
   }
   case FLOW_RESULTS:flow_advance=1;open_level_results();return 1;
   case FLOW_FINALE:
    flow_advance=0;tr2_audio_cut_music(52);
    if(!credits_present())return 0;
    if(running){home_playing=0;campaign_screen=FINAL_RESULTS;menu_reset();}return 1;
   case FLOW_END:
    if(flow_advance && current_level+1<(int)level_count)return start_campaign_level(current_level+1,1,0);
    return return_to_title();
   default:return 0;
  }
 }
 return 1;
}
static int home_transition(void){
 int transition=pending_transition;pending_transition=0;if(!transition)return 1;
 memset(home_keys,0,sizeof home_keys);pad_previous=0;ring_held_key=0;
 if(transition==4){
  int index=slot_levels[slot_cursor];char path[MAX_PATH];slot_path(path,slot_cursor);
  if(index<0 || index>=(int)level_count || !tr2_level_restore(asset_root,level_files[index],index,path)){slot_error=1;
   if(death_passport){ring_exit=0;ring_open=1;title_page=1;title_choice=0;campaign_screen=LOAD_SLOTS;}
   return 1;}
  launch_options=tr2_level_world()->level->custom.options;audio_close();begin_level_flow(index);return 1;
 }
 if(transition==5){tr2_level_remember();return resume_level_flow();}
 if(transition==6)return resume_level_flow();
 if(transition==1){
  int index=launch_level,from_home=launch_campaign_home;launch_level=launch_campaign_home=0;audio_close();
  if(campaign_screen==HOME_RESULTS){tr2_home_close();home_session=home_playing=0;campaign_screen=PLAY;}
  if(start_campaign_level(index,0,from_home))return 1;
  level_available[index]=0;if(!index)home_available=0;return return_to_title();
 }
 if(transition==2){tr2_home_interpolation_reset();home_playing=1;ring_exit=0;tr2_audio_inventory(0);return 1;}
 return return_to_title();
}
