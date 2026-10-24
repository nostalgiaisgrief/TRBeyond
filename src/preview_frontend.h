/* Win32/GL adapter for the original option ring and passport.
   Included after font/ring helpers; all gameplay remains in the C runtime. */
static void frontend_free(void){
 if(title_visual){glDeleteTextures((GLsizei)(title_visual->tile_count*2+1),title_tiles);glDeleteTextures((GLsizei)title_visual->tile_count,title_font_tiles);}
 free(title_tiles);free(title_font_tiles);tomb_visual_free(title_visual);tomb_level_free(title_level);glDeleteTextures(1,&title_backdrop);
}
static int frontend_assets(void){
 if(title_visual)return 1;
 char path[MAX_PATH],error[256];asset_path(path,"TITLE.PHD");
 title_level=tomb_level_load(path,error,sizeof error);if(!title_level)return 0;
 title_visual=tomb_visual_load(title_level,error,sizeof error);if(!title_visual)return 0;
 TombLevel *saved_level=level;TombVisual *saved_visual=visual;GLuint *saved_tiles=tiles,*saved_font=font_tiles;
 level=title_level;visual=title_visual;int ok=make_tiles();title_tiles=tiles;title_font_tiles=font_tiles;
 level=saved_level;visual=saved_visual;tiles=saved_tiles;font_tiles=saved_font;if(!ok)return 0;
 asset_path(path,"TITLEH.PCX");unsigned char *rgb=tomb_picture_load(path);if(!rgb)return 0;
 glGenTextures(1,&title_backdrop);glBindTexture(GL_TEXTURE_2D,title_backdrop);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,1024,512,0,GL_RGB,GL_UNSIGNED_BYTE,rgb);free(rgb);return 1;
}
static void frontend_backdrop(void){
 glEnable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);glBindTexture(GL_TEXTURE_2D,title_backdrop);glColor3f(1,1,1);
 glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2d((width-height*4.0/3)/2,0);glTexCoord2f(640.f/1024,0);glVertex2d((width+height*4.0/3)/2,0);glTexCoord2f(640.f/1024,480.f/512);glVertex2d((width+height*4.0/3)/2,height);glTexCoord2f(0,480.f/512);glVertex2d((width-height*4.0/3)/2,height);glEnd();glDisable(GL_TEXTURE_2D);
}
static const char *frontend_name(int id){switch(id){case 71:case 81:return "Game";case 73:return title_mode?"Lara's Home":"Customisation";case 95:return "Detail Levels";case 96:return "Sound";case 97:return "Controls";default:return tomb_inventory_level_name(level_number,id);}}
/* Title always offers Autosave; gameplay offers it only while enabled. */
static int frontend_autosave_visible(int page){return page==0 && (title_mode || (play.gameplay_options&TOMB_AUTOSAVE));}
static int frontend_slot_count(int page){return 16+!!frontend_autosave_visible(page);}
static int frontend_slot(int page,int row){return frontend_autosave_visible(page)?(row==0?16:row-1):row;}
static int slot_memory_ready,slot_remembered,slot_manual;
static void refresh_slots(void){
 char path[MAX_PATH];FILETIME newest={0};int latest=0;
 for(int i=0;i<17;i++){
  save_path(path,i);save_levels[i]=tomb_save_level(path);
  if(!slot_memory_ready && i<16 && save_levels[i]>=0){
   WIN32_FILE_ATTRIBUTE_DATA data;
   if(GetFileAttributesExA(path,GetFileExInfoStandard,&data) && CompareFileTime(&data.ftLastWriteTime,&newest)>0){newest=data.ftLastWriteTime;latest=i;}
  }
 }
 /* Existing saves have no DOS sequence counter; write time supplies the same
    newest-save ordering without changing their format. Scan only on startup. */
 if(!slot_memory_ready){slot_remembered=slot_manual=latest;slot_memory_ready=1;}
}
static void frontend_remember_slot(int page){
 slot_remembered=frontend_slot(page,slot_current);
 if(slot_remembered<16)slot_manual=slot_remembered;
}
static void frontend_open_slots(int page){
 refresh_slots();int slot=slot_remembered;
 if(slot==16 && !frontend_autosave_visible(page))slot=slot_manual;
 slot_current=frontend_autosave_visible(page)?(slot==16?0:slot+1):slot;
 slot_menu=1;menu_error[0]=0;
}
static void frontend_title(void){
 xr.panel_open=0;
 audio_stop();title_mode=1;death_menu=0;play.quest_request=0;inventory_begin();options_ring=1;tomb_ring_init_options(&inventory_ring,1);inventory_sound();refresh_slots();
}
/* Completion follows the entry route; resetting Home preserves that route. */
static int frontend_home_complete(void){
 if(level_number!=0 || title_mode || !play.gym.complete)return 0;
 accumulator=0;
 if(home_from_level_select){int mode=play.shooter.mode;uint32_t options=play.gameplay_options;if(!load_game_level(1,0,NULL))frontend_title();else{tomb_gameplay_start(&play,options);tomb_shooter_start(&play,mode);level_inventory=play.inventory;}}
 else frontend_title();
 return 1;
}
/* Load the replacement title before discarding the previous source's menu.
   The next New Game/Load constructs a fresh world from the selected source. */
static int frontend_source_apply(void){
 if(source_pending==source_selected){if(music_selected!=music_pending)audio_stop();music_selected=music_pending;fmv_selected=fmv_pending;return 1;}
 int previous=source_selected;
 TombLevel *old_level=title_level;TombVisual *old_visual=title_visual;
 GLuint *old_tiles=title_tiles,*old_font=title_font_tiles,old_backdrop=title_backdrop;
 title_level=NULL;title_visual=NULL;title_tiles=title_font_tiles=NULL;title_backdrop=0;
 source_selected=source_pending;
 if(!frontend_assets()){
  frontend_free();title_level=old_level;title_visual=old_visual;title_tiles=old_tiles;title_font_tiles=old_font;title_backdrop=old_backdrop;
  source_selected=source_pending=previous;music_pending=music_selected;fmv_pending=fmv_selected;strcpy(menu_error,"Could not load source");return 0;
 }
 if(old_visual){glDeleteTextures((GLsizei)(old_visual->tile_count*2+1),old_tiles);glDeleteTextures((GLsizei)old_visual->tile_count,old_font);}
 free(old_tiles);free(old_font);tomb_visual_free(old_visual);tomb_level_free(old_level);glDeleteTextures(1,&old_backdrop);
 audio_stop();music_selected=music_pending;fmv_selected=fmv_pending;slot_memory_ready=0;refresh_slots();return 1;
}
static void frontend_execute(int id){
 char path[MAX_PATH];int action=menu_action;menu_action=0;
 if(id==73){if(title_mode)load_game_level(0,0,NULL);else if(action==4){if(level_number==0)play.gym.complete=1;else objects.progress->complete=1;}return;}
 if(action==1){int slot=frontend_slot(0,slot_current);save_path(path,slot);if(!load_game_level(save_levels[slot],0,path)){inventory_begin();options_ring=1;tomb_ring_init_options(&inventory_ring,title_mode?1:2);}}
 else if(action==2){if(title_mode || (level_number==0 && !passport_save)){if(!load_game_level(new_game_level,0,NULL)){inventory_begin();options_ring=1;tomb_ring_init_options(&inventory_ring,title_mode?1:2);}else {home_from_level_select=level_number==0;tomb_gameplay_start(&play,new_game_options);tomb_shooter_start(&play,new_game_mode);tomb_vr_reset(&vr,&play);if(new_game_mode!=TOMB_MODE_VR && xr.instance)xr_close();camera.ready=0;update_camera(1.0/30);level_inventory=play.inventory;tomb_pickups_replace_owned(&objects,&play.inventory);}}else{save_path(path,slot_current);if(!tomb_save_write(path,level_number,&play,&objects,&camera,&level_inventory)){inventory_begin();strcpy(menu_error,"Could not save game");}}}
 else if(action==3){if(title_mode)running=0;else frontend_title();}
}
static int customise_page;
/* Actions: -1 cycles gun power, -2 skips the level, 1..4 grant weapons. */
typedef struct FrontendGameplayOption { const char *name; uint32_t bit; int action; } FrontendGameplayOption;
static const char *customise_pages[]={"QUALITY OF LIFE","CHEATS","FUN STUFF"};
static const FrontendGameplayOption customise_quality[]={
 {"Autosave",TOMB_AUTOSAVE,0},{"Consistent Weapons",TOMB_CONSISTENT_WEAPONS,0},{"3D Pickups",TOMB_3D_PICKUPS,0}};
static const FrontendGameplayOption customise_cheats_title[]={
 {"Invincibility",TOMB_INVINCIBLE,0},{"Infinite Ammo",TOMB_INFINITE_AMMO,0},{"Gun Power",0,-1},
 {"Start with Shotgun",TOMB_START_SHOTGUN,0},{"Start with Magnums",TOMB_START_MAGNUMS,0},{"Start with Uzis",TOMB_START_UZIS,0}};
static const FrontendGameplayOption customise_cheats_game[]={
 {"Skip Level",0,-2},{"Invincibility",TOMB_INVINCIBLE,0},{"Infinite Ammo",TOMB_INFINITE_AMMO,0},{"Gun Power",0,-1},
 {"Give Pistol",0,1},{"Give Shotgun",0,2},{"Give Magnums",0,3},{"Give Uzis",0,4}};
static const FrontendGameplayOption customise_fun[]={
 {"No Enemies",TOMB_NO_ENEMIES,0},{"Double-Tap Jump to Swim",TOMB_DOUBLE_JUMP_SWIM,0},
 {"Max Draw Distance",TOMB_MAX_DRAW_DISTANCE,0},{"Climb Anything in VR",TOMB_VR_CLIMB_ANYTHING,0}};
static const FrontendGameplayOption *frontend_gameplay_options(int starting,int *count){
 if(customise_page==0){*count=3;return customise_quality;}
 if(customise_page==2){*count=4;return customise_fun;}
 *count=starting?6:8;return starting?customise_cheats_title:customise_cheats_game;
}
static const char *gun_power_names[]={"1x","2x","5x","One Shot Kill"};
static void frontend_gun_power(uint32_t *options,unsigned input){
 int mode=(*options&TOMB_GUN_POWER_MASK)>>TOMB_GUN_POWER_SHIFT;
 mode=(mode+((input&TOMB_RING_LEFT)?3:1))%4;
 *options=(*options&~(uint32_t)TOMB_GUN_POWER_MASK)|((uint32_t)mode<<TOMB_GUN_POWER_SHIFT);
}
static void frontend_gameplay_input(int starting,int *row,unsigned input){
 int count;const FrontendGameplayOption *options=frontend_gameplay_options(starting,&count);
 if((input&16) && *row>=0)--*row;
 if((input&32) && *row<count-1+starting)++*row;
 if(*row<0){
  if(input&(TOMB_RING_LEFT|TOMB_RING_RIGHT))customise_page=(customise_page+((input&TOMB_RING_RIGHT)?1:2))%3;
  return;
 }
 if(starting && *row==count){if(input&TOMB_RING_SELECT)new_game_menu=4;return;}
 if(!(input&(TOMB_RING_SELECT|TOMB_RING_LEFT|TOMB_RING_RIGHT)))return;
 const FrontendGameplayOption *o=options+*row;uint32_t *bits=starting?&new_game_options:&play.gameplay_options;
 if(o->action==-1)frontend_gun_power(bits,input);
 else if(o->bit){
  *bits^=o->bit;
  if(!starting && o->bit==TOMB_DOUBLE_JUMP_SWIM && !(*bits&TOMB_DOUBLE_JUMP_SWIM)){play.swim_override=0;play.swim_tap_ticks=0;}
 }else if(!starting && (input&TOMB_RING_SELECT)){
  if(o->action==-2){menu_action=4;tomb_ring_finish_item(&inventory_ring,73);}
  else if(o->action>0){
   int gun=o->action-1;
   if(!gun)play.inventory.counts[0]=1;
   else {tomb_inventory_add(&play.inventory,play.inventory.counts[gun]?88+gun:84+gun);tomb_pickups_replace_owned(&objects,&play.inventory);}
   inventory_ring.sound=114;inventory_sound();
  }
 }
}
static const int frontend_binding_order[13]={0,1,2,3,4,5,6,7,8,9,11,10,12};
static int frontend_input(unsigned *input){
 TombRing *r=&inventory_ring;TombRingItem *i=r->items+r->current;
 if(passport_shortcut){
  if(r->motion.status==1 && !r->rotating)*input=TOMB_RING_SELECT;
  else if(r->motion.status==8 && r->ready){
   int frame=passport_shortcut==2?19:14;
   if(i->frame!=frame){i->goal=(int16_t)frame;i->direction=i->frame<frame?1:-1;r->ready=0;}
   else{frontend_open_slots(passport_shortcut==2?1:0);passport_shortcut=0;}
   *input=0;return 0;
  }else *input=0;
 }
 if(r->motion.status==1 && (*input&TOMB_RING_SELECT)){option_row=(i->object==97 || (i->object==73 && !title_mode))?-1:0;if(i->object==73 && !title_mode)customise_page=0;}
 if(death_menu && r->motion.status==1 && !r->rotating){r->current=0;*input=TOMB_RING_SELECT;}
 if(r->motion.status!=8 || !r->ready)return 0;
 if(i->object==71){
  int page=(i->frame-i->open_frame)/5;
  if(new_game_menu && new_game_menu<4){
   if(new_game_menu==3){
    if(*input&TOMB_RING_BACK){new_game_menu=2;new_game_row=0;}
    else frontend_gameplay_input(1,&new_game_row,*input);
    *input=0;return 0;
   }
   int rows=new_game_menu==1?4:2;
   if(*input&16 && new_game_row>0)--new_game_row;
   if(*input&32 && new_game_row<rows-1)++new_game_row;
   if(*input&TOMB_RING_BACK){--new_game_menu;new_game_row=0;}
   else if(*input&TOMB_RING_SELECT){
    if(new_game_menu==1){
     if(new_game_row==TOMB_MODE_VR && !xr_open()){snprintf(menu_error,sizeof menu_error,"%.95s",xr.error);}
     else{new_game_mode=new_game_row;new_game_menu=2;new_game_row=0;menu_error[0]=0;}
    }
    else if(new_game_menu==2){new_game_custom=new_game_row!=0;if(new_game_custom){new_game_menu=3;new_game_row=-1;customise_page=0;}else{new_game_options=0;new_game_menu=4;}}
   }
   *input=0;return 0;
  }
  if(new_game_menu==4){
   if(*input&16 && new_game_level>0)new_game_level--;
   if(*input&32 && new_game_level<LEVEL_CHOICE_COUNT-1)new_game_level++;
   if(*input&TOMB_RING_BACK){new_game_menu=new_game_custom?3:2;new_game_row=new_game_custom?-1:0;}
   else if(*input&TOMB_RING_SELECT){new_game_menu=0;menu_action=2;tomb_ring_finish_item(r,71);}
   *input=0;return 0;
  }
  if(slot_menu){
   if(*input&16 && slot_current>0)slot_current--;if(*input&32 && slot_current<(frontend_slot_count(page)-1))slot_current++;
   frontend_remember_slot(page);
   if(*input&TOMB_RING_BACK){slot_menu=0;menu_error[0]=0;}
  else if(*input&TOMB_RING_SELECT){if(page==1 || save_levels[frontend_slot(page,slot_current)]>=0){menu_action=page+1;slot_menu=0;tomb_ring_finish_item(r,71);}else strcpy(menu_error,"Empty Slot");}
   *input=0;return 0;
  }
  /* When there are no saves DOS skips the load page. */
  int any=0;for(int n=0;n<frontend_slot_count(0);n++)if(save_levels[frontend_slot(0,n)]>=0)any=1;
  if(page==0 && !any){i->goal=death_menu?24:19;i->direction=1;r->ready=0;*input=0;return 0;}
  int direction=(*input&TOMB_RING_RIGHT)?1:(*input&TOMB_RING_LEFT)?-1:0;
  if(direction){int next=page+direction;if(next>=(!any?1:0) && next<=2 && !(death_menu && next==1)){i->goal=(int16_t)(14+next*5);i->direction=(int16_t)direction;r->ready=0;r->sound=115;inventory_sound();}else if(death_menu && next==1){i->goal=(int16_t)(direction>0?24:14);i->direction=(int16_t)direction;r->ready=0;}*input=0;}
  else if(death_menu && (*input&TOMB_RING_BACK)){*input=0;}
  else if(*input&TOMB_RING_SELECT){
   if(page==0 || (page==1 && !title_mode && (level_number!=0 || passport_save))){frontend_open_slots(page);}
   else if(page==1 && (title_mode || (level_number==0 && !passport_save))){new_game_menu=1;new_game_row=0;new_game_options=0;new_game_custom=0;new_game_mode=0;new_game_level=1;menu_error[0]=0;}
   else{menu_action=page+1;tomb_ring_finish_item(r,71);}*input=0;
  }
 }else if(i->object==73 && !title_mode){
  if(*input&TOMB_RING_BACK)tomb_ring_finish_item(r,-1);
  else frontend_gameplay_input(0,&option_row,*input);
  *input=0;
 }else if(i->object==73){
  if(*input&TOMB_RING_SELECT){tomb_ring_finish_item(r,73);*input=0;}
 }
 else if(i->object>=95 && i->object<=97){
  int rows=i->object==95?(title_mode?7:4):i->object==96?2:13;
  if(i->object==95){if(*input&16 && option_row>0)option_row--;if(*input&32 && option_row<rows-1)option_row++;}
  else if(i->object==97){
   if(option_row<0){if(*input&(TOMB_RING_LEFT|TOMB_RING_RIGHT))controls_page=(controls_page+((*input&TOMB_RING_RIGHT)?1:3))%4;if(*input&32)option_row=0;}
   else if(controls_page==3){if(*input&16)--option_row;if(*input&TOMB_RING_LEFT && settings_vr_deadzone>0)--settings_vr_deadzone;if(*input&TOMB_RING_RIGHT && settings_vr_deadzone<50)++settings_vr_deadzone;}
   else if(controls_page){if(*input&16)--option_row;if((*input&32) && option_row<(controls_page==2?9:8))++option_row;}
   else {
    int row=0;while(row<12 && frontend_binding_order[row]!=option_row)row++;
    if(*input&16)--row;if((*input&32) && row<12)++row;
    if(row>=0 && (*input&(TOMB_RING_LEFT|TOMB_RING_RIGHT)))row=row<7?(row==6?12:row+7):(row==12?6:row-7);
    option_row=row<0?-1:frontend_binding_order[row];
   }
  }else {if(*input&16 && option_row>0)option_row--;if(*input&32 && option_row<rows-1)option_row++;}
  if(i->object==95 && (*input&(TOMB_RING_LEFT|TOMB_RING_RIGHT))){
   int forward=(*input&TOMB_RING_RIGHT)!=0;
   if(option_row==0)settings_detail=(settings_detail+(forward?2:1))%3;
   else if(option_row==1)settings_framerate=(settings_framerate+(forward?1:2))%3;
   else if(option_row==2)settings_vsync=!settings_vsync;
   else if(option_row==3)settings_force43=!settings_force43;
   else if(title_mode && option_row==4)source_pending=source_cycle(source_pending,forward?1:-1);
   else if(title_mode && option_row==5)music_pending=source_cycle_mask(music_pending,forward?1:-1,music_available);
   else if(title_mode && option_row==6)fmv_pending=source_cycle_mask(fmv_pending,forward?1:-1,fmv_available);
  }
  if(i->object==96){int *v=option_row?&settings_sound:&settings_music;if(*input&TOMB_RING_LEFT && *v>0)--*v;if(*input&TOMB_RING_RIGHT && *v<10)++*v;}
  if(i->object==97 && (*input&TOMB_RING_SELECT)){if(option_row<0 || controls_page==3){*input=0;return 0;}binding_wait=1;*input=0;return 0;}
  if(*input&(TOMB_RING_SELECT|TOMB_RING_BACK)){if(title_mode && i->object==95)frontend_source_apply();settings_write();tomb_ring_finish_item(r,-1);option_row=0;}
  *input=0;
 }
 return 0;
}
/* DOS 1a21c maps a frozen indexed frame through light-map rows 16..24.
   The modern RGB framebuffer is quantized once to the original palette. */
/* RGB readback loses palette identity. Preserve black instead of remapping it
   through the reserved transparent index's sometimes-coloured shade entries. */
static int frontend_dark_index(const unsigned char *palette,const unsigned char *light,int index){
 return (palette[index*3]|palette[index*3+1]|palette[index*3+2])?light[index]:index;
}
static void frontend_inventory_backdrop(void){
 if(xr.panel_drawing)return;
 int cinema=objects.progress->complete && cinematic_pixels;
 const unsigned char *palette=cinema?cinematic_palette:visual->palette;
 static GLuint texture;static unsigned char *indices,*rgb,*original;static int w,h,pw,ph,shade=-1;
 int full_colour=visual->rgba_tiles!=NULL;
 if(w!=width || h!=height || backdrop_dirty){
  free(indices);free(rgb);free(original);original=NULL;w=width;h=height;pw=ph=1;while(pw<w)pw*=2;while(ph<h)ph*=2;
  indices=malloc((size_t)w*h);rgb=calloc((size_t)pw*ph,3);unsigned char *pixels=malloc((size_t)w*h*3);
  if(!indices || !rgb || !pixels){free(pixels);return;}
  unsigned char lut[32768];
  if(!full_colour)for(int key=0;key<32768;key++){int best=0,dist=INT_MAX;for(int i=0;i<256;i++){int d=0;for(int c=0;c<3;c++){int value=((key>>(10-c*5))&31)*255/31-palette[i*3+c]*255/63;d+=value*value;}if(d<dist){dist=d;best=i;}}lut[key]=(unsigned char)best;}
  glPixelStorei(GL_PACK_ALIGNMENT,1);if(cinema){for(int y=0;y<h;y++)for(int x=0;x<w;x++)memcpy(pixels+((size_t)y*w+x)*3,cinematic_pixels+((size_t)(y*cinematic_height/h)*cinematic_width+x*cinematic_width/w)*3,3);}
  else glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels);
  if(full_colour)original=pixels;
  else {for(size_t n=0;n<(size_t)w*h;n++)indices[n]=lut[((pixels[n*3]>>3)<<10)|((pixels[n*3+1]>>3)<<5)|(pixels[n*3+2]>>3)];free(pixels);}
  if(!texture)glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,pw,ph,0,GL_RGB,GL_UNSIGNED_BYTE,rgb);shade=-1;backdrop_dirty=0;
 }
 if(!indices || !rgb)return;
 int row=objects.progress->complete?24:backdrop_shade/2;
 glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);
 if(shade!=row){const unsigned char *light=cinema?cinematic_light:level->file_data+level->item_parsed_bytes+row*256;
  if(full_colour && original){for(int y=0;y<h;y++)for(int x=0;x<w;x++)for(int c=0;c<3;c++)rgb[((size_t)y*pw+x)*3+c]=(unsigned char)(original[((size_t)y*w+x)*3+c]*(32-row)/16);}
  else for(int y=0;y<h;y++)for(int x=0;x<w;x++){int index=indices[y*w+x];if(row!=16)index=frontend_dark_index(palette,light,index);for(int c=0;c<3;c++)rgb[((size_t)y*pw+x)*3+c]=(unsigned char)(palette[index*3+c]*255/63);}
  glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,pw,ph,GL_RGB,GL_UNSIGNED_BYTE,rgb);shade=row;
 }
 glColor3f(1,1,1);glBegin(GL_QUADS);glTexCoord2f(0,(float)h/ph);glVertex2i(0,0);glTexCoord2f((float)w/pw,(float)h/ph);glVertex2i(w,0);glTexCoord2f((float)w/pw,0);glVertex2i(w,h);glTexCoord2f(0,0);glVertex2i(0,h);glEnd();glDisable(GL_TEXTURE_2D);
}
/* DOS translucent panels use light-map row 24 (1fa5c), not an opaque fill.
   Quantize the small RGB panel region to the supplied palette for that lookup. */
static void frontend_panel_shade(int x,int y,int w,int h) {
 static unsigned char lut[32768],palette[768];static int valid;static GLuint texture;
 if(!visual->rgba_tiles && (!valid || memcmp(palette,visual->palette,sizeof palette))) {
  memcpy(palette,visual->palette,sizeof palette);valid=1;
  for(int key=0;key<32768;key++){int best=0,dist=INT_MAX;for(int n=0;n<256;n++){int d=0;for(int c=0;c<3;c++){int delta=((key>>(10-5*c))&31)*255/31-palette[3*n+c]*255/63;d+=delta*delta;}if(d<dist){dist=d;best=n;}}lut[key]=(unsigned char)best;}
 }
 if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}if(x+w>width)w=width-x;if(y+h>height)h=height-y;if(w<=0 || h<=0)return;
 int pw=1,ph=1;while(pw<w)pw*=2;while(ph<h)ph*=2;
 unsigned char *pixels=malloc((size_t)w*h*3),*rgb=calloc((size_t)pw*ph,3);if(!pixels || !rgb){free(pixels);free(rgb);return;}
 /* The caller selects the read buffer: GL_BACK on desktop, COLOR_ATTACHMENT0
    for an OpenXR panel. GL_BACK is invalid on a bound headset framebuffer. */
 glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(x,height-y-h,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels);
 const TombLevel *panel_level=visual->level;
 const unsigned char *light=panel_level->file_data+panel_level->item_parsed_bytes+24*256;
 if(visual->rgba_tiles){for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++)for(int c=0;c<3;c++)rgb[((size_t)yy*pw+xx)*3+c]=pixels[((size_t)yy*w+xx)*3+c]/2;}
 else for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++){const unsigned char *p=pixels+((size_t)yy*w+xx)*3;int index=frontend_dark_index(palette,light,lut[((p[0]>>3)<<10)|((p[1]>>3)<<5)|(p[2]>>3)]);for(int c=0;c<3;c++)rgb[((size_t)yy*pw+xx)*3+c]=(unsigned char)(palette[index*3+c]*255/63);}
 if(!texture)glGenTextures(1,&texture);glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,pw,ph,0,GL_RGB,GL_UNSIGNED_BYTE,rgb);glColor3f(1,1,1);
 glBegin(GL_QUADS);glTexCoord2f(0,(float)h/ph);glVertex2i(x,y);glTexCoord2f((float)w/pw,(float)h/ph);glVertex2i(x+w,y);glTexCoord2f((float)w/pw,0);glVertex2i(x+w,y+h);glTexCoord2f(0,0);glVertex2i(x,y+h);glEnd();glDisable(GL_TEXTURE_2D);free(pixels);free(rgb);
}
static void frontend_edge(double x,double y,double w,double h,int index) {
 double scale=fmax(.5,fmin(width/640.0,height/480.0));
 colour((unsigned)index,1);glBegin(GL_QUADS);glVertex2d(x,y);glVertex2d(x+w*scale,y);glVertex2d(x+w*scale,y+h*scale);glVertex2d(x,y+h*scale);glEnd();
}
static void frontend_panel(const TombDialogEntry *entry,const char *text) {
 TombTextStyle style={0,0,65536,65536,1,6,0};int bounds[4];tomb_dialog_bounds(entry,tomb_text_width(text,&style),bounds);
 double scale=fmax(.5,fmin(width/640.0,height/480.0)),x=(width-640*scale)/2+bounds[0]*scale;
 double y=(entry->flags&TOMB_TEXT_BOTTOM)?height-480*scale+bounds[1]*scale:(height-480*scale)/2+bounds[1]*scale;
 int w=bounds[2],h=bounds[3];frontend_panel_shade((int)x,(int)y,(int)(w*scale),(int)(h*scale));
 /* Eight one-pixel bevel lines, palette 15/31, from DOS 1f938. */
 frontend_edge(x,y-scale,w+1,1,15);frontend_edge(x,y,w,1,31);
 frontend_edge(x+w*scale,y,1,h+1,15);frontend_edge(x+(w+1)*scale,y,1,h+2,31);
 frontend_edge(x,y+h*scale,w+1,1,15);frontend_edge(x-scale,y+(h+1)*scale,w+2,1,31);
 frontend_edge(x-scale,y-scale,1,h+2,15);frontend_edge(x,y,1,h+1,31);
}
static void frontend_entry(const TombDialogEntry *entry,const char *text,int selected) {
 if(entry->panel_width || selected)frontend_panel(entry,text);
 if(entry->flags&TOMB_TEXT_CENTRE)ui_text(entry->x,entry->y,text,(unsigned)entry->flags);
 else {double scale=fmax(.5,fmin(width/640.0,height/480.0));text_aligned((float)((width-640*scale)/2+entry->x*scale),(float)(entry->y*scale),text,(unsigned)entry->flags);}
}
/* Use the original font's horizontal arrows around the mode selector. */
static void frontend_control_mode(const TombDialogEntry *entry,const char *name,int selected) {
 frontend_entry(entry,name,selected);
 double scale=fmax(.5,fmin(width/640.0,height/480.0));
 TombTextStyle style={0,0,(int32_t)(scale*65536),(int32_t)(scale*65536),1,6,0};
 int span=tomb_text_width(name,&style),left=(width-span)/2+(int)(entry->x*scale);
 int y=height/2+(int)(entry->y*scale);
 TombTextGlyph arrow={108,left-(int)(24*scale),y,style.scale_x,style.scale_y};
 glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glColor3f(1,1,1);
 font_glyph(NULL,&arrow);arrow.glyph=109;arrow.x=left+span+(int)(10*scale);font_glyph(NULL,&arrow);
 glDisable(GL_ALPHA_TEST);glDisable(GL_TEXTURE_2D);
}
static void frontend_gameplay_overlay(int starting,int selected){
 int count;const FrontendGameplayOption *options=frontend_gameplay_options(starting,&count);
 int rows=count+(starting?2:0),top=-30-rows*9;
 TombDialogEntry panel={0,top,TOMB_TEXT_CENTRE|TOMB_TEXT_MIDDLE,400,64+rows*18};
 frontend_entry(&panel," ",0);
 TombDialogEntry heading={0,top+14,TOMB_TEXT_CENTRE|TOMB_TEXT_MIDDLE,0,0};
 frontend_control_mode(&heading,customise_pages[customise_page],selected<0);
 uint32_t bits=starting?new_game_options:play.gameplay_options;
 for(int row=0;row<count+starting;row++){
  char text[100];
  if(row==count)snprintf(text,sizeof text,"Start Game");
  else if(options[row].action==-1)snprintf(text,sizeof text,"Gun Power: %s",gun_power_names[(bits&TOMB_GUN_POWER_MASK)>>TOMB_GUN_POWER_SHIFT]);
  else if(options[row].bit)snprintf(text,sizeof text,"%s: %s",options[row].name,bits&options[row].bit?"On":"Off");
  else snprintf(text,sizeof text,"%s",options[row].name);
  TombDialogEntry entry={0,top+38+18*(row+(row==count)),TOMB_TEXT_CENTRE|TOMB_TEXT_MIDDLE,row==selected?380:0,16};
  frontend_entry(&entry,text,0);
 }
}
static void frontend_key_name(unsigned key,char out[40]) {
 if(key==VK_LBUTTON || key==VK_RBUTTON || key==VK_MBUTTON){snprintf(out,40,"%s",key==VK_LBUTTON?"Left Click":key==VK_RBUTTON?"Right Click":"Middle Click");return;}
 if(key>=VK_NUMPAD0 && key<=VK_NUMPAD9){snprintf(out,40,"PAD%u",key-VK_NUMPAD0);return;}
 LONG scan=(LONG)(MapVirtualKeyA(key,MAPVK_VK_TO_VSC)<<16);if(key>=VK_PRIOR && key<=VK_DOWN)scan|=1L<<24;
 if(!GetKeyNameTextA(scan,out,40))snprintf(out,40,"%u",key);
}
static void frontend_overlay(void){
 TombRing *r=&inventory_ring;TombRingItem *i=r->items+r->current;
 if(r->motion.status!=8 || !r->ready)return;
 TombDialogEntry entries[32];
 if(i->object==73 && !title_mode){
  frontend_gameplay_overlay(0,option_row);return;
 }
 if(i->object==71){
  if(new_game_menu && new_game_menu<4){
   static const char *modes[]={"Original","Third Person Shooter","First Person Shooter","Virtual Reality"};
   if(new_game_menu==3){frontend_gameplay_overlay(1,new_game_row);return;}
   int rows=new_game_menu==1?4:2;
   tomb_dialog_layout(71,new_game_row,1,entries);
   entries[0].panel_width=380;entries[0].panel_height=40+18*rows;entries[1].panel_width=376;
   frontend_entry(entries," ",0);frontend_entry(entries+1,new_game_menu==1?"Select Mode":new_game_menu==2?"Select Gameplay":"Customise Gameplay",0);
   for(int row=0;row<rows;row++){char text[100];
    entries[row+2]=(TombDialogEntry){0,-270+18*row,TOMB_TEXT_CENTRE|TOMB_TEXT_BOTTOM,row==new_game_row?360:0,16};
    if(new_game_menu==1)snprintf(text,sizeof text,"%s",modes[row]);
    else if(new_game_menu==2)snprintf(text,sizeof text,"%s",row?"Customise...":"Play with Defaults");
    if(entries[row+2].panel_width)entries[row+2].panel_width=360;frontend_entry(entries+row+2,text,0);
   }if(menu_error[0])ui_text(0,-40,menu_error,TOMB_TEXT_CENTRE|TOMB_TEXT_BOTTOM);return;
  }
  if(new_game_menu==4){
   int first=new_game_level>=10?new_game_level-9:0;
   tomb_dialog_layout(71,new_game_level-first,1,entries);
   frontend_entry(entries," ",0);frontend_entry(entries+1,"Choose Starting Level",0);
   for(int row=0;row<10 && first+row<LEVEL_CHOICE_COUNT;row++)frontend_entry(entries+row+2,level_choices[first+row].name,0);
   if(first>0)ui_text(0,-282,"[",TOMB_TEXT_CENTRE|TOMB_TEXT_BOTTOM);
   if(first+10<LEVEL_CHOICE_COUNT)ui_text(0,-88,"]",TOMB_TEXT_CENTRE|TOMB_TEXT_BOTTOM);
   return;
  }
  int page=(i->frame-14)/5;
  const char *label=page==0?"Load Game":page==1?(title_mode || (level_number==0 && !passport_save)?"New Game":"Save Game"):(title_mode?"Exit Game":"Exit to Title");
  int first=slot_current>=10?slot_current-9:0;
  tomb_dialog_layout(71,slot_current-first,slot_menu,entries);
  if(!slot_menu)frontend_entry(entries,label,0);
  else {
   frontend_entry(entries," ",0);frontend_entry(entries+1,label,0);
   for(int row=0;row<10 && first+row<frontend_slot_count(page);row++){int n=frontend_slot(page,first+row);char line[96];if(n==16)snprintf(line,sizeof line,"Autosave - %s",save_levels[n]>=0?level_choices[save_levels[n]].name:"Empty Slot");else snprintf(line,sizeof line,"%02d %s",n+1,save_levels[n]>=0?level_choices[save_levels[n]].name:"Empty Slot");frontend_entry(entries+row+2,line,0);}
   if(first>0)ui_text(0,-282,"[",TOMB_TEXT_CENTRE|TOMB_TEXT_BOTTOM);
   if(first+10<frontend_slot_count(page))ui_text(0,-88,"]",TOMB_TEXT_CENTRE|TOMB_TEXT_BOTTOM);
  }
 }else if(i->object==95 || i->object==96){
  int detail=i->object==95,rows=detail?(title_mode?7:4):2;
  tomb_dialog_layout(i->object,option_row,0,entries);
  if(detail){entries[0].panel_height=title_mode?252:132;entries[5]=entries[4];entries[5].y=75;entries[5].panel_width=option_row==3?288:0;entries[0].panel_width=300;entries[1].panel_width=296;for(int row=0;row<3;row++)if(entries[row+2].panel_width)entries[row+2].panel_width=288;}
  if(detail && title_mode){
   entries[0].panel_width=460;entries[1].panel_width=456;entries[0].y=-112;entries[1].y=-110;
   for(int row=0;row<7;row++){entries[row+2]=entries[2];entries[row+2].y=-80+30*row;entries[row+2].panel_width=option_row==row?448:0;}
  }
  frontend_entry(entries," ",0);frontend_entry(entries+1,detail?"Detail Levels":"Set Volumes",0);
  for(int row=0;row<rows;row++){char line[80];if(detail && row>=4)snprintf(line,sizeof line,"%s: %s",row==4?"Level Source":row==5?"Music and Speech Source":"FMV Source",(row==4?source_available:row==5?music_available:fmv_available)?source_names[row==4?source_pending:row==5?music_pending:fmv_pending]:"Unavailable");else if(detail)snprintf(line,sizeof line,"%s: %s",row==0?"Detail":row==1?"Framerate Limit":row==2?"VSync":"Force 4:3",row==0?(settings_detail==2?"High":settings_detail==1?"Medium":"Low"):row==1?(settings_framerate==0?"None":settings_framerate==1?"60":"30"):((row==2?settings_vsync:settings_force43)?"On":"Off"));else snprintf(line,sizeof line,"%c %2d",row==0?'|':'}',row==0?settings_music:settings_sound);frontend_entry(entries+row+2,line,0);}
 }else if(i->object==97){
  /* Preserve user bindings; DOS puts Look before Roll in its right column. */
  tomb_dialog_layout(97,option_row,0,entries);
  if(controls_page==3){
   entries[0].y=-60;entries[0].panel_height=110;entries[1].y=-45;
   frontend_entry(entries," ",0);frontend_control_mode(entries+1,"Virtual Reality",option_row<0);
   TombDialogEntry label={170,-12,TOMB_TEXT_MIDDLE,0,0},value={365,-12,TOMB_TEXT_MIDDLE,0,0};char text[32];snprintf(text,sizeof text,"%d percent",settings_vr_deadzone);
   frontend_entry(&label,"Joystick deadzone",option_row==0);frontend_entry(&value,text,option_row==0);
  }else if(controls_page){
   entries[0].y=-100;entries[0].panel_height=230;entries[1].y=-85;
   frontend_entry(entries," ",0);frontend_control_mode(entries+1,controls_page==2?"First Person Shooter":"Third Person Shooter",option_row<0);
   for(int row=0;row<(controls_page==2?10:9);row++){
    TombDialogEntry label={170,-57+18*row,TOMB_TEXT_MIDDLE,0,0},value={365,-57+18*row,TOMB_TEXT_MIDDLE,0,0};char key[40];frontend_key_name(control_bindings()[row],key);
    frontend_entry(&label,(controls_page==2?fps_binding_names:shooter_binding_names)[row],row==option_row && !binding_wait);frontend_entry(&value,binding_wait && row==option_row?"?":key,row==option_row && binding_wait);
   }
  }else {
   frontend_entry(entries," ",0);frontend_control_mode(entries+1,"Original",option_row<0);
   for(int row=0;row<13;row++){int b=frontend_binding_order[row];char key[40];frontend_key_name(bindings[b],key);frontend_entry(entries+2+row,binding_names[b],b==option_row && !binding_wait);frontend_entry(entries+15+row,binding_wait && b==option_row?"?":key,b==option_row && binding_wait);}
  }
 }
 if(menu_error[0])ui_text(0,-45,menu_error,TOMB_TEXT_CENTRE|TOMB_TEXT_BOTTOM);
}
