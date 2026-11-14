#ifndef PRESENTATION_SYNC_H
#define PRESENTATION_SYNC_H
/* Shared desktop swap policy. Call with a current WGL context; the caller
   owns the policy (TR1 settings/VR, or TR2's forced desktop V-sync).
   Read back the interval because another game can use the same context. */
static int tomb_presentation_sync(int enabled){
    typedef BOOL (WINAPI *SetInterval)(int);
    typedef int (WINAPI *GetInterval)(void);
    static HGLRC context;
    static SetInterval set_interval;
    static GetInterval get_interval;
    static int applied=-1;
    HGLRC current=wglGetCurrentContext();
    if(!current)return 0;
    if(current!=context){
        PROC proc;
        context=current;set_interval=NULL;get_interval=NULL;applied=-1;
        proc=wglGetProcAddress("wglSwapIntervalEXT");
        if(proc && (INT_PTR)proc!=-1 && (UINT_PTR)proc>3)memcpy(&set_interval,&proc,sizeof set_interval);
        proc=wglGetProcAddress("wglGetSwapIntervalEXT");
        if(proc && (INT_PTR)proc!=-1 && (UINT_PTR)proc>3)memcpy(&get_interval,&proc,sizeof get_interval);
    }
    enabled=!!enabled;
    int actual=get_interval?get_interval():applied;
    if(set_interval && actual!=enabled){
        if(set_interval(enabled))applied=enabled;
        actual=get_interval?get_interval():applied;
    }
    return actual>0;
}
#endif
