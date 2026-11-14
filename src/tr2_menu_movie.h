/* Fit into the original 4:3 playback area, then contain the source within it. */
static void media_extent(int screen_w,int screen_h,int source_w,int source_h,int *w,int *h){
 int area_w=screen_w,area_h=screen_w*3/4;if(area_h>screen_h){area_h=screen_h;area_w=screen_h*4/3;}
 *w=area_w;*h=area_w*source_h/source_w;if(*h>area_h){*h=area_h;*w=area_h*source_w/source_h;}
}
static void media_picture_gain(GLuint texture,int source_w,int source_h,float gain){
 glViewport(0,0,width,height);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);menu_flat();glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
 int vw,vh;media_extent(width,height,source_w,source_h,&vw,&vh);glViewport((width-vw)/2,(height-vh)/2,vw,vh);
 glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);glColor3f(gain,gain,gain);glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2i(0,0);glTexCoord2f(1,0);glVertex2i(640,0);glTexCoord2f(1,1);glVertex2i(640,480);glTexCoord2f(0,1);glVertex2i(0,480);glEnd();
}
static void media_picture(GLuint texture,int source_w,int source_h){media_picture_gain(texture,source_w,source_h,1);}
typedef struct LegalFade {int phase;double elapsed;} LegalFade;
static void legal_advance(LegalFade *fade,double dt,int skip){
 /* 454c14: 30-step palette fade in; 44c486: 10-step fade to black.
    450cb0 paces each palette step at two 60 Hz ticks. */
 const double duration[]={1,3,10.0/30};
 if(fade->phase==1 && skip){fade->phase=2;fade->elapsed=0;return;}
 fade->elapsed+=dt;
 while(fade->phase<3 && fade->elapsed+1e-9>=duration[fade->phase]){
  fade->elapsed=fmax(0,fade->elapsed-duration[fade->phase]);fade->phase++;
  if(fade->phase==1 && skip){fade->phase=2;fade->elapsed=0;break;}
 }
}
static float legal_gain(const LegalFade *fade){
 int step=(int)floor(fade->elapsed*30+1e-7);
 return fade->phase==0?clamp(step,0,30)/30.f:fade->phase==1?1:fade->phase==2?1-clamp(step,0,10)/10.f:0;
}
static int legal_present(int test,const char *shot){
 GLuint picture=0;if(!picture_load("DATA/LEGAL.PCX",&picture))return 0;
 /* 454be2: legal.pcx; 454c1d: Wait(180,1). Timer 4566fa uses 1/60 s. */
 LARGE_INTEGER frequency,last,now;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&last);LegalFade fade={0};int ok=1;
 intro_active=2;intro_skip=0;pad_previous=~0u;
 while(running && fade.phase<3){
  int focused=pump();QueryPerformanceCounter(&now);double dt=(double)(now.QuadPart-last.QuadPart)/frequency.QuadPart;last=now;
  if(focused || test || shot){
   media_picture_gain(picture,4,3,shot?1:legal_gain(&fade));if(shot)ok=capture_image(shot);SwapBuffers(host->dc);if(shot)break;
   legal_advance(&fade,test?1.0/30:dt,intro_skip);
  }
  if(!test)MsgWaitForMultipleObjectsEx(0,NULL,8,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
 }
 glDeleteTextures(1,&picture);intro_active=intro_skip=0;ring_held_key=0;pad_previous=~0u;
 glViewport(0,0,width,height);glClear(GL_COLOR_BUFFER_BIT);SwapBuffers(host->dc);return ok;
}
/* 44cb40 shows CREDIT01..09 with cue 52, 450 ticks per picture at 60 Hz. */
static int credits_present(void){
 LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
 intro_active=2;intro_skip=0;pad_previous=~0u;
 for(int page=1;page<=9 && running;page++){
  char path[32];snprintf(path,sizeof path,"DATA/CREDIT%02d.PCX",page);
  GLuint picture=0;if(!picture_load(path,&picture)){intro_active=0;return 0;}
  LARGE_INTEGER last,now;QueryPerformanceCounter(&last);double elapsed=0;
  intro_skip=0;
  while(running && !intro_skip && elapsed<1+7.5+10.0/30){
   int focused=pump();QueryPerformanceCounter(&now);
   tr2_audio_pause(!focused && !movie_test);
   if(focused || movie_test){
    elapsed+=movie_test?1:(double)(now.QuadPart-last.QuadPart)/frequency.QuadPart;
    double gain=elapsed<1?elapsed:elapsed<8.5?1:fmax(0,1-(elapsed-8.5)*3);
    media_picture_gain(picture,4,3,(float)gain);SwapBuffers(host->dc);
   }
   last=now;if(!movie_test)MsgWaitForMultipleObjectsEx(0,NULL,8,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
  }
  glDeleteTextures(1,&picture);
 }
 intro_active=intro_skip=0;pad_previous=~0u;return 1;
}
static int movie_play(const char *relative,int test){
 char path[MAX_PATH];asset_path(path,relative);TombMovie movie;if(!tomb_movie_open(&movie,path))return 0;
 HWAVEOUT device=NULL;WAVEHDR buffer={0};int ok=1;GLuint texture=0;
 if(movie.audio_bytes){
  for(unsigned i=0;i+1<movie.audio_bytes;i+=2){int value=tr2_i16(movie.audio+i)*title_music/10;movie.audio[i]=(unsigned char)value;movie.audio[i+1]=(unsigned char)((unsigned)value>>8);}
  WAVEFORMATEX fmt={WAVE_FORMAT_PCM,(WORD)movie.channels,movie.rate,movie.rate*movie.channels*2,(WORD)(movie.channels*2),16,0};
  if(waveOutOpen(&device,tomb_sound_output_device(),&fmt,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){ok=0;goto done;}
  waveOutPause(device);buffer.lpData=(LPSTR)movie.audio;buffer.dwBufferLength=movie.audio_bytes;
  if(waveOutPrepareHeader(device,&buffer,sizeof buffer)!=MMSYSERR_NOERROR || waveOutWrite(device,&buffer,sizeof buffer)!=MMSYSERR_NOERROR){ok=0;goto done;}
 }
 glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 /* Linear filtering must not blend opposite edges of the movie frame. */
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812f /* GL_CLAMP_TO_EDGE */);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812f /* GL_CLAMP_TO_EDGE */);
 glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,(int)movie.width,(int)movie.height,0,GL_RGB,GL_UNSIGNED_BYTE,NULL);
 LARGE_INTEGER frequency,previous,now;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&previous);double elapsed=0;unsigned last=~0u;int active=0;
 intro_active=1;intro_skip=0;
 while(running && !intro_skip){
  int focused=pump();QueryPerformanceCounter(&now);double dt=(double)(now.QuadPart-previous.QuadPart)/frequency.QuadPart;previous=now;
  if(device && active!=focused){if(focused)waveOutRestart(device);else waveOutPause(device);}active=focused;
  if(!focused && !test){MsgWaitForMultipleObjectsEx(0,NULL,16,QS_ALLINPUT,MWMO_INPUTAVAILABLE);continue;}
  elapsed+=dt;double clock=elapsed;MMTIME time={0};time.wType=TIME_SAMPLES;
  if(device && waveOutGetPosition(device,&time,sizeof time)==MMSYSERR_NOERROR && time.wType==TIME_SAMPLES && !(buffer.dwFlags&WHDR_DONE))clock=(double)time.u.sample/movie.rate;
  unsigned frame=test?(last==~0u?0:last==0?movie.frames/2:movie.frames-1):(unsigned)(clock*movie.numerator/movie.denominator);
  if(frame>=movie.frames)break;
  if(frame!=last){if(!tomb_movie_frame(&movie,frame)){ok=0;break;}glBindTexture(GL_TEXTURE_2D,texture);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,(int)movie.width,(int)movie.height,GL_RGB,GL_UNSIGNED_BYTE,movie.pixels);last=frame;}
  media_picture(texture,(int)movie.width,(int)movie.height);SwapBuffers(host->dc);
  if(test && frame==movie.frames-1)break;MsgWaitForMultipleObjectsEx(0,NULL,8,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
 }
done:
 intro_active=0;intro_skip=0;ring_held_key=0;pad_previous=~0u;
 if(device){waveOutReset(device);if(buffer.dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(device,&buffer,sizeof buffer);waveOutClose(device);}
 if(texture)glDeleteTextures(1,&texture);tomb_movie_close(&movie);
 glViewport(0,0,width,height);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);SwapBuffers(host->dc);return ok;
}
static void menu_reset(void){death_passport=0;lock_ring=0;if(home_session)inventory_refresh();new_game_page=PASSPORT;game_ring=ring_switch=switch_pitch=flare_pick=0;memset(ring_memory,0,sizeof ring_memory);memset(ring_items,0,sizeof ring_items);title_selection=title_page=title_choice=return_item=return_frame=turning=turn_ticks=ring_exit=ring_held_key=0;title_angle=ring_open=ring_fraction=0;control_capture=-1;running=1;}
static void test_settle(void){ring_held_key=0;for(int i=0;i<240;i++)menu_tick(1.0/60);}
static int settings_roundtrip_test(void){
 char directory[MAX_PATH],temporary[MAX_PATH],saved_path[MAX_PATH];int saved_keys[14],music=title_music,effects=title_effects,layout=control_layout,framerate=title_framerate,resolution=title_resolution,filtering=title_filtering,force43=title_force43,water_style=title_water_style,distance=title_draw_distance;
 memcpy(saved_path,config_path,sizeof saved_path);memcpy(saved_keys,control_keys,sizeof saved_keys);
 if(!GetTempPathA(MAX_PATH,directory) || !GetTempFileNameA(directory,"tr2",0,temporary))return 0;
 snprintf(config_path,sizeof config_path,"%s",temporary);title_music=3;title_effects=7;control_layout=1;
 memcpy(control_keys,control_alternate,sizeof control_keys);control_keys[0]='W';settings_save();title_music=title_effects=0;control_layout=0;control_keys[0]='Q';settings_load();
 int ok=title_music==3 && title_effects==7 && control_layout==1 && control_keys[0]=='W';
 for(int cap=0;cap<3;cap++){title_framerate=cap;settings_save();title_framerate=-1;settings_load();ok=ok && title_framerate==cap;}
 WritePrivateProfileStringA("Display","FramerateLimit",NULL,config_path);title_framerate=-1;settings_load();ok=ok && title_framerate==0;
 for(int value=0;value<4;value++){title_resolution=value;settings_save();title_resolution=-1;settings_load();ok=ok && title_resolution==value;}
 for(int value=-1;value<=4;value+=5){char invalid[16];snprintf(invalid,sizeof invalid,"%d",value);WritePrivateProfileStringA("Display","Resolution",invalid,config_path);settings_load();ok=ok && title_resolution==0;}
 WritePrivateProfileStringA("Display","Resolution",NULL,config_path);settings_load();ok=ok && title_resolution==0;
 for(int n=0;n<2;n++){title_filtering=n;settings_save();title_filtering=-1;settings_load();ok=ok && title_filtering==n;}
 WritePrivateProfileStringA("Display","TextureFiltering",NULL,config_path);settings_load();ok=ok && title_filtering==1;
 WritePrivateProfileStringA("Display","TextureFiltering","9",config_path);settings_load();ok=ok && title_filtering==1;
 for(int n=0;n<2;n++){title_force43=n;settings_save();title_force43=-1;settings_load();ok=ok && title_force43==n;}
 WritePrivateProfileStringA("Display","Force43",NULL,config_path);settings_load();ok=ok && title_force43==0;
 WritePrivateProfileStringA("Display","Force43","9",config_path);settings_load();ok=ok && title_force43==0;
 for(int n=0;n<2;n++){title_water_style=n;settings_save();title_water_style=-1;settings_load();ok=ok && title_water_style==n;}
 WritePrivateProfileStringA("Display","WaterStyle",NULL,config_path);settings_load();ok=ok && title_water_style==0;
 WritePrivateProfileStringA("Display","WaterStyle","9",config_path);settings_load();ok=ok && title_water_style==0;
 for(int n=0;n<DRAW_DISTANCE_COUNT;n++){title_draw_distance=n;settings_save();title_draw_distance=-1;settings_load();ok=ok && title_draw_distance==n;}
 WritePrivateProfileStringA("Display","DrawDistance",NULL,config_path);settings_load();ok=ok && title_draw_distance==0;
 WritePrivateProfileStringA("Display","DrawDistance","9",config_path);settings_load();ok=ok && title_draw_distance==0;
 DeleteFileA(temporary);memcpy(config_path,saved_path,sizeof config_path);memcpy(control_keys,saved_keys,sizeof saved_keys);title_music=music;title_effects=effects;control_layout=layout;title_framerate=framerate;title_resolution=resolution;tr2_home_resolution(resolution);title_filtering=filtering;filtering_apply();title_force43=force43;tr2_home_force43(force43);title_water_style=water_style;tr2_home_water_style(water_style);title_draw_distance=distance;tr2_home_draw_distance(distance);return ok;
}
static int filtering_check(void){
 for(int group=0;group<(home_session?2:1);group++){
  unsigned pages[256];Tr2Level *assets=group?tr2_home_menu_assets(pages):title_level;
  if(!group)memcpy(pages,title_textures,sizeof pages);
  for(unsigned i=0;i<assets->tile_count;i++){GLint min_filter,mag_filter;glBindTexture(GL_TEXTURE_2D,pages[i]);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&min_filter);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&mag_filter);if(min_filter!=(title_filtering?GL_LINEAR:GL_NEAREST) || mag_filter!=min_filter)return 0;}
 }
 return glGetError()==GL_NO_ERROR;
}
/* Compare the centred picture pixel-for-pixel with rendering at the fitted
   dimensions, and require every border pixel to be black. */
static int aspect_render_test(int home){
 int full_w=width,full_h=height,w=width,h=height,saved=title_force43;
 tomb_render_fit43(&w,&h);
 size_t bytes=(size_t)w*h*3,full_bytes=(size_t)full_w*full_h*3;
 unsigned char *expected=malloc(bytes),*actual=malloc(full_bytes);
 if(!expected || !actual){free(expected);free(actual);return 0;}
 title_force43=0;tr2_home_force43(0);width=w;height=h;
 if(home)tr2_home_render(w,h);else title_render();
 glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,expected);
 width=full_w;height=full_h;title_force43=1;tr2_home_force43(1);
 if(home)tr2_home_render(width,height);else title_render();
 glReadPixels(0,0,full_w,full_h,GL_RGB,GL_UNSIGNED_BYTE,actual);
 int x0=(full_w-w)/2,y0=(full_h-h)/2,ok=1;
 for(int y=0;y<full_h && ok;y++)for(int x=0;x<full_w;x++){
  const unsigned char *p=actual+((size_t)y*full_w+x)*3;
  if(x>=x0 && x<x0+w && y>=y0 && y<y0+h){if(memcmp(p,expected+((size_t)(y-y0)*w+x-x0)*3,3))ok=0;}
  else if(p[0] || p[1] || p[2])ok=0;
 }
 title_force43=saved;tr2_home_force43(saved);free(expected);free(actual);
 return ok && glGetError()==GL_NO_ERROR;
}
static void campaign_events(void){
    if(tr2_level_take_autosave())write_autosave();
    if(!pending_transition && tr2_level_finished()){
     if(campaign_screen==CUTSCENE)pending_transition=6;
     else if(!current_level && tr2_home_campaign_exit()){home_playing=0;menu_reset();title_choice=0;campaign_screen=HOME_RESULTS;tr2_audio_inventory(1);}
     else pending_transition=5;
    }else if(tr2_home_return_title()){
     if(current_level){home_playing=0;menu_reset();game_ring=OPTION_RING;death_passport=lock_ring=1;scan_slots();tr2_audio_inventory(1);memset(home_keys,0,sizeof home_keys);}
     else pending_transition=3;
    }else{
     int request=tr2_level_keys_requested();if(request)open_key_ring(request);
    }
}
static int skip_from_inventory_test(void){
 title_key(VK_ESCAPE);test_settle();title_key(VK_DOWN);test_settle();
 title_key(VK_RIGHT);test_settle();
 if(game_ring!=OPTION_RING || ring_id(title_selection)!=122)return 0;
 title_key(VK_RETURN);test_settle();
 if(title_page!=CHEATS_PAGE || tr2_level_finished())return 0;
 title_key(VK_ESCAPE);test_settle();
 if(title_page || tr2_level_finished())return 0;
 title_key(VK_RETURN);test_settle();title_key(VK_RETURN);
 if(tr2_level_finished())return 0;
 test_settle();
 if(ring_open!=0 || !home_transition())return 0;
 campaign_events();return home_transition();
}
static int custom_campaign_test(void){
 char path[MAX_PATH];slot_path(path,TOMB_MANUAL_SLOTS);
 launch_options=TR2_AUTOSAVE|TR2_CONSISTENT_WEAPONS;
 if(!start_campaign_level(1,0,0) || tr2_slot_level(path)!=1)return 0;
 const Tr2World *w=tr2_level_world();
 tr2_inventory_add((Tr2World *)w,138);tr2_inventory_add((Tr2World *)w,140);
 tr2_inventory_use((Tr2World *)w,NULL,138);
 for(int i=0;i<90;i++)tr2_home_update(1.0/30,0);
 tr2_inventory_use((Tr2World *)w,NULL,140);
 for(int i=0;i<150;i++)tr2_home_update(1.0/30,0);
 if(w->level->custom.sidearm!=3 || w->level->custom.last_drawn!=5 || tr2_level_take_autosave())return 0;
 if(!tr2_wall_key_test(193,0) || !tr2_level_take_autosave())return 0;
 write_autosave();
 if(!tr2_level_restore(asset_root,level_files[1],1,path) || tr2_level_take_autosave() ||
    w->level->custom.options!=launch_options || !w->inventory->count[193])return 0;
 if(!skip_from_inventory_test() || campaign_screen!=CUTSCENE || w->level->custom.sidearm!=3 || w->combat->back_weapon!=5)return 0;
 tr2_home_render(width,height);
 if(!start_campaign_level(2,1,0) || w->combat->weapon!=5 || w->combat->back_weapon!=5 || w->level->custom.sidearm!=3 || tr2_slot_level(path)!=2)return 0;
 if(!tr2_venice_water_test())return 0;
 for(int tick=0;tick<120;tick++)tr2_home_update(1.0/30,0);
 if(!tr2_level_take_autosave())return 0;
 tr2_level_remember();
 if(!start_campaign_level(5,1,0) || w->level->custom.sidearm || w->level->custom.last_drawn || w->combat->back_weapon)return 0;
 if(!start_campaign_level(0,0,1) || !skip_from_inventory_test() || campaign_screen!=HOME_RESULTS)return 0;
 title_key(VK_RETURN);if(!home_transition() || current_level!=1)return 0;
 fprintf(stderr,"TR2B inventory Skip Level, cancel, ring closure, Great Wall cutscene and campaign Home progression: PASS\n");
 if(!start_campaign_level(18,0,0) || !tr2_final_level_test(18))return 0;
 if(!start_campaign_level(18,0,0) || !skip_from_inventory_test() || campaign_screen!=PLAY || !tr2_unarmed_shower_test())return 0;
 campaign_events();if(!home_transition() || campaign_screen!=FINAL_RESULTS)return 0;
 fprintf(stderr,"TR2B unarmed Home Sweet Home skip with Consistent Weapons: shower, shotgun reveal and final statistics PASS\n");
 launch_options=0;DeleteFileA(path);
 if(!return_to_title())return 0;
 fprintf(stderr,"TR2B enhancements: autosave, restore, sidearms, drawn rifle carry, cutscene and seven shower weapon meshes: PASS\n");
 return glGetError()==GL_NO_ERROR;
}
static int menu_self_test(void){
 LegalFade fade={0};legal_advance(&fade,.5,0);if(fabs(legal_gain(&fade)-.5f)>.001)return 0;
 legal_advance(&fade,.5,0);if(fade.phase!=1 || legal_gain(&fade)!=1)return 0;
 legal_advance(&fade,0,1);if(fade.phase!=2 || legal_gain(&fade)!=1)return 0;
 legal_advance(&fade,5.0/30,1);if(fabs(legal_gain(&fade)-.5f)>.001)return 0;
 legal_advance(&fade,5.0/30,1);if(fade.phase!=3 || legal_gain(&fade)!=0)return 0;
 fade=(LegalFade){0};legal_advance(&fade,4,0);if(fade.phase!=2 || legal_gain(&fade)!=1)return 0;
 legal_advance(&fade,10.0/30,0);if(fade.phase!=3)return 0;
 int vw,vh;media_extent(1920,1080,320,160,&vw,&vh);if(vw!=1440 || vh!=720)return 0;
 media_extent(1920,1080,320,240,&vw,&vh);if(vw!=1440 || vh!=1080)return 0;
 media_extent(640,480,320,160,&vw,&vh);if(vw!=640 || vh!=320)return 0;
 media_extent(600,900,320,160,&vw,&vh);if(vw!=600 || vh!=300)return 0;
 if(!settings_roundtrip_test())return 0;
 if(!custom_campaign_test()){fprintf(stderr,"TR2B custom campaign regression failed\n");return 0;}
 {int rw,rh;const int expected_w[]={1920,1067,853,512},expected_h[]={1080,600,480,240};
  for(int n=0;n<4;n++){tr2_resolution_extent(n,1920,1080,&rw,&rh);if(rw!=expected_w[n] || rh!=expected_h[n])return 0;}
  tr2_resolution_extent(3,960,720,&rw,&rh);if(rw!=384 || rh!=240)return 0;
  tr2_resolution_extent(1,320,240,&rw,&rh);if(rw!=320 || rh!=240)return 0;
 }
 int saved_keys[14];memcpy(saved_keys,control_keys,sizeof saved_keys);
 if(!controls_bind(0,'W') || !control_matches(0,'W') || !control_matches(0,VK_UP))return 0;
 if(!controls_bind(1,VK_UP) || control_matches(0,VK_UP) || !control_matches(1,VK_UP))return 0;
 memcpy(control_keys,saved_keys,sizeof saved_keys);
 /* Num Lock-off keypad Up must bind and release as Num 8, not Up. */
 memset(home_keys,0,sizeof home_keys);int saved_capture=control_capture;
 control_keys[0]=VK_NUMPAD8;
 SendMessageA(host->window,WM_KEYDOWN,VK_UP,(LPARAM)(0x48u<<16));
 if(!home_keys[VK_NUMPAD8] || home_keys[VK_UP])return 0;
 SendMessageA(host->window,WM_KEYUP,VK_NUMPAD8,(LPARAM)((0x48u<<16)|0xc0000000u));
 if(home_keys[VK_NUMPAD8])return 0;
 memcpy(control_keys,saved_keys,sizeof saved_keys);control_capture=saved_capture;ring_held_key=0;
 const int menu_sounds[]={108,111,112,115};for(unsigned n=0;n<sizeof menu_sounds/sizeof *menu_sounds;n++){int id=menu_sounds[n];
  int detail=tr2_i16(level->sound_map+id*2);if(detail<0 || (unsigned)detail>=level->sound_details.count)return 0;
  unsigned sample=tr2_u16(level->sound_details.data+detail*8);if(sample>=level->sample_indices.count || !audio_samples[sample].data)return 0;
 }menu_sound(108);audio_pump();
 menu_reset();test_settle();SendMessageA(host->window,WM_KEYDOWN,VK_RETURN,1);test_settle();if(title_page!=1 || !page_ready())return 0;
 title_key(VK_RETURN);if(title_page!=1 || ring_exit)return 0;
 title_key(VK_RIGHT);test_settle();title_key(VK_RETURN);if(title_choice!=1 || new_game_page!=CAMPAIGN || ring_exit)return 0;
 for(int index=1;index>=0;index--){
  for(int page=CAMPAIGN;page<STARTING_LEVEL;page++){
   if(page==GAMEPLAY){
    enhancement_row=0;selected_options=0;
    title_key(VK_DOWN);title_key(VK_RETURN);if(new_game_page!=ENHANCEMENTS)return 0;
    title_key(VK_RIGHT);title_key(VK_DOWN);title_key(VK_RETURN);
    if(selected_options!=(TR2_AUTOSAVE|TR2_CONSISTENT_WEAPONS))return 0;
    title_key(VK_DOWN);title_key(VK_RETURN);if(new_game_page!=STARTING_LEVEL)return 0;
    title_key(VK_ESCAPE);title_key(VK_ESCAPE);title_key(VK_UP);
   }else title_key(VK_DOWN);
   title_key(VK_RETURN);if((int)new_game_page!=page+1)return 0;
   title_key(VK_ESCAPE);if((int)new_game_page!=page)return 0;title_key(VK_RETURN);
  }
  for(int i=2;i<(int)level_count;i++){title_key(VK_DOWN);if(selected_level!=i)return 0;}
  for(int i=(int)level_count-2;i>=index;i--){title_key(VK_UP);if(selected_level!=i)return 0;}
  title_render();title_key(VK_RETURN);test_settle();
  if(launch_level!=index || pending_transition!=1 || !home_transition() || !home_playing)return 0;
  tr2_home_update(1.0/30,0);tr2_home_render(width,height);if(glGetError()!=GL_NO_ERROR)return 0;
  if(index==1){
   if(!tr2_wall_ammo_hud_test() || !tr2_wall_water_fog_test())return 0;
   const Tr2World *w=tr2_level_world();if(w->rewards[6]!=1 || w->rewards[13]!=2 || w->rewards[15]!=1)return 0;
   title_key(VK_ESCAPE);test_settle();if(ring_count()!=6)return 0;
   title_key(VK_DOWN);test_settle();title_key(VK_RETURN);test_settle();title_key(VK_RIGHT);test_settle();title_key(VK_RETURN);
   if(campaign_screen!=SAVE_SLOTS)return 0;title_key(VK_RETURN);
   menu_tick(10.0/60);if(ring_open!=1 || ring_items[133].frame>=19 || !return_item)return 0;
   test_settle();if(!home_transition() || !home_playing || scan_slots()<0)return 0;
   open_slots(0);title_key(VK_RETURN);if(!home_transition() || !home_playing || current_level!=1)return 0;
   if(!tr2_home_presentation_test(1))return 0;campaign_events();test_settle();
   if(!death_passport || ring_count()!=3 || campaign_screen!=LOAD_SLOTS || title_page!=1)return 0;
   title_key(VK_ESCAPE);if(campaign_screen!=LOAD_SLOTS)return 0;
   title_key(VK_RIGHT);test_settle();if(title_choice!=2 || !page_ready())return 0;
   title_key(VK_LEFT);test_settle();if(campaign_screen!=LOAD_SLOTS)return 0;
   title_key(VK_RETURN);test_settle();if(pending_transition!=4 || !home_transition() || tr2_home_dead() || death_passport)return 0;
   char death_slot[MAX_PATH];slot_path(death_slot,slot_cursor);DeleteFileA(death_slot);
   if(!tr2_home_presentation_test(1))return 0;campaign_events();test_settle();
   if(!death_passport || !page_ready() || title_choice!=2 || slots_available)return 0;
   title_key(VK_LEFT);title_key(VK_ESCAPE);if(title_choice!=2 || title_page!=1)return 0;
   title_key(VK_RETURN);test_settle();if(pending_transition!=3 || !home_transition() || home_session)return 0;
   launch_level=1;pending_transition=1;if(!home_transition() || !home_playing)return 0;
   fprintf(stderr,"TR2B death passport, load slots, no-save exit and ring closing: PASS\n");
   for(int key=193;key<=194;key++){
    if(!tr2_wall_key_test(key,0) || !tr2_wall_key_test(key,1) || !tr2_level_keys_requested())return 0;
    open_key_ring(1);test_settle();if(ring_pickup(title_selection)!=key || !ring_name(title_selection)[0])return 0;
    title_render();title_key(VK_RETURN);test_settle();if(!home_transition() || !tr2_wall_key_test(key,2))return 0;
   }
   if(!tr2_wall_reward_test())return 0;
   fprintf(stderr,"TR2B real key pickups, lock ring, key consumption, both doors and dragon reward: PASS\n");
   char path[MAX_PATH];slot_path(path,slot_cursor);DeleteFileA(path);scan_slots();
   /* Finish with the secret reward selected. Venice resets the selection
      and chooses the owned shotgun ahead of the grenade launcher. */
   w->combat->weapon=w->combat->requested=w->combat->last_weapon=6;
   w->combat->back_weapon=6;
   if(!tr2_wall_effect_test() || !tr2_wall_exit_test())return 0;campaign_events();if(pending_transition!=5 || !home_transition() || campaign_screen!=CUTSCENE)return 0;
   for(int frame=1;frame<=2504;frame++){
    tr2_home_update(1.0/30,0);
    if(!tr2_cut_actor_test())return 0;
    if((frame==318 || frame==319 || frame==1976 || frame==1977) && !tr2_cut1_pistols_test(frame))return 0;
   }
   tr2_home_render(width,height);if(!tr2_level_finished() || glGetError()!=GL_NO_ERROR)return 0;
   campaign_events();if(!home_transition() || campaign_screen!=RESULTS || tr2_audio_music_cue()!=41)return 0;title_render();
   if(!capture_image("build/tr2-wall-results.ppm"))return 0;
   title_key(VK_RETURN);if(!home_transition() || current_level!=2 || tr2_level_world()->inventory->count[136]!=1 || tr2_level_world()->combat->ammo[6]!=6 || tr2_level_world()->inventory->secrets)return 0;
   if(w->combat->weapon!=1 || w->combat->requested!=1 || w->combat->last_weapon!=1 ||
      w->combat->back_weapon!=4 || w->start_combat->back_weapon!=4 || !w->inventory->count[141])return 0;
   if(!tr2_level_save(path,2) || !tr2_venice_water_test() || !tr2_level_restore(asset_root,level_files[2],2,path))return 0;
   DeleteFileA(path);begin_level_flow(2);
   if(!tr2_venice_exit_test())return 0;campaign_events();
   if(!home_transition() || campaign_screen!=RESULTS || current_level!=2)return 0;
   title_key(VK_RETURN);if(!home_transition() || current_level!=3 || w->combat->ammo[6]!=6 || w->inventory->ticks)return 0;
   const Tr2Room *sunset_room=NULL;
   for(unsigned r=0;r<w->level->room_count;r++)if(w->level->rooms[r].light_mode==3){sunset_room=&w->level->rooms[r];break;}
   if(!sunset_room || !w->level->sunset)return 0;
   const int times[]={0,1161,1162,18000,35999,36000,37000},phases[]={0,0,1,15,30,31,31};
   for(unsigned i=0;i<sizeof times/sizeof *times;i++){
    w->inventory->ticks=times[i];if(tr2_room_light_phase(w->level,sunset_room,19)!=phases[i])return 0;
   }
   w->inventory->ticks=18000;
   if(!tr2_level_save(path,3) || !tr2_level_restore(asset_root,level_files[3],3,path))return 0;
   DeleteFileA(path);begin_level_flow(3);
   if(!w->level->sunset || w->inventory->ticks!=18000)return 0;
   if(!tr2_wall_key_test(194,0) || !tr2_bartoli_detonator_test(0))return 0;
   campaign_events();test_settle();if(!lock_ring || ring_pickup(title_selection)!=194)return 0;
   title_key(VK_ESCAPE);test_settle();if(!home_transition() || !w->inventory->count[194])return 0;
   if(!tr2_bartoli_detonator_test(0))return 0;
   campaign_events();test_settle();title_key(VK_RETURN);test_settle();
   if(!home_transition() || !tr2_bartoli_detonator_test(1))return 0;
   fprintf(stderr,"TR2B Bartoli sunset timing and restore, detonator ring cancel/use and demolition: PASS\n");
   /* Exercise each remaining script boundary without playing through each map. */
   for(int i=3;i<(int)level_count;i++){
    pending_transition=5;if(!home_transition())return 0;
    if(campaign_screen==CUTSCENE){tr2_home_update(1.0/30,0);if(!tr2_cut_swap_test())return 0;tr2_home_render(width,height);
     title_key(VK_ESCAPE);if(!home_transition())return 0;}
    int last=i+1==(int)level_count;
    if(campaign_screen!=(last?FINAL_RESULTS:RESULTS))return 0;
    if(i==17 && (flow_words[flow_cursor]!=FLOW_MOVIE || _stricmp(flow_movies[flow_words[flow_cursor+1]],"FMV/END.bmv")))return 0;
    if(last){if(tr2_campaign_totals().secrets!=3)return 0;title_render();}
    title_key(VK_RETURN);if(!home_transition())return 0;
    if(last){if(home_session)return 0;}
    else if(current_level!=i+1 || campaign_screen!=PLAY || !home_playing)return 0;
    if(current_level==4 && !tr2_creature_contact_test((const int[]){15,21,20},3))return 0;
    if(current_level==5){
     if(w->inventory->count[135] || !tr2_creature_contact_test((const int[]){29,30,31},3))return 0;
     fprintf(stderr,"TR2B Offshore Rig diver and both armed workers: land, underwater and defeated contacts PASS\n");
    }
    if(current_level==6 && !tr2_creature_contact_test((const int[]){34},1))return 0;
    if(current_level==7 && !tr2_fathoms_shark_death_test())return 0;
    if(current_level==9 && !tr2_creature_contact_test((const int[]){33},1))return 0;
    if(current_level>=11 && current_level<=14 && !tr2_tibet_level_test(current_level))return 0;
    if(!last && current_level>=15 && !tr2_final_level_test(current_level))return 0;
    if(current_level==12 && !w->inventory->count[177])return 0;
   }
   fprintf(stderr,"TR2B all campaign boundaries, Venice exit, cutscenes, post-statistics FMV and final credits: PASS\n");
   fprintf(stderr,"TR2B campaign equipment, passport save/load, CUT1, results and Venice carry: PASS\n");
  }else{
   if(!tr2_home_gate_test(1))return 0;
   campaign_events();if(campaign_screen!=HOME_RESULTS || home_playing)return 0;
   title_render();title_key(VK_RETURN);
   if(!home_transition() || current_level!=1 || !home_playing || !tr2_level_world()->inventory->count[136])return 0;
   for(int i=2;i<(int)level_count;i++){
    launch_level=i;pending_transition=1;
    if(!home_transition() || current_level!=i || !home_playing)return 0;
    tr2_home_update(1.0/30,0);tr2_home_render(width,height);
    if(glGetError()!=GL_NO_ERROR)return 0;
   }
   fprintf(stderr,"TR2B all campaign starts and Home gate to Best Times and Great Wall: PASS\n");
  }
  pending_transition=3;if(!home_transition() || home_session)return 0;
  test_settle();title_key(VK_RETURN);test_settle();title_key(VK_RIGHT);test_settle();title_key(VK_RETURN);
 }
 title_key(VK_ESCAPE);if(new_game_page!=PASSPORT || title_choice!=1)return 0;
 title_key(VK_ESCAPE);test_settle();if(title_page || return_item)return 0;
 title_key(VK_RIGHT);test_settle();if(title_ids[title_selection]!=122)return 0;
 int available=home_available;home_available=0;title_key(VK_RETURN);test_settle();if(title_page || ring_items[122].lift)return 0;home_available=available;
 title_key(VK_RETURN);test_settle();if(title_page!=5 || ring_items[122].lift!=16)return 0;
 title_key(VK_ESCAPE);test_settle();
 title_key(VK_RIGHT);test_settle();if(title_ids[title_selection]!=154)return 0;
 title_key(VK_RETURN);test_settle();if(title_page!=3)return 0;
 int music=title_music;title_key(VK_LEFT);if(title_music!=clamp(music-1,0,10))return 0;title_music=music;
 title_key(VK_ESCAPE);test_settle();title_key(VK_RIGHT);test_settle();if(title_ids[title_selection]!=153)return 0;
 title_key(VK_RETURN);test_settle();if(title_page!=7 || !page_ready())return 0;
 {int cap=title_framerate;title_key(VK_DOWN);title_key(VK_DOWN);for(int n=1;n<=3;n++){title_key(VK_RIGHT);if(title_framerate!=(cap+n)%3)return 0;}
  for(int n=1;n<=3;n++){title_key(VK_LEFT);if(title_framerate!=(cap+6-n)%3)return 0;}
  title_key(VK_UP);title_key(VK_UP);title_key(VK_RETURN);if(title_page!=7 || title_choice!=0 || title_framerate!=cap)return 0;}
 {int resolution=title_resolution;for(int n=1;n<=4;n++){title_key(VK_RIGHT);if(title_resolution!=(resolution+n)%4)return 0;}for(int n=1;n<=4;n++){title_key(VK_LEFT);if(title_resolution!=(resolution+8-n)%4)return 0;}}
 {int resolution=title_resolution;for(int n=0;n<4;n++){char path[96];title_resolution=n;title_render();snprintf(path,sizeof path,"build/tr2-resolution-title-%d.ppm",n);if(!capture_image(path) || glGetError()!=GL_NO_ERROR)return 0;}title_resolution=resolution;}
 {int style=title_water_style;title_key(VK_UP);if(title_choice!=5)return 0;title_key(VK_RIGHT);if(title_water_style==style)return 0;title_key(VK_LEFT);if(title_water_style!=style)return 0;title_key(VK_DOWN);if(title_choice!=0)return 0;}
 {int distance=title_draw_distance;title_key(VK_UP);title_key(VK_UP);if(title_choice!=4)return 0;for(int n=1;n<=DRAW_DISTANCE_COUNT;n++){title_key(VK_RIGHT);if(title_draw_distance!=(distance+n)%DRAW_DISTANCE_COUNT)return 0;}for(int n=1;n<=DRAW_DISTANCE_COUNT;n++){title_key(VK_LEFT);if(title_draw_distance!=(distance+DRAW_DISTANCE_COUNT-n)%DRAW_DISTANCE_COUNT)return 0;}title_key(VK_DOWN);title_key(VK_DOWN);if(title_choice!=0)return 0;}
 {int filtering=title_filtering;title_key(VK_UP);title_key(VK_UP);title_key(VK_UP);if(title_choice!=3)return 0;
  title_key(VK_RIGHT);if(title_filtering==filtering)return 0;title_key(VK_LEFT);if(title_filtering!=filtering)return 0;
  for(int n=0;n<2;n++){title_filtering=n;filtering_apply();title_render();char path[96];snprintf(path,sizeof path,"build/tr2-filter-title-%d.ppm",n);if(!capture_image(path) || !filtering_check())return 0;}
  title_filtering=filtering;filtering_apply();title_key(VK_DOWN);title_key(VK_DOWN);title_key(VK_DOWN);if(title_choice!=0)return 0;}
 {int force43=title_force43;title_key(VK_DOWN);if(title_choice!=1)return 0;title_key(VK_RIGHT);if(title_force43==force43)return 0;title_key(VK_LEFT);if(title_force43!=force43)return 0;title_key(VK_UP);}
 title_render();if(!capture_image("build/tr2-details-title.ppm"))return 0;
 title_key(VK_ESCAPE);test_settle();if(title_page)return 0;
 title_key(VK_RIGHT);test_settle();title_key(VK_RETURN);test_settle();if(title_page!=4)return 0;
 title_key(VK_ESCAPE);test_settle();title_key(VK_RIGHT);test_settle();title_key(VK_RETURN);test_settle();title_key(VK_RIGHT);test_settle();title_key(VK_RIGHT);test_settle();title_key(VK_RETURN);test_settle();if(running)return 0;
 menu_reset();test_settle();return 1;
}
int tr2_menu_run(const Tr2MenuHost *platform,int argc,char **argv){
 host=platform;int test=0,home_test=0,direct_home=0,skip=0,page=0;const char *shot=NULL;
 for(int i=1;i<argc;i++){if(!strcmp(argv[i],"--tr2-test"))test=1;else if(!strcmp(argv[i],"--tr2-home-test")){home_test=1;skip=1;}else if(!strcmp(argv[i],"--tr2-home")){direct_home=1;skip=1;}else if(!strcmp(argv[i],"--tr2-no-intro"))skip=1;else if(!strcmp(argv[i],"--capture") && i+1<argc)shot=argv[++i];else if(!strcmp(argv[i],"--tr2-page") && i+1<argc)page=atoi(argv[++i]);}
 if(!title_init() || !flow_read()){title_close();return 1;}settings_load();menu_reset();home_session=home_playing=pending_transition=0;
 movie_test=test || home_test;if(test){char path[MAX_PATH];slot_path(path,0);DeleteFileA(path);remembered_slot=0;}campaign_screen=PLAY;current_level=0;scan_slots();launch_level=launch_campaign_home=0;launch_options=0;
 for(int index=0;index<(int)level_count;index++){
  level_available[index]=1;int movie=level_movie[index];const char *files[]={level_files[index],"sine.bin","atan.bin",movie>=0?flow_movies[movie]:NULL};
  for(int i=0;i<4 && files[i];i++){FILE *f=asset_open(files[i]);if(f)fclose(f);else level_available[index]=0;}
 }
 home_available=level_available[0];
 /* Tests never write to the user's configuration. */
 char saved_config[MAX_PATH];memcpy(saved_config,config_path,sizeof saved_config);if(test || home_test)config_path[0]=0;tr2_home_score_path(config_path);
 LONG_PTR previous=SetWindowLongPtrA(host->window,GWLP_WNDPROC,(LONG_PTR)title_window);RECT rect;GetClientRect(host->window,&rect);width=rect.right;height=rect.bottom;
 int ok=1;
 /* Off removes only the application cap; desktop presentation stays synced,
    including the disclaimer and movies that precede the main loop. */
 if(test)tomb_presentation_sync(0);
 int synced=tomb_presentation_sync(1);
 fprintf(stderr,"TR2B V-sync requested: on; reported active: %s\n",synced?"yes":"no");
 if(test && !synced)ok=0;
 if((!skip && !shot) || (shot && page==6))ok=legal_present(test,shot);
 if(ok && !skip && !shot)for(unsigned i=0;i<intro_count && running;i++)if(!movie_play(flow_movies[intro_movies[i]],test)){ok=0;break;}
 if(ok && running){ok=audio_open() && audio_title((int)title_track);}
 if(ok && test){ok=menu_self_test();fprintf(stderr,"TR2 menu state, disabled options, passport exit, audio and %u intro movies: %s\n",intro_count,ok?"PASS":"FAIL");}
 if(ok && (home_test || direct_home)){
  test_settle();title_key(VK_RIGHT);test_settle();title_key(VK_RETURN);test_settle();
  ok=title_page==5 && page_ready();if(ok){title_key(VK_RETURN);test_settle();ok=pending_transition==1 && home_transition() && home_playing && tr2_home_check(0);}
  if(ok && home_test){
   ok=tr2_home_depth_check(width,height);ok=ok && tr2_home_distance_test();
   if(ok && shot){for(int mode=0;mode<DRAW_DISTANCE_COUNT && ok;mode++){char path[96];tr2_home_draw_distance(mode);tr2_home_render(width,height);snprintf(path,sizeof path,"build/tr2-distance-%d.ppm",mode);ok=capture_image(path);}tr2_home_draw_distance(title_draw_distance);}
   SendMessageA(host->window,WM_KEYDOWN,(WPARAM)control_keys[0],1);
   for(int i=0;i<90;i++)tr2_home_update(1.0/30,home_input());
   SendMessageA(host->window,WM_KEYUP,(WPARAM)control_keys[0],1);
   int music_ok=tr2_audio_inventory_test();fprintf(stderr,"TR2 inventory music cursor pause/resume, menu output and focus: %s\n",music_ok?"PASS":"FAIL");ok=ok && music_ok;
   ok=ok && home_input()==0;
   ok=ok && tr2_home_check(1);tr2_home_render(width,height);if(shot)ok=ok && capture_image(shot);
   title_key(VK_ESCAPE);test_settle();ok=ok && !home_playing && home_session && game_ring==0 && ring_count()==1 && ring_id(0)==121;
   for(int direction=2;direction<=3;direction++){
    double angle=title_angle;title_key(control_keys[direction]);
    ok=ok && !turning && !turn_ticks && !ring_held_key && title_angle==angle;
    menu_tick(1);ok=ok && !turning && !turn_ticks && title_selection==0 && title_angle==angle;
   }
   title_key(VK_RETURN);test_settle();ok=ok && title_page==8 && page_ready();
   if(ok && shot){title_render();ok=capture_image("build/tr2-stopwatch-empty.ppm");}
   title_key(VK_RETURN);test_settle();ok=ok && !title_page && !return_item;
   title_key(VK_RETURN);test_settle();title_key(VK_ESCAPE);test_settle();ok=ok && !title_page && !home_playing;
   if(ok && shot){title_render();ok=capture_image("build/tr2-inventory-ring.ppm");}
   title_key(VK_DOWN);ring_fraction=0;menu_tick(24.0/60);ok=ok && game_ring==1 && ring_open==0;
   menu_tick(24.0/60);ok=ok && ring_open==1 && !switch_pitch;
   test_settle();ok=ok && game_ring==1 && ring_count()==4 && ring_id(0)==133;
   if(ok && shot){title_render();ok=capture_image("build/tr2-option-ring.ppm");}
   title_key(VK_LEFT);test_settle();title_key(VK_LEFT);test_settle();title_key(VK_RETURN);test_settle();
   ok=ok && title_page==7 && page_ready();{int cap=title_framerate;title_key(VK_DOWN);title_key(VK_DOWN);title_key(VK_RIGHT);title_key(VK_RETURN);ok=ok && title_choice==2 && title_page==7 && title_framerate==(cap+1)%3;title_key(VK_LEFT);ok=ok && title_framerate==cap;}
   if(ok && shot){int filtering=title_filtering;title_key(VK_DOWN);ok=title_choice==3;
    for(int n=0;n<2 && ok;n++){title_filtering=n;filtering_apply();title_render();char path[96];snprintf(path,sizeof path,"build/tr2-filter-inventory-%d.ppm",n);ok=capture_image(path) && filtering_check();}
    title_filtering=filtering;filtering_apply();title_key(VK_UP);}
   if(ok && shot){int resolution=title_resolution;for(int n=0;n<4 && ok;n++){char path[96];title_resolution=n;title_render();snprintf(path,sizeof path,"build/tr2-resolution-inventory-%d.ppm",n);ok=capture_image(path) && glGetError()==GL_NO_ERROR;}title_resolution=resolution;title_render();ok=ok && capture_image("build/tr2-details-ingame.ppm");}
   title_key(VK_ESCAPE);test_settle();title_key(VK_RIGHT);test_settle();title_key(VK_RIGHT);test_settle();
   title_key(VK_LEFT);test_settle();ok=ok && ring_id(title_selection)==155;
   title_key(VK_UP);test_settle();ok=ok && game_ring==0;
   title_key(VK_UP);test_settle();ok=ok && game_ring==0; /* No empty ITEMS ring. */
   title_key(VK_DOWN);test_settle();ok=ok && game_ring==1 && ring_id(title_selection)==155;
   title_key(VK_UP);test_settle();
   title_key(VK_ESCAPE);test_settle();ok=ok && home_transition() && home_playing;
   {int previous=control_keys[13];control_keys[13]='I';
    title_key('I');test_settle();ok=ok && !home_playing && !title_page;
    title_key(VK_RETURN);test_settle();title_key('I');test_settle();ok=ok && !title_page && !home_playing;
    title_key('I');test_settle();ok=ok && home_transition() && home_playing;control_keys[13]=previous;}
   title_key(VK_ESCAPE);test_settle();title_key(VK_DOWN);test_settle();title_key(VK_RETURN);test_settle();title_key(VK_RIGHT);test_settle();title_key(VK_RIGHT);test_settle();title_key(VK_RETURN);test_settle();
   ok=ok && home_transition() && !home_session && !home_playing;
   pending_transition=1;ok=ok && home_transition() && tr2_home_check(0);
   ok=ok && tr2_home_assault_test();
   ok=ok && tr2_home_scores()->times[0]==151 && tr2_home_scores()->finishes[0]==1 && tr2_home_score_test();
   if(ok && shot){title_key(VK_ESCAPE);test_settle();title_key(VK_RETURN);test_settle();title_render();ok=page_ready() && capture_image("build/tr2-stopwatch-times.ppm");title_key(VK_ESCAPE);test_settle();title_key(VK_ESCAPE);test_settle();ok=ok && home_transition();}
   if(ok && shot){tr2_home_render(width,height);ok=capture_image(shot);}
   ok=ok && tr2_home_water_test();if(ok && shot){for(int style=0;style<2 && ok;style++){char image[96];tr2_home_water_style(style);tr2_home_render(width,height);snprintf(image,sizeof image,"build/tr2-water-style-%d.ppm",style);ok=capture_image(image) && glGetError()==GL_NO_ERROR;}tr2_home_water_style(title_water_style);tr2_home_render(width,height);ok=capture_image("build/tr2-water-entry.ppm");for(int n=1;n<4 && ok;n++){char path[96];tr2_home_resolution(n);tr2_home_render(width,height);snprintf(path,sizeof path,"build/tr2-resolution-water-%d.ppm",n);ok=capture_image(path) && glGetError()==GL_NO_ERROR;}tr2_home_resolution(title_resolution);}
   ok=ok && tr2_home_flare_pickup_test();
   SendMessageA(host->window,WM_KEYDOWN,(WPARAM)control_keys[10],1);ok=ok && (home_input()&0x80000);
   SendMessageA(host->window,WM_KEYUP,(WPARAM)control_keys[10],1);ok=ok && !(home_input()&0x80000);
   title_key(VK_ESCAPE);test_settle();ok=ok && ring_count()==2;
   title_key(control_keys[2]);test_settle();ok=ok && ring_id(title_selection)==173;
   if(ok && shot){title_render();ok=capture_image("build/tr2-flare-inventory.ppm");}
   title_key(VK_RETURN);test_settle();ok=ok && pending_transition==2 && home_transition() && home_playing;
   ok=ok && tr2_home_flare_use_test();
   if(ok && shot){tr2_home_render(width,height);ok=capture_image("build/tr2-flare-held.ppm");}
   ok=ok && tr2_home_presentation_test(0);
   if(ok && shot)ok=capture_image("build/tr2-health-air.ppm");
   ok=ok && tr2_home_presentation_test(1);
   if(ok){title_key(VK_ESCAPE);ok=home_playing;pending_transition=3;ok=ok && home_transition() && !home_session;pending_transition=1;ok=ok && home_transition() && !tr2_home_dead() && tr2_home_check(0);}
   fprintf(stderr,"TR2 health/air render, death timing/music stop/title return/fresh Home: %s\n",ok?"PASS":"FAIL");
   {const int rates[]={30,60,144,144};uint64_t reference=0;
    for(int n=0;n<4 && ok;n++){tr2_home_resolution(n==3?3:title_resolution);pending_transition=1;ok=home_transition();uint64_t digest=0;ok=ok && tr2_home_interpolation_test(rates[n],&digest);if(n)ok=ok && digest==reference;else reference=digest;
     if(ok && shot && n==2)ok=capture_image("build/tr2-interpolation.ppm");}
    fprintf(stderr,"TR2 identical gameplay at 30/60/144 rendered FPS: %s\n",ok?"PASS":"FAIL");}
   if(ok && shot){for(int n=0;n<4 && ok;n++){char path[96];tr2_home_resolution(n);tr2_home_render(width,height);snprintf(path,sizeof path,"build/tr2-resolution-home-%d.ppm",n);ok=capture_image(path) && glGetError()==GL_NO_ERROR;}tr2_home_resolution(title_resolution);}
   if(ok && shot){
    RECT saved,target={0,0,1280,720},client;GetWindowRect(host->window,&saved);
    AdjustWindowRectEx(&target,(DWORD)GetWindowLongPtrA(host->window,GWL_STYLE),FALSE,(DWORD)GetWindowLongPtrA(host->window,GWL_EXSTYLE));
    SetWindowPos(host->window,NULL,0,0,target.right-target.left,target.bottom-target.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    GetClientRect(host->window,&client);width=client.right;height=client.bottom;
    for(int n=0;n<4 && ok;n++){char path[96];tr2_home_resolution(n);tr2_home_render(width,height);snprintf(path,sizeof path,"build/tr2-resolution-wide-%d.ppm",n);ok=capture_image(path) && glGetError()==GL_NO_ERROR;}
    {int resolution=title_resolution;
     for(int n=0;n<4 && ok;n++){title_resolution=n;tr2_home_resolution(n);ok=aspect_render_test(1);char path[96];snprintf(path,sizeof path,"build/tr2-force43-wide-%d.ppm",n);if(ok)ok=capture_image(path);ok=ok && aspect_render_test(0);}
     title_resolution=resolution;tr2_home_resolution(resolution);
     target=(RECT){0,0,600,900};AdjustWindowRectEx(&target,(DWORD)GetWindowLongPtrA(host->window,GWL_STYLE),FALSE,(DWORD)GetWindowLongPtrA(host->window,GWL_EXSTYLE));
     SetWindowPos(host->window,NULL,0,0,target.right-target.left,target.bottom-target.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
     GetClientRect(host->window,&client);width=client.right;height=client.bottom;
     ok=ok && aspect_render_test(1);if(ok)ok=capture_image("build/tr2-force43-portrait.ppm");ok=ok && aspect_render_test(0);
     fprintf(stderr,"TR2B Force 4:3: all resolutions, wide/portrait borders and unchanged centred pixels: %s\n",ok?"PASS":"FAIL");
    }
    SetWindowPos(host->window,NULL,saved.left,saved.top,saved.right-saved.left,saved.bottom-saved.top,SWP_NOZORDER|SWP_NOACTIVATE);
    GetClientRect(host->window,&client);width=client.right;height=client.bottom;tr2_home_resolution(title_resolution);
   }
   if(ok && shot){int filtering=title_filtering;
    for(int res=0;res<2 && ok;res++)for(int n=0;n<2 && ok;n++){title_filtering=n;filtering_apply();tr2_home_resolution(res?3:0);tr2_home_render(width,height);char path[96];snprintf(path,sizeof path,"build/tr2-filter-home-%d-%d.ppm",res,n);ok=capture_image(path) && filtering_check();}
    title_filtering=filtering;filtering_apply();tr2_home_resolution(title_resolution);
   }
   if(ok)ok=tr2_home_gate_test(0);
   fprintf(stderr,"TR2 Home entry, movement, pause/resume, title return and fresh re-entry: %s\n",ok?"PASS":"FAIL");
  }else if(ok && shot){tr2_home_render(width,height);ok=capture_image(shot);}
 }
 if(ok && shot && page!=6 && !home_test && !direct_home){test_settle();if(page){title_selection=page==3?3:page==4?1:page==5?4:page==7?2:0;title_angle=title_selection*2*PI/TITLE_ITEMS;if(page!=7){title_key(VK_RETURN);test_settle();if(page==2){title_key(VK_RIGHT);test_settle();}}}title_render();ok=capture_image(shot);}
 LARGE_INTEGER frequency,previous_time,now,last_present={0};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&previous_time);int active=1,timer_resolution=0;
 while(ok && running && !shot && !test && !home_test){
  int focused=pump();QueryPerformanceCounter(&now);double dt=(double)(now.QuadPart-previous_time.QuadPart)/frequency.QuadPart;previous_time=now;
  if(focused!=active && home_session)tr2_home_interpolation_reset();
  if(focused!=active && audio_device){if(focused)waveOutRestart(audio_device);else waveOutPause(audio_device);}active=focused;
  if(home_session)tr2_audio_pause(!focused);
  /* Sleep(1) otherwise rounds to the coarse Windows timer interval and
     undershoots the requested rate. Release the request when uncapped/idle. */
  if(focused && title_framerate && !timer_resolution)timer_resolution=timeBeginPeriod(1)==TIMERR_NOERROR;
  else if((!focused || !title_framerate) && timer_resolution){timeEndPeriod(1);timer_resolution=0;}
  if(focused){
   if(home_playing){
    tr2_home_update(fmin(dt,.1),home_input());
    campaign_events();
    ok=home_transition();if(home_playing)tr2_home_render(width,height);else title_render();
   }
   else{menu_tick(fmin(dt,.1));ok=home_transition();audio_pump();if(home_session){tr2_audio_volume(title_music,title_effects);tr2_audio_pump();}if(home_playing)tr2_home_render(width,height);else title_render();}
   tomb_frame_limit_wait(last_present,title_framerate,tomb_presentation_sync(1));
   SwapBuffers(host->dc);QueryPerformanceCounter(&last_present);
  }
  if(!focused){last_present.QuadPart=0;MsgWaitForMultipleObjectsEx(0,NULL,50,QS_ALLINPUT,MWMO_INPUTAVAILABLE);}
 }
 if(timer_resolution)timeEndPeriod(1);
 if(test || home_test)memcpy(config_path,saved_config,sizeof config_path);
 fprintf(stderr,"TR2 GL status: %u\n",(unsigned)glGetError());tr2_home_close();home_session=home_playing=0;audio_close();title_close();SetWindowLongPtrA(host->window,GWLP_WNDPROC,previous);host=NULL;return ok?0:1;
}
