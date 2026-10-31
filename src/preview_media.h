/* Modal presentation: gameplay never advances during movies or CUT1. */
static void media_path(char *path,const char *relative){source_path_for(path,relative,fmv_selected);}
typedef struct MediaAudio { HWAVEOUT device; WAVEHDR buffer; unsigned char *scaled; } MediaAudio;
static int media_audio_open(MediaAudio *a,const unsigned char *data,unsigned bytes,unsigned rate,unsigned channels,unsigned bits){
    memset(a,0,sizeof *a);if(!bytes)return 1;
    WAVEFORMATEX fmt={0};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=(WORD)channels;fmt.nSamplesPerSec=rate;fmt.wBitsPerSample=(WORD)bits;fmt.nBlockAlign=(WORD)(channels*bits/8);fmt.nAvgBytesPerSec=rate*fmt.nBlockAlign;
    if(waveOutOpen(&a->device,tomb_sound_output_device(),&fmt,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR)return 0;
    waveOutPause(a->device);
    /* waveOutSetVolume also changes other streams in the Windows audio session.
       Apply music gain to this buffer only, leaving effects and future levels alone. */
    int volume=settings_music<0?0:settings_music>10?10:settings_music;
    if(volume<10){
        if((bits!=8 && bits!=16) || (bits==16 && (bytes&1)))return 0;
        a->scaled=malloc(bytes);if(!a->scaled)return 0;
        if(bits==8){for(unsigned i=0;i<bytes;i++)a->scaled[i]=(unsigned char)(128+((int)data[i]-128)*volume/10);}
        else for(unsigned i=0;i<bytes;i+=2){
            int sample=(int16_t)(data[i]|(unsigned)data[i+1]<<8);
            uint16_t value=(uint16_t)(sample*volume/10);a->scaled[i]=(unsigned char)value;a->scaled[i+1]=(unsigned char)(value>>8);
        }
        data=a->scaled;
    }
    a->buffer.lpData=(LPSTR)data;a->buffer.dwBufferLength=bytes;
    if(waveOutPrepareHeader(a->device,&a->buffer,sizeof a->buffer)!=MMSYSERR_NOERROR || waveOutWrite(a->device,&a->buffer,sizeof a->buffer)!=MMSYSERR_NOERROR)return 0;
    return 1;
}
static void media_audio_close(MediaAudio *a){if(a->device){waveOutReset(a->device);if(a->buffer.dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(a->device,&a->buffer,sizeof a->buffer);waveOutClose(a->device);}free(a->scaled);memset(a,0,sizeof *a);}
static double media_clock(MediaAudio *a,double elapsed,unsigned rate){
    MMTIME time={0};time.wType=TIME_SAMPLES;
    if(a->device && !(a->buffer.dwFlags&WHDR_DONE) && waveOutGetPosition(a->device,&time,sizeof time)==MMSYSERR_NOERROR && time.wType==TIME_SAMPLES)return (double)time.u.sample/rate;
    return elapsed;
}
/* Refresh even without mouse motion when entering/leaving modal playback. */
static void media_cursor_refresh(void){
    HWND window=WindowFromDC(wglGetCurrentDC());POINT point;
    if(window && GetCursorPos(&point) && WindowFromPoint(point)==window){
        ScreenToClient(window,&point);RECT client;GetClientRect(window,&client);
        if(PtInRect(&client,point))SendMessageA(window,WM_SETCURSOR,(WPARAM)window,MAKELPARAM(HTCLIENT,WM_MOUSEMOVE));
    }
}
static int media_messages(MediaAudio *a,HWND window,int *active){
    MSG msg;while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)running=0;TranslateMessage(&msg);DispatchMessage(&msg);}
    int now=GetActiveWindow()==window && !IsIconic(window);
    if(xr.instance){TombVrInput in={0};if(xr.begun)xr_end(0,0);xr_begin(&in);now=xr.state==XR_SESSION_STATE_FOCUSED;static int held;int pressed=in.secondary[0] || in.secondary[1];if(pressed && !held)media_skip=1;held=pressed;}
    if(now!=*active){media_cursor_refresh();if(a->device){if(now)waveOutRestart(a->device);else waveOutPause(a->device);}*active=now;}
    return running && !media_skip;
}
static void media_begin(void){if(xr.begun)xr_end(0,0);xr.panel_open=0;mouse_release();media_busy=1;media_cursor_refresh();media_skip=0;audio_stop();tomb_sound_output_update(sound_output,1);tomb_sound_output_update(ring_sound_output,1);memset(held_keys,0,sizeof held_keys);}
static void media_end(void){if(xr.begun)xr_end(0,0);xr.panel_open=0;media_busy=0;media_cursor_refresh();media_skip=0;accumulator=0;memset(held_keys,0,sizeof held_keys);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glColor3f(1,1,1);}
static int media_audio_check(void){
    unsigned char silence[44100*4]={0};MediaAudio audio={0};
    if(!media_audio_open(&audio,silence,sizeof silence,44100,2,16)){media_audio_close(&audio);return 0;}
    MMTIME position={0};position.wType=TIME_SAMPLES;waveOutGetPosition(audio.device,&position,sizeof position);int ok=position.wType==TIME_SAMPLES && position.u.sample==0;
    waveOutRestart(audio.device);Sleep(100);position.wType=TIME_SAMPLES;waveOutGetPosition(audio.device,&position,sizeof position);ok=ok && position.wType==TIME_SAMPLES && position.u.sample>0;
    waveOutPause(audio.device);position.wType=TIME_SAMPLES;waveOutGetPosition(audio.device,&position,sizeof position);DWORD before=position.u.sample;
    Sleep(40);position.wType=TIME_SAMPLES;waveOutGetPosition(audio.device,&position,sizeof position);ok=ok && position.u.sample==before;
    /* A second stream stands in for effects/music after a level transition.
       Muted, half-volume and restored playback must never change its gain. */
    DWORD original=0,current=0;int saved_music=settings_music;
    ok=ok && waveOutGetVolume(audio.device,&original)==MMSYSERR_NOERROR;
    const unsigned char pcm16[]={0,128,255,127,0,0,16,39},pcm8[]={0,128,255,64};
    for(int bits=8;bits<=16;bits+=8)for(int volume=0;volume<=10;volume+=5){
        settings_music=volume;MediaAudio movie={0};
        const unsigned char *pcm=bits==8?pcm8:pcm16;unsigned bytes=bits==8?sizeof pcm8:sizeof pcm16;
        int opened=media_audio_open(&movie,pcm,bytes,44100,2,(unsigned)bits);ok=ok && opened;
        if(opened){
            const unsigned char *out=(const unsigned char *)movie.buffer.lpData;
            for(unsigned i=0;i<bytes;i+=bits/8){
                int sample=bits==8?(int)pcm[i]-128:(int16_t)(pcm[i]|(unsigned)pcm[i+1]<<8);
                int result=bits==8?(int)out[i]-128:(int16_t)(out[i]|(unsigned)out[i+1]<<8);
                ok=ok && result==sample*volume/10;
            }
        }
        media_audio_close(&movie);
        ok=ok && waveOutGetVolume(audio.device,&current)==MMSYSERR_NOERROR && current==original;
    }
    settings_music=saved_music;
    media_audio_close(&audio);return ok;
}
static int play_movie(const char *name,int test){
    if(fmv_selected==0){if(!strcmp(name,"ESCAPE"))return 1;if(!strcmp(name,"CORE"))name="CORELOGO";}
    char path[MAX_PATH],relative[80];snprintf(relative,sizeof relative,"FMV/%s.bmv",name);media_path(path,relative);
    TombMovie movie;if(!tomb_movie_open(&movie,path)){if(!test)MessageBoxA(WindowFromDC(wglGetCurrentDC()),"Movie unavailable. Prepare the original FMV assets with tools/prepare_movies.py.","Tomb Raider Beyond",MB_OK|MB_ICONWARNING);return 0;}
    MediaAudio audio={0};int ok=1;
    if(!test && !media_audio_open(&audio,movie.audio,movie.audio_bytes,movie.rate,movie.channels,16)){media_audio_close(&audio);tomb_movie_close(&movie);return 0;}
    media_begin();HDC dc=wglGetCurrentDC();HWND window=WindowFromDC(dc);GLuint texture;glGenTextures(1,&texture);
    glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    GLuint probe=0;unsigned char *reference=NULL,*repeat=NULL;size_t pixels=(size_t)width*height*3;
    if(test){glGenTextures(1,&probe);reference=malloc(pixels);repeat=malloc(pixels);if(!reference || !repeat){free(reference);free(repeat);glDeleteTextures(1,&probe);glDeleteTextures(1,&texture);tomb_movie_close(&movie);media_end();return 0;}}
    int pw=1,ph=1;while(pw<(int)movie.width)pw*=2;while(ph<(int)movie.height)ph*=2;
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,pw,ph,0,GL_RGB,GL_UNSIGNED_BYTE,NULL);
    LARGE_INTEGER frequency,last,now;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&last);double elapsed=0;int active=-1;unsigned previous=UINT_MAX;
    for(unsigned iteration=0;running;iteration++){
        if(!test && !media_messages(&audio,window,&active))break;
        QueryPerformanceCounter(&now);if(active==1)elapsed+=(double)(now.QuadPart-last.QuadPart)/frequency.QuadPart;last=now;
        unsigned frame=test?(iteration<2?0:iteration<4?movie.frames/2:movie.frames-1):(unsigned)(media_clock(&audio,elapsed,movie.rate)*movie.numerator/movie.denominator);
        if(frame>=movie.frames)break;
        if(frame!=previous){if(!tomb_movie_frame(&movie,frame)){ok=0;break;}glBindTexture(GL_TEXTURE_2D,texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,(GLsizei)movie.width,(GLsizei)movie.height,GL_RGB,GL_UNSIGNED_BYTE,movie.pixels);previous=frame;}
        light_UseProgram(0);glDisable(GL_DEPTH_TEST);glDisable(GL_ALPHA_TEST);glDisable(GL_SCISSOR_TEST);glViewport(0,0,width,height);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
        int vw=width,vh=width*3/4;if(vh>height){vh=height;vw=height*4/3;}/* DOS mode 1 doubles source pixels inside its 640x480 canvas. */
        if(fmv_selected!=0){vw=vw*(int)movie.width/320;vh=vh*(int)movie.height/240;}glViewport((width-vw)/2,(height-vh)/2,vw,vh);
        glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1,1,0,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);glColor3f(1,1,1);
        float u=(float)movie.width/pw,v=(float)movie.height/ph;
        glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(0,0);glTexCoord2f(u,0);glVertex2f(1,0);glTexCoord2f(u,v);glVertex2f(1,1);glTexCoord2f(0,v);glVertex2f(0,1);glEnd();
        if(glGetError()!=GL_NO_ERROR){ok=0;break;}if(test && iteration==1 && !strcmp(name,"LIFT"))capture("build/fmv40.ppm");if(test){
            glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,(iteration&1)?repeat:reference);
            if((iteration&1) && memcmp(reference,repeat,pixels)){fprintf(stderr,"Movie repeated frame changed after VR texture binding: %s\n",name);ok=0;break;}
            if(iteration==5)break;
            glBindTexture(GL_TEXTURE_2D,probe);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,width,height,0);
        }vr_media_present();display_present(dc);if(!test)Sleep(1);
    }
    free(reference);free(repeat);if(probe)glDeleteTextures(1,&probe);
    media_audio_close(&audio);glDeleteTextures(1,&texture);tomb_movie_close(&movie);media_end();return ok;
}
static int cinematic_effect_check(void){
    size_t bytes=(size_t)width*height*3;unsigned char *before=malloc(bytes),*after=malloc(bytes);
    if(!before || !after){free(before);free(after);return 0;}
    glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,before);
    uint32_t saved=effect_ticks;effect_ticks+=6;int ok=draw_frame_composite(0);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,after);effect_ticks=saved;
    size_t changed=0;for(size_t i=0;i<bytes;i++)changed+=before[i]!=after[i];
    free(before);free(after);FILE *log=fopen("build/cinematic-effects.txt","w");if(log){fprintf(log,"%s: fixed CUT1 camera/actors, %zu colour channels changed by water/texture clock\n",ok && changed?"PASS":"FAIL",changed);fclose(log);}
    return ok && changed>0;
}
static void cinematic_event(void *user,int kind,int16_t id,TombActor *actor){
    TombObjects *w=user;
    if(kind!=6)return; /* Cinematic sound is carried by its original CD track. */
    if(id==13){if(!tomb_flip_map(w,1))running=0;}
    else if(id==14){
        size_t i=0;while(i<w->count && &w->items[i].actor!=actor)++i;
        TombModel model,guns;if(i==w->count || !tomb_visual_model(w->visual,w->items[i].object,&model) || !tomb_visual_model(w->visual,1,&guns)){running=0;return;}
        /* DOS 1e24c exchanges the right thigh and right hand meshes. */
        for(int j=4;j<=10;j+=6){TombMeshView mesh=w->visual->meshes[model.mesh_start+j];w->visual->meshes[model.mesh_start+j]=w->visual->meshes[guns.mesh_start+j];w->visual->meshes[guns.mesh_start+j]=mesh;}
    }
}
static int play_cutscene_sequence(int test,int number){
    char path[MAX_PATH],error[256];char name[32];snprintf(name,sizeof name,"CUT%d.PHD",number);asset_path(path,name);TombLevel *cut=tomb_level_load(path,error,sizeof error);TombVisual *view=NULL;TombObjects world={0};TombCinematic sequence;
    TombScreenRect *rects=NULL;
    if(!cut || !(view=tomb_visual_load(cut,error,sizeof error)) || !(rects=calloc(cut->room_count,sizeof *rects)) || !tomb_objects_init(&world,cut,view) || !tomb_cinematic_init_sequence(&sequence,&world,number)){
        fprintf(stderr,"CUT%d initialization failed: %s\n",number,error);tomb_objects_free(&world);free(rects);tomb_visual_free(view);tomb_level_free(cut);return 0;
    }
    unsigned char *wav=NULL,*last_frame_pixels=NULL;MediaAudio audio={0};int ok=1;
    if(!test){
        int cd=number==1?8:number==2?10:number==3?9:7;if(music_source()==0)cd+=15;
        snprintf(name,sizeof name,"audio/cd%02d.wav",cd);source_path_for(path,name,music_source());FILE *f=fopen(path,"rb");long bytes=0;if(f){if(!fseek(f,0,SEEK_END))bytes=ftell(f);rewind(f);if(bytes>0 && bytes<100000000)wav=malloc((size_t)bytes);if(wav && fread(wav,1,(size_t)bytes,f)!=(size_t)bytes){free(wav);wav=NULL;}fclose(f);}
        TombSoundSample sample;if(!wav || !tomb_sound_sample(&sample,wav,(size_t)bytes) || !media_audio_open(&audio,sample.pcm,(unsigned)(sample.frames*sample.channels*sample.bits/8),sample.rate,sample.channels,sample.bits))ok=0;
    }
    TombLevel *old_level=level;TombVisual *old_visual=visual;TombObjects old_objects=objects;TombPlaytest old_play=play;TombFollowCamera old_camera=camera;
    GLuint *old_tiles=tiles,*old_font=font_tiles;TombScreenRect *old_rects=room_rects;int old_caves=caves,old_pose=pose_mode,old_manual=manual_camera;double old_alpha=camera_alpha;uint32_t old_effect_ticks=effect_ticks;
    cinematic_equipment=(play.gameplay_options&TOMB_CONSISTENT_WEAPONS)?old_visual:NULL;
    cinematic_equipment_tiles=old_tiles;cinematic_sidearm=play.sidearm_type;cinematic_shotgun=play.inventory.counts[1]!=0;
    media_begin();cinema_mode=1;level=cut;visual=view;objects=world;room_rects=rects;tiles=font_tiles=NULL;caves=1;pose_mode=manual_camera=0;camera_alpha=1;
    memset(&play,0,sizeof play);play.lara.actor=sequence.origin;memset(&camera,0,sizeof camera);
    if(!make_tiles())ok=0;
    TombAnimContext ctx={cut->animations,cut->animation_count,cut->changes,cut->change_count,cut->ranges,cut->range_count,cut->commands,cut->command_count,sine_table,0,0,0,cinematic_event,&objects};
    /* DOS controls active actors before drawing cinematic camera frame zero. */
    for(size_t i=0;i<objects.count;i++)if(objects.items[i].object>=77 && objects.items[i].object<=80 && !tomb_object_animate(objects.items+i,&ctx))ok=0;
    HDC dc=wglGetCurrentDC();HWND window=WindowFromDC(dc);LARGE_INTEGER frequency,last,now;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&last);double elapsed=0;int active=-1;unsigned tick=0;
    while(ok && running && sequence.frame<sequence.count){
        if(!test && !media_messages(&audio,window,&active))break;
        QueryPerformanceCounter(&now);if(active==1)elapsed+=(double)(now.QuadPart-last.QuadPart)/frequency.QuadPart;last=now;
        unsigned wanted=test?tick++:(unsigned)(media_clock(&audio,elapsed,44100)*30);
        /* DOS 14d22..14d45 exits on the terminal control tick before drawing.
           Its actor update can loop the animation. Keep the preceding visible
           pose and camera, including when the audio clock skips ahead. */
        unsigned final_frame=sequence.count>1?sequence.count-2:0;
        unsigned target=wanted<final_frame?wanted:final_frame;
        while(sequence.frame<target)if(!tomb_cinematic_tick(&sequence,&objects,&ctx)){ok=0;break;}
        if(!ok || sequence.frame>=sequence.count)break;
        effect_ticks=sequence.frame;
        TombDosCamera shot;if(!tomb_cinematic_camera(&sequence,sine_table,&shot,&cinema_fov,&cinema_roll)){ok=0;break;}
        camera.eye[0]=camera.previous_eye[0]=shot.eye.x;camera.eye[1]=camera.previous_eye[1]=shot.eye.y;camera.eye[2]=camera.previous_eye[2]=shot.eye.z;
        camera.target[0]=camera.previous_target[0]=shot.target.x;camera.target[1]=camera.previous_target[1]=shot.target.y;camera.target[2]=camera.previous_target[2]=shot.target.z;camera.room=shot.eye.room;camera.ready=1;
        if(!test || sequence.frame%200==0 || sequence.frame==final_frame){glEnable(GL_DEPTH_TEST);if(!draw_frame_composite(0))ok=0;if(test && number==1 && sequence.frame==800){if(!cinematic_effect_check())ok=0;capture("build/cutscene40.ppm");}if(!test){vr_media_present();display_present(dc);}}
        if(test && !last_frame_pixels && sequence.frame==final_frame){
            size_t bytes=(size_t)width*height*3;last_frame_pixels=malloc(bytes);
            if(!last_frame_pixels)ok=0;
            else {glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,last_frame_pixels);}
        }
        if(wanted>=sequence.count-1)break;
        if(!test)Sleep(1);
    }
    if(ok && old_objects.progress && old_objects.progress->complete){
        /* Capture the final scene in the back buffer, independent of window occlusion. */
        if(!draw_frame_composite(0))ok=0;
        free(cinematic_pixels);cinematic_width=width;cinematic_height=height;
        cinematic_pixels=malloc((size_t)width*height*3);
        if(cinematic_pixels){
            glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
            glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,cinematic_pixels);glReadBuffer(GL_BACK);
            if(test && last_frame_pixels && memcmp(last_frame_pixels,cinematic_pixels,(size_t)width*height*3)){
                fprintf(stderr,"CUT%d results backdrop differs from last displayed frame\n",number);ok=0;
            }
            memcpy(cinematic_palette,view->palette,768);
            memcpy(cinematic_light,cut->file_data+cut->item_parsed_bytes+24*256,256);
        }
    }
    media_audio_close(&audio);free(wav);free(last_frame_pixels);
    if(tiles){glDeleteTextures((GLsizei)(visual->tile_count*2+1),tiles);free(tiles);}if(font_tiles){glDeleteTextures((GLsizei)visual->tile_count,font_tiles);free(font_tiles);}
    tomb_objects_free(&objects);free(rects);tomb_visual_free(view);tomb_level_free(cut);
    level=old_level;visual=old_visual;objects=old_objects;play=old_play;camera=old_camera;tiles=old_tiles;font_tiles=old_font;room_rects=old_rects;caves=old_caves;pose_mode=old_pose;manual_camera=old_manual;camera_alpha=old_alpha;
    cinematic_equipment=NULL;cinematic_equipment_tiles=NULL;effect_ticks=old_effect_ticks;shadow_palette=NULL;cinema_mode=0;media_end();return ok;
}

static int play_cutscene(int test){return play_cutscene_sequence(test,1);}

/* DOS 2057c: END movie followed by END/CRED1/CRED2/CRED3 pictures. */
/* Both DOS executables request 60 palette steps in and 20 out, at 60 Hz. */
#define CREDITS_FADE_OUT (20.0/60.0)
#define CREDITS_HOLD 15.0
static float credits_brightness(double elapsed,double fade_start,float fade_from){
 if(fade_start>=0)return (float)fmax(0,fade_from*(1-(elapsed-fade_start)/CREDITS_FADE_OUT));
 return (float)fmax(0,fmin(1,fmin(elapsed,(CREDITS_HOLD+CREDITS_FADE_OUT-elapsed)/CREDITS_FADE_OUT)));
}
static int ending_picture_path(char *path,int ub,int page){
 const char *names[]={"END","CRED1","CRED2","CRED3"};
 int combined=ub && campaign_selected==CAMPAIGN_COMBINED;
 if(ub && page==(combined?4:1)){ub_path(path,"DATA/CRED0.PCX");return GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES;}
 int index=ub && !combined && page>1?page-1:page;
 if(!ub || combined){char name[32];snprintf(name,sizeof name,"%s.PCX",names[index]);asset_path(path,name);return GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES;}
 /* UB's unstarred paths read the original CD pictures. Prefer those DOS
    assets, using available equivalents when installed with another source. */
 const int order[]={1,0,2};
 for(int i=0;i<3;i++){
  char relative[40];snprintf(relative,sizeof relative,"DATA/%s.%s",names[index],order[i]==0?"tbi":"PCX");source_path_for(path,relative,order[i]);
  if(GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES)return 1;
 }
 return 0;
}
static int play_ending(int test){
    int ub=level_number>=UB_FIRST_LEVEL || campaign_selected==CAMPAIGN_COMBINED;
    if(!ub && !test && !play_movie("END",0))return 0;
    HDC dc=wglGetCurrentDC();HWND window=WindowFromDC(dc);media_begin();int ok=1;
    for(int page=0;page<(ub?5:source_selected==0?2:4) && running && ok;page++){
        char path[MAX_PATH];if(!ending_picture_path(path,ub,page)){if(ub && page!=(campaign_selected==CAMPAIGN_COMBINED?4:1))continue;ok=0;break;}
        unsigned char *rgb=tomb_picture_load(path);if(!rgb){ok=0;break;}
        GLuint texture=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,1024,512,0,GL_RGB,GL_UNSIGNED_BYTE,rgb);free(rgb);
        LARGE_INTEGER frequency,last,now;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&last);double elapsed=0,fade_start=-1;float fade_from=1;int active=-1,test_frame=0;uint64_t test_half=0;MediaAudio silent={0};media_skip=0;
        while(ok && running){
            if(!test){
                media_messages(&silent,window,&active);if(!running)break;
                if(media_skip && fade_start<0){fade_from=credits_brightness(elapsed,-1,1);fade_start=elapsed;}
                media_skip=0;
            }
            QueryPerformanceCounter(&now);if(active==1)elapsed+=(double)(now.QuadPart-last.QuadPart)/frequency.QuadPart;last=now;
            if(test){static const double frames[]={0,.5,1,CREDITS_HOLD,CREDITS_HOLD+CREDITS_FADE_OUT/2,CREDITS_HOLD+CREDITS_FADE_OUT};elapsed=frames[test_frame++];}
            float brightness=credits_brightness(elapsed,fade_start,fade_from);
            light_UseProgram(0);glDisable(GL_DEPTH_TEST);glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glViewport(0,0,width,height);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
            int vw=width,vh=width*3/4;if(vh>height){vh=height;vw=height*4/3;}glViewport((width-vw)/2,(height-vh)/2,vw,vh);
            glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1,1,0,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);glColor3f(brightness,brightness,brightness);
            glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(0,0);glTexCoord2f(640.f/1024,0);glVertex2f(1,0);glTexCoord2f(640.f/1024,480.f/512);glVertex2f(1,1);glTexCoord2f(0,480.f/512);glVertex2f(0,1);glEnd();
            if(test){
                size_t bytes=(size_t)width*height*4;unsigned char *pixels=malloc(bytes);uint64_t total=0;
                if(!pixels)ok=0;
                else{glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);for(size_t i=0;i<bytes;i+=4)total+=pixels[i]+pixels[i+1]+pixels[i+2];free(pixels);
                    if(test_frame==1 || test_frame==6){if(total)ok=0;}
                    else if(test_frame==2){test_half=total;if(!total)ok=0;}
                    else if(test_frame==3 && total<=test_half)ok=0;
                }
            }
            if(glGetError()!=GL_NO_ERROR)ok=0;
            if(!test){vr_media_present();display_present(dc);}
            if(elapsed>=CREDITS_HOLD+CREDITS_FADE_OUT || (fade_start>=0 && elapsed>=fade_start+CREDITS_FADE_OUT))break;
            if(!test)Sleep(1);
        }
        glDeleteTextures(1,&texture);
    }
    media_end();return ok;
}
