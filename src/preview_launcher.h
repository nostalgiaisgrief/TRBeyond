/* Platform launcher. This loop does not tick, render or initialize any game.
   The selected game receives the existing window and GL/XR context. */
#include "launcher.h"
typedef struct LauncherWindow { HWND window;HDC dc;HGLRC context; } LauncherWindow;
static LauncherWindow launcher_window;
static unsigned launcher_input;
static int launcher_quit;
static LRESULT CALLBACK launcher_window_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_CLOSE || (msg==WM_SYSKEYDOWN && wp==VK_F4)){launcher_quit=1;return 0;}
    if(msg==WM_SETCURSOR && LOWORD(lp)==HTCLIENT){SetCursor(NULL);return TRUE;}
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT){PAINTSTRUCT p;BeginPaint(w,&p);EndPaint(w,&p);return 0;}
    if(msg==WM_KILLFOCUS){launcher_input=0;return 0;}
    if(msg==WM_SYSKEYDOWN && wp==VK_RETURN && (lp&(1L<<29))){if(!(lp&(1L<<30)))toggle_fullscreen(w);return 0;}
    if(msg==WM_KEYDOWN){
        if(!(lp&(1L<<30))){
            if(wp==VK_UP || wp==VK_NUMPAD8)launcher_input|=16;
            if(wp==VK_DOWN || wp==VK_NUMPAD2)launcher_input|=32;
            if(wp==VK_RETURN || wp==VK_CONTROL)launcher_input|=TOMB_RING_SELECT;
            if(wp==VK_ESCAPE)launcher_input|=TOMB_RING_BACK;
        }return 0;
    }
    return DefWindowProcA(w,msg,wp,lp);
}
static int launcher_window_open(void){
    HINSTANCE instance=GetModuleHandleA(NULL);
    WNDCLASSA wc={0};wc.style=CS_OWNDC;wc.lpfnWndProc=launcher_window_proc;wc.hInstance=instance;wc.lpszClassName="TombBeyondLauncher";
    if(!RegisterClassA(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;
    RECT bounds={0,0,width,height};AdjustWindowRectEx(&bounds,WS_OVERLAPPEDWINDOW,FALSE,0);
    launcher_window.window=CreateWindowA(wc.lpszClassName,"Tomb Raider Beyond",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,NULL,NULL,instance,NULL);
    if(!launcher_window.window)return 0;
    launcher_window.dc=GetDC(launcher_window.window);
    PIXELFORMATDESCRIPTOR pfd={0};pfd.nSize=sizeof pfd;pfd.nVersion=1;
    pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=24;pfd.cDepthBits=24;
    int format=ChoosePixelFormat(launcher_window.dc,&pfd);
    if(!format || !SetPixelFormat(launcher_window.dc,format,&pfd))return 0;
    launcher_window.context=wglCreateContext(launcher_window.dc);
    return launcher_window.context && wglMakeCurrent(launcher_window.dc,launcher_window.context);
}
static void launcher_window_close(void){
    xr_close();
    if(launcher_window.context){wglMakeCurrent(NULL,NULL);wglDeleteContext(launcher_window.context);}
    if(launcher_window.dc)ReleaseDC(launcher_window.window,launcher_window.dc);
    if(launcher_window.window)DestroyWindow(launcher_window.window);
    memset(&launcher_window,0,sizeof launcher_window);
}
static void launcher_edge(const TombLauncherStyle *style,double x,double y,double w,double h,int index,double scale){
    const unsigned char *c=style->palette+index*3;glColor3f(c[0]/63.f,c[1]/63.f,c[2]/63.f);
    glBegin(GL_QUADS);glVertex2d(x,y);glVertex2d(x+w*scale,y);glVertex2d(x+w*scale,y+h*scale);glVertex2d(x,y+h*scale);glEnd();
}
static void launcher_entry(const TombLauncherStyle *style,const TombDialogEntry *entry,const char *text,float brightness,int w,int h,double scale){
    TombTextStyle font={0,0,65536,65536,1,6,0};
    if(entry->panel_width){
        int bounds[4];tomb_dialog_bounds(entry,tomb_text_width(text,&font),bounds);
        double x=(w-640*scale)/2+bounds[0]*scale,y=(h-480*scale)/2+bounds[1]*scale;
        int bw=bounds[2],bh=bounds[3];glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);
        launcher_edge(style,x,y-scale,bw+1,1,15,scale);launcher_edge(style,x,y,bw,1,31,scale);
        launcher_edge(style,x+bw*scale,y,1,bh+1,15,scale);launcher_edge(style,x+(bw+1)*scale,y,1,bh+2,31,scale);
        launcher_edge(style,x,y+bh*scale,bw+1,1,15,scale);launcher_edge(style,x-scale,y+(bh+1)*scale,bw+2,1,31,scale);
        launcher_edge(style,x-scale,y-scale,1,bh+2,15,scale);launcher_edge(style,x,y,1,bh+1,31,scale);
    }
    font=(TombTextStyle){(int)(entry->x*scale),(int)(entry->y*scale),(int32_t)(scale*65536),(int32_t)(scale*65536),1,6,(uint16_t)entry->flags};
    glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glColor3f(brightness,brightness,brightness);
    tomb_text_layout(text,&font,w,h,style->glyph,style->context);
    glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);
}
static void launcher_draw(const TombLauncherGame *games,int count,const TombLauncherSelection *selection,const TombLauncherStyle *style,int w,int h){
    if(style->draw_selector){style->draw_selector(selection->current,selection->available,w,h);return;}
    glViewport(0,0,w,h);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glAlphaFunc(GL_GREATER,.5f);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,w,h,0,-4096,4096);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    double scale=fmax(.5,fmin(w/640.0,h/480.0));
    TombDialogEntry entries[32];tomb_dialog_layout(71,selection->current,1,entries);
    for(int i=0;i<count+2;i++){
        entries[i].flags=TOMB_TEXT_CENTRE|TOMB_TEXT_MIDDLE;entries[i].y+=250;
        if(entries[i].panel_width)entries[i].panel_width=i==0?390:i==1?386:378;
    }
    entries[0].panel_height=40+18*count;
    launcher_entry(style,entries," ",1,w,h,scale);launcher_entry(style,entries+1,"Select Game",1,w,h,scale);
    for(int i=0;i<count;i++)launcher_entry(style,entries+i+2,games[i].name,selection->available&(1u<<i)?1:.35f,w,h,scale);
}
static unsigned launcher_pad(void){
    if(!pad_initialized)return 0;
    SDL_Event e;while(SDL_PollEvent(&e))if(e.type==SDL_EVENT_GAMEPAD_ADDED)pad_open(e.gdevice.which);
    unsigned input=0;
    for(int i=0;i<16;i++){
        PadDevice *d=pad_devices+i;if(!d->handle)continue;
        if(!SDL_GamepadConnected(d->handle)){SDL_CloseGamepad(d->handle);memset(d,0,sizeof *d);continue;}
        SDL_Gamepad *p=d->handle;
        if(SDL_GetGamepadButton(p,SDL_GAMEPAD_BUTTON_SOUTH))input|=TOMB_RING_SELECT;
        if(SDL_GetGamepadButton(p,SDL_GAMEPAD_BUTTON_EAST))input|=TOMB_RING_BACK;
        int y=SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_LEFTY);
        int x=SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_LEFTX);
        if(SDL_GetGamepadButton(p,SDL_GAMEPAD_BUTTON_DPAD_LEFT) || x<-20000)input|=TOMB_RING_LEFT;
        if(SDL_GetGamepadButton(p,SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || x>20000)input|=TOMB_RING_RIGHT;
        if(SDL_GetGamepadButton(p,SDL_GAMEPAD_BUTTON_DPAD_UP) || y<-20000)input|=16;
        if(SDL_GetGamepadButton(p,SDL_GAMEPAD_BUTTON_DPAD_DOWN) || y>20000)input|=32;
    }
    return input;
}
static void launcher_xr_panel(const TombVrInput *input){
    if(xr.panel_open || !input->head.valid)return;
    const TombVrPose *head=&input->head;double x=head->q[0],y=head->q[1],z=head->q[2],w=head->q[3];
    double yaw=atan2(2*(x*z+w*y),1-2*(x*x+y*y));
    xr.panel=(XrPosef){{0,(float)sin(yaw/2),0,(float)cos(yaw/2)},{(float)(head->p[0]-sin(yaw)*1.8),(float)(head->p[1]-xr.floor_offset),(float)(head->p[2]-cos(yaw)*1.8)}};
    xr.panel_open=1;
}
static int launcher_run(const TombLauncherGame *games,int count,int vr_requested,const char *shot){
    char path[MAX_PATH],last[16];snprintf(path,sizeof path,"%s/launcher.ini",install_root);
    GetPrivateProfileStringA("Launcher","last_game","",last,sizeof last,path);
    TombLauncherSelection selection=tomb_launcher_select(games,count,last);
    if(selection.current<0){MessageBoxA(NULL,"No supported game data was found. Run the installer to import an owned copy of Tomb Raider.","Tomb Raider Beyond",MB_OK|MB_ICONINFORMATION);return -1;}
    if(!launcher_window_open()){launcher_window_close();return -1;}
    TombLauncherStyle style={0};
    if(!games[selection.style].open_style(&style)){launcher_window_close();return -1;}
    if(!shot){toggle_fullscreen(launcher_window.window);ShowWindow(launcher_window.window,SW_SHOW);pad_init();}
    SetTimer(launcher_window.window,1,500,NULL);
    if(vr_requested && !xr_open())MessageBoxA(launcher_window.window,xr.error,"Virtual Reality unavailable",MB_OK|MB_ICONWARNING);
    unsigned previous=0;int first=1,capture_ok=0;launcher_input=0;launcher_quit=0;
    while(!launcher_quit && !selection.confirmed){
        MSG message;while(PeekMessageA(&message,NULL,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageA(&message);}
        TombVrInput input={0};if(xr.instance)xr_begin(&input);
        unsigned buttons=launcher_pad();
        if(xr.instance){
            if(input.primary[0] || input.primary[1])buttons|=TOMB_RING_SELECT;
            if(input.secondary[0] || input.secondary[1])buttons|=TOMB_RING_BACK;
            double threshold=style.vr_menu_threshold>0?style.vr_menu_threshold:.672;
            if(input.stick[0][1]>threshold)buttons|=16;if(input.stick[0][1]<-threshold)buttons|=32;
        }
        int focused=xr.instance?xr.state==XR_SESSION_STATE_FOCUSED:GetActiveWindow()==launcher_window.window;
        unsigned action=focused?launcher_input|(first?0:buttons&~previous):0;launcher_input=0;previous=buttons;first=0;
        if(action&16)tomb_launcher_move(&selection,count,-1);if(action&32)tomb_launcher_move(&selection,count,1);
        if(action&TOMB_RING_BACK)launcher_quit=1;
        if(action&TOMB_RING_SELECT)selection.confirmed=1;
#ifdef TOMB_BUILD_TESTS
        if(getenv("TRB_LAUNCHER_HANDOFF_TEST")){
            if(level || visual || sound_output || ring_sound_output){launcher_window_close();return -1;}
            selection.confirmed=1;
        }
#endif
        RECT client;GetClientRect(launcher_window.window,&client);width=client.right>0?client.right:1;height=client.bottom>0?client.bottom:1;
        if(xr.begun){
            launcher_xr_panel(&input);int panel=0;
            if(xr.frame.shouldRender && xr.panel_open && xr_target_begin(2)){launcher_draw(games,count,&selection,&style,xr.target[2].width,xr.target[2].height);panel=1;}
            if(xr.target[2].acquired)xr_target_end(2);xr_end(0,panel);
        }
        launcher_draw(games,count,&selection,&style,width,height);
        if(shot){capture_ok=capture(shot);break;}
        SwapBuffers(launcher_window.dc);
        MsgWaitForMultipleObjectsEx(0,NULL,focused?8:16,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    }
    pad_close();KillTimer(launcher_window.window,1);games[selection.style].close_style(&style);
    if(shot || launcher_quit){launcher_window_close();return shot?(capture_ok?-2:-1):-2;}
    /* Do not reveal a title background while the selected game initializes. */
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);SwapBuffers(launcher_window.dc);
    WritePrivateProfileStringA("Launcher","last_game",games[selection.current].id,path);
    return selection.current;
}
