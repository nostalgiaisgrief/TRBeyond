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
    if(front_test)return;
    char path[MAX_PATH],value[32],key[32];settings_path(path);
    WritePrivateProfileStringA("Game","source",source_dirs[source_selected],path);
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

static void settings_read(void){
    char path[MAX_PATH],key[32];settings_path(path);
    settings_force43=GetPrivateProfileIntA("Display","force43",0,path)!=0;
    settings_framerate=(int)GetPrivateProfileIntA("Display","framerate",0,path);
    if(settings_framerate<0 || settings_framerate>2)settings_framerate=0;
    settings_vsync=GetPrivateProfileIntA("Display","vsync",1,path)!=0;
    settings_vr_deadzone=(int)GetPrivateProfileIntA("VR","deadzone",18,path);
    if(settings_vr_deadzone<0 || settings_vr_deadzone>50)settings_vr_deadzone=18;
    for(int i=0;i<16;i++){
        snprintf(key,sizeof key,"value%d",i);
        unsigned def=i==2?2:i<3?10:bindings[i-3];
        unsigned v=settings_named_read("Tomb",tomb_setting_keys[i],key,def,path);
        if(i<2 && v<=10){if(i==0)settings_music=(int)v;else settings_sound=(int)v;}
        else if(i==2 && v<=2)settings_detail=(int)v;
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
