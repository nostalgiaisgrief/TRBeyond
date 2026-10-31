/* Portable settings: named keys take precedence over legacy numbered keys. */
static const char *const tomb_setting_keys[16]={
    "music_volume","sound_volume","detail","run","back","turn_left","turn_right",
    "step_left","step_right","walk","jump","action","draw_weapon","roll","look","inventory"
};
static const char *const shooter_setting_keys[9]={
    "forward","back","left","right","walk","jump","action","draw_weapon","inventory"
};
static const char *const fps_setting_keys[10]={
    "forward","back","left","right","walk","jump","action","shoot","weapon_mode","inventory"
};

static unsigned settings_named_read(const char *section,const char *name,const char *legacy,unsigned fallback,const char *path){
    unsigned old=GetPrivateProfileIntA(section,legacy,(int)fallback,path);
    return GetPrivateProfileIntA(section,name,(int)old,path);
}

static void settings_named_write(const char *section,const char *name,const char *legacy,unsigned value,const char *path){
    char text[32];snprintf(text,sizeof text,"%u",value);
    if(WritePrivateProfileStringA(section,name,text,path))
        WritePrivateProfileStringA(section,legacy,NULL,path);
}

static void settings_write(void){
    if(!settings_full_colour)settings_texture_filtering=0;
    if(front_test)return;
    char path[MAX_PATH],value[32],key[32];settings_path(path);
    WritePrivateProfileStringA("Game","campaign",campaign_selected==CAMPAIGN_COMBINED?"Combined":campaign_selected==CAMPAIGN_UB?"UB":"Tomb Raider",path);
    WritePrivateProfileStringA("Game","level_source",source_dirs[source_selected],path);
    WritePrivateProfileStringA("Game","music_speech_source",music_selected==3?"PS1+DOS":source_dirs[music_selected],path);
    WritePrivateProfileStringA("Game","fmv_source",source_dirs[fmv_selected],path);
    WritePrivateProfileStringA("Display","colour_depth",settings_full_colour?"Full Colour":"256 colours",path);
    if(WritePrivateProfileStringA("Display","texture_filtering",settings_texture_filtering?"1":"0",path))
        WritePrivateProfileStringA("Display","texture_smoothing",NULL,path);
    WritePrivateProfileStringA("Display","draw_distance",draw_distance_names[settings_draw_distance],path);
    WritePrivateProfileStringA("Display","water_style",visual_style_names[settings_water_style],path);
    WritePrivateProfileStringA("Display","preset",settings_preset<0?"Custom":preset_names[settings_preset],path);
    snprintf(value,sizeof value,"%d",settings_force43);WritePrivateProfileStringA("Display","force43",value,path);
    snprintf(value,sizeof value,"%d",settings_framerate);WritePrivateProfileStringA("Display","framerate",value,path);
    snprintf(value,sizeof value,"%d",settings_vsync);WritePrivateProfileStringA("Display","vsync",value,path);
    snprintf(value,sizeof value,"%d",settings_vr_deadzone);WritePrivateProfileStringA("VR","deadzone",value,path);
    int values[3]={settings_music,settings_sound,settings_detail};
    for(int i=0;i<16;i++){
        snprintf(key,sizeof key,"value%d",i);
        settings_named_write("Tomb",tomb_setting_keys[i],key,i<3?(unsigned)values[i]:bindings[i-3],path);
    }
    for(int i=0;i<9;i++){
        snprintf(key,sizeof key,"key%d",i);
        settings_named_write("Shooter",shooter_setting_keys[i],key,shooter_bindings[i],path);
    }
    for(int i=0;i<10;i++){
        snprintf(key,sizeof key,"key%d",i);
        settings_named_write("FPS",fps_setting_keys[i],key,fps_bindings[i],path);
    }
}

static void settings_apply_preset(int preset){
    settings_detail=preset==1?0:preset==2?1:preset==3?3:2;
    settings_force43=preset!=0;
    settings_framerate=preset==0?0:2;
    settings_full_colour=preset==2 || preset==3;
    settings_texture_filtering=preset==2;
    settings_water_style=preset==0?3:preset==3?0:preset;
    settings_draw_distance=preset==0?1:0;
}

static void settings_read(void){
    char path[MAX_PATH],key[32];settings_path(path);
    char style[32],preset[32];
    GetPrivateProfileStringA("Game","campaign","Tomb Raider",style,sizeof style,path);
    if(title_mode)campaign_selected=ub_available()?(!strcmp(style,"Combined")?CAMPAIGN_COMBINED:!strcmp(style,"UB")?CAMPAIGN_UB:CAMPAIGN_TR):CAMPAIGN_TR;
    else if(campaign_selected!=CAMPAIGN_COMBINED)campaign_selected=level_number>=UB_FIRST_LEVEL;
    GetPrivateProfileStringA("Display","preset","Optimal",preset,sizeof preset,path);
    settings_preset=-1;for(int i=0;i<4;i++)if(!strcmp(preset,preset_names[i]))settings_preset=i;
    /* Only an absent setting inherits an initial value. Source changes never
       override the user's colour-depth choice. */
    settings_apply_preset(settings_preset<0?0:settings_preset);
    int initial_full_colour=settings_full_colour;
    GetPrivateProfileStringA("Display","colour_depth",initial_full_colour?"Full Colour":"256 colours",style,sizeof style,path);
    settings_full_colour=!strcmp(style,"Full Colour");
    settings_texture_filtering=settings_named_read("Display","texture_filtering","texture_smoothing",settings_preset==2,path)!=0;
    /* Honour an explicit palette choice in old or hand-edited settings. */
    if(!settings_full_colour)settings_texture_filtering=0;
    GetPrivateProfileStringA("Display","water_style",visual_style_names[settings_water_style],style,sizeof style,path);
    settings_water_style=1;for(int i=0;i<4;i++)if(!strcmp(style,visual_style_names[i]))settings_water_style=i;
    GetPrivateProfileStringA("Display","draw_distance",draw_distance_names[settings_draw_distance],style,sizeof style,path);
    for(int i=0;i<3;i++)if(!strcmp(style,draw_distance_names[i]))settings_draw_distance=i;
    settings_force43=GetPrivateProfileIntA("Display","force43",settings_force43,path)!=0;
    settings_framerate=(int)GetPrivateProfileIntA("Display","framerate",settings_framerate,path);
    if(settings_framerate<0 || settings_framerate>2)settings_framerate=0;
    settings_vsync=GetPrivateProfileIntA("Display","vsync",1,path)!=0;
    settings_vr_deadzone=(int)GetPrivateProfileIntA("VR","deadzone",18,path);
    if(settings_vr_deadzone<0 || settings_vr_deadzone>50)settings_vr_deadzone=18;
    for(int i=0;i<16;i++){
        snprintf(key,sizeof key,"value%d",i);
        unsigned def=i==2?(unsigned)settings_detail:i<3?10:bindings[i-3];
        unsigned v=settings_named_read("Tomb",tomb_setting_keys[i],key,def,path);
        if(i<2 && v<=10){if(i==0)settings_music=(int)v;else settings_sound=(int)v;}
        else if(i==2 && v<=3)settings_detail=(int)v;
        else if(i>=3 && v>0 && v<256)bindings[i-3]=v;
    }
    for(int i=0;i<9;i++){
        snprintf(key,sizeof key,"key%d",i);
        unsigned v=settings_named_read("Shooter",shooter_setting_keys[i],key,shooter_bindings[i],path);
        if(v>0 && v<256)shooter_bindings[i]=v;
    }
    for(int i=0;i<10;i++){
        snprintf(key,sizeof key,"key%d",i);
        unsigned v=settings_named_read("FPS",fps_setting_keys[i],key,fps_bindings[i],path);
        if(v>0 && v<256)fps_bindings[i]=v;
    }
}
