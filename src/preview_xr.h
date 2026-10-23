/* OpenXR adapter. Loaded only on mode selection; no runtime needed for desktop play. */
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_NO_PROTOTYPES
#include <unknwn.h>
#include "../third_party/openxr/include/openxr/openxr.h"
#include "../third_party/openxr/include/openxr/openxr_platform.h"
#include "vr.h"
#define XR_FUNCTIONS(X) \
 X(DestroyInstance) X(GetSystem) X(GetInstanceProperties) X(CreateSession) X(DestroySession) \
 X(EnumerateReferenceSpaces) X(CreateReferenceSpace) X(DestroySpace) X(EnumerateViewConfigurationViews) \
 X(EnumerateSwapchainFormats) X(CreateSwapchain) X(DestroySwapchain) X(EnumerateSwapchainImages) \
 X(PollEvent) X(BeginSession) X(EndSession) X(WaitFrame) X(BeginFrame) X(EndFrame) X(LocateViews) \
 X(AcquireSwapchainImage) X(WaitSwapchainImage) X(ReleaseSwapchainImage) X(StringToPath) \
 X(CreateActionSet) X(DestroyActionSet) X(CreateAction) X(SuggestInteractionProfileBindings) \
 X(AttachSessionActionSets) X(CreateActionSpace) X(SyncActions) X(GetActionStateFloat) \
 X(GetActionStateBoolean) X(GetActionStateVector2f) X(GetActionStatePose) X(LocateSpace) X(ApplyHapticFeedback)
#define XR_POINTER(n) static PFN_xr##n xr_##n;
XR_FUNCTIONS(XR_POINTER)
#undef XR_POINTER
static PFN_xrGetInstanceProcAddr xr_get;
static PFN_xrCreateInstance xr_create;
static PFN_xrGetOpenGLGraphicsRequirementsKHR xr_requirements;
typedef struct XrTarget {
    XrSwapchain handle;
    XrSwapchainImageOpenGLKHR *images;
    uint32_t count,index;
    int width,height,acquired;
    GLuint depth;
} XrTarget;
static struct {
    HMODULE library;
    XrInstance instance;
    XrSession session;
    XrSpace space,head,hands[2],aims[2];
    XrActionSet actions;
    XrAction grip,trigger,primary,secondary,stick,pose,aim,haptic;
    XrPath paths[2];
    XrSessionState state;
    XrTarget target[3];
    XrView views[2];
    XrFrameState frame;
    XrCompositionLayerProjectionView projection[2];
    XrPosef panel;
    int running,begun,valid,render_eye,panel_open,panel_drawing,reset,audio_routed;
    float floor_offset;
    GLuint fbo;
    char error[256];
} xr={.render_eye=-1};
static TombVr vr;
#define XR_GL(ret,n,args) static ret(APIENTRY *xr_gl##n)args
XR_GL(void,GenFramebuffers,(GLsizei,GLuint*));
XR_GL(void,BindFramebuffer,(GLenum,GLuint));
XR_GL(void,DeleteFramebuffers,(GLsizei,const GLuint*));
XR_GL(void,FramebufferTexture2D,(GLenum,GLenum,GLenum,GLuint,GLint));
XR_GL(void,GenRenderbuffers,(GLsizei,GLuint*));
XR_GL(void,BindRenderbuffer,(GLenum,GLuint));
XR_GL(void,RenderbufferStorage,(GLenum,GLenum,GLsizei,GLsizei));
XR_GL(void,FramebufferRenderbuffer,(GLenum,GLenum,GLenum,GLuint));
XR_GL(void,DeleteRenderbuffers,(GLsizei,const GLuint*));
XR_GL(GLenum,CheckFramebufferStatus,(GLenum));
#undef XR_GL
static int xr_result(XrResult result,const char *operation){
    if(XR_SUCCEEDED(result))return 1;
    if(result==XR_ERROR_RUNTIME_UNAVAILABLE){snprintf(xr.error,sizeof xr.error,"OpenXR runtime unavailable. Start your VR software and connect the headset.");fprintf(stderr,"%s\n",xr.error);return 0;}
    snprintf(xr.error,sizeof xr.error,"OpenXR: %s failed (%d). Check headset connection and active runtime.",operation,(int)result);
    fprintf(stderr,"%s\n",xr.error);return 0;
}
#define XR_OK(call) xr_result((call),#call)
static void xr_close(void){
    if(xr.audio_routed)audio_route(NULL);
    if(xr_glBindFramebuffer)xr_glBindFramebuffer(0x8d40,0);
    for(int i=0;i<3;i++){
        XrTarget *t=xr.target+i;
        if(t->handle)xr_DestroySwapchain(t->handle);
        if(t->depth)xr_glDeleteRenderbuffers(1,&t->depth);
        free(t->images);
    }
    if(xr.fbo)xr_glDeleteFramebuffers(1,&xr.fbo);
    for(int i=0;i<2;i++){if(xr.hands[i])xr_DestroySpace(xr.hands[i]);if(xr.aims[i])xr_DestroySpace(xr.aims[i]);}
    if(xr.head)xr_DestroySpace(xr.head);if(xr.space)xr_DestroySpace(xr.space);
    if(xr.session)xr_DestroySession(xr.session);
    if(xr.actions)xr_DestroyActionSet(xr.actions);
    if(xr.instance)xr_DestroyInstance(xr.instance);
    if(xr.library)FreeLibrary(xr.library);
    char error[256];memcpy(error,xr.error,sizeof error);memset(&xr,0,sizeof xr);memcpy(xr.error,error,sizeof error);xr.render_eye=-1;
}
static XrPath xr_path(const char *name){XrPath p=0;xr_StringToPath(xr.instance,name,&p);return p;}
static int xr_action(XrAction *out,const char *name,XrActionType type){
    XrActionCreateInfo info={.type=XR_TYPE_ACTION_CREATE_INFO,.actionType=type,.countSubactionPaths=2,.subactionPaths=xr.paths};
    snprintf(info.actionName,sizeof info.actionName,"%s",name);snprintf(info.localizedActionName,sizeof info.localizedActionName,"%s",name);
    return XR_OK(xr_CreateAction(xr.actions,&info,out));
}
static int xr_bind(const char *profile,int index){
    XrActionSuggestedBinding bindings[20];uint32_t count=0;
    for(int h=0;h<2;h++){
        const char *root=h?"/user/hand/right":"/user/hand/left";char path[128];
        XrAction actions[]={xr.pose,xr.aim,xr.grip,xr.trigger,xr.stick,xr.primary,xr.secondary,xr.haptic};
        const char *touch[]={"input/grip/pose","input/aim/pose","input/squeeze/value","input/trigger/value","input/thumbstick",h?"input/a/click":"input/x/click",h?"input/b/click":"input/y/click","output/haptic"};
        const char *knuckles[]={"input/grip/pose","input/aim/pose","input/squeeze/value","input/trigger/value","input/thumbstick","input/a/click","input/b/click","output/haptic"};
        for(int j=0;j<8;j++){snprintf(path,sizeof path,"%s/%s",root,index?knuckles[j]:touch[j]);bindings[count++]=(XrActionSuggestedBinding){actions[j],xr_path(path)};}
    }
    XrInteractionProfileSuggestedBinding info={.type=XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,.interactionProfile=xr_path(profile),.countSuggestedBindings=count,.suggestedBindings=bindings};
    XrResult result=xr_SuggestInteractionProfileBindings(xr.instance,&info);
    return XR_SUCCEEDED(result) || result==XR_ERROR_PATH_UNSUPPORTED;
}
static int xr_open(void){
    if(xr.instance)return 1;
    /* The desktop menu regression exercises the unavailable-runtime branch.
       Keep it deterministic without waking or waiting on attached hardware. */
    if(front_test){snprintf(xr.error,sizeof xr.error,"OpenXR unavailable in desktop menu regression");return 0;}
    memset(&xr,0,sizeof xr);xr.render_eye=-1;
    xr.library=LoadLibraryExA("openxr_loader.dll",NULL,LOAD_LIBRARY_SEARCH_APPLICATION_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!xr.library){snprintf(xr.error,sizeof xr.error,"OpenXR loader missing. Place openxr_loader.dll beside the executable.");return 0;}
    FARPROC proc=GetProcAddress(xr.library,"xrGetInstanceProcAddr");memcpy(&xr_get,&proc,sizeof proc);
    if(!xr_get){snprintf(xr.error,sizeof xr.error,"Invalid OpenXR loader.");goto fail;}
    if(!XR_OK(xr_get(XR_NULL_HANDLE,"xrCreateInstance",(PFN_xrVoidFunction*)&xr_create)))goto fail;
    const char *extensions[]={XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,XR_OCULUS_AUDIO_DEVICE_GUID_EXTENSION_NAME};
    XrInstanceCreateInfo info={.type=XR_TYPE_INSTANCE_CREATE_INFO,.enabledExtensionCount=1,.enabledExtensionNames=extensions};
    PFN_xrEnumerateInstanceExtensionProperties enumerate=NULL;uint32_t extension_count=0;
    if(XR_SUCCEEDED(xr_get(XR_NULL_HANDLE,"xrEnumerateInstanceExtensionProperties",(PFN_xrVoidFunction*)&enumerate)) && XR_SUCCEEDED(enumerate(NULL,0,&extension_count,NULL))){
        XrExtensionProperties *properties=calloc(extension_count,sizeof *properties);
        if(properties){
            for(uint32_t i=0;i<extension_count;i++)properties[i].type=XR_TYPE_EXTENSION_PROPERTIES;
            if(XR_SUCCEEDED(enumerate(NULL,extension_count,&extension_count,properties)))for(uint32_t i=0;i<extension_count;i++)if(!strcmp(properties[i].extensionName,extensions[1]))info.enabledExtensionCount=2;
            free(properties);
        }
    }
    strcpy(info.applicationInfo.applicationName,"Tomb Raider Beyond");strcpy(info.applicationInfo.engineName,"TR Beyond C");info.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
    if(!XR_OK(xr_create(&info,&xr.instance)))goto fail;
#define XR_LOAD(n) if(!XR_OK(xr_get(xr.instance,"xr" #n,(PFN_xrVoidFunction*)&xr_##n)))goto fail;
    XR_FUNCTIONS(XR_LOAD)
#undef XR_LOAD
    if(!XR_OK(xr_get(xr.instance,"xrGetOpenGLGraphicsRequirementsKHR",(PFN_xrVoidFunction*)&xr_requirements)))goto fail;
    XrSystemGetInfo system_info={.type=XR_TYPE_SYSTEM_GET_INFO,.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};XrSystemId system;
    if(!XR_OK(xr_GetSystem(xr.instance,&system_info,&system)))goto fail;
    XrGraphicsRequirementsOpenGLKHR requirements={.type=XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
    if(!XR_OK(xr_requirements(xr.instance,system,&requirements)))goto fail;
    int major=0,minor=0;sscanf((const char*)glGetString(GL_VERSION),"%d.%d",&major,&minor);
    if(XR_MAKE_VERSION(major,minor,0)<requirements.minApiVersionSupported){snprintf(xr.error,sizeof xr.error,"OpenXR needs a newer OpenGL driver (current %d.%d).",major,minor);goto fail;}
#define XR_GL_LOAD(n) do{PROC p=wglGetProcAddress("gl" #n);if(!p || (uintptr_t)p<=3 || (intptr_t)p==-1){snprintf(xr.error,sizeof xr.error,"OpenGL framebuffer support unavailable.");goto fail;}memcpy(&xr_gl##n,&p,sizeof p);}while(0)
    XR_GL_LOAD(GenFramebuffers);XR_GL_LOAD(BindFramebuffer);XR_GL_LOAD(DeleteFramebuffers);XR_GL_LOAD(FramebufferTexture2D);
    XR_GL_LOAD(GenRenderbuffers);XR_GL_LOAD(BindRenderbuffer);XR_GL_LOAD(RenderbufferStorage);XR_GL_LOAD(FramebufferRenderbuffer);XR_GL_LOAD(DeleteRenderbuffers);XR_GL_LOAD(CheckFramebufferStatus);
#undef XR_GL_LOAD
    XrGraphicsBindingOpenGLWin32KHR graphics={.type=XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR,.hDC=wglGetCurrentDC(),.hGLRC=wglGetCurrentContext()};
    XrSessionCreateInfo session={.type=XR_TYPE_SESSION_CREATE_INFO,.next=&graphics,.systemId=system};
    if(!XR_OK(xr_CreateSession(xr.instance,&session,&xr.session)))goto fail;
    XrReferenceSpaceCreateInfo space={.type=XR_TYPE_REFERENCE_SPACE_CREATE_INFO,.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_STAGE,.poseInReferenceSpace={{0,0,0,1},{0,0,0}}};
    if(XR_FAILED(xr_CreateReferenceSpace(xr.session,&space,&xr.space))){space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;xr.floor_offset=1.7f;if(!XR_OK(xr_CreateReferenceSpace(xr.session,&space,&xr.space)))goto fail;}
    space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;
    if(!XR_OK(xr_CreateReferenceSpace(xr.session,&space,&xr.head)))goto fail;
    xr.paths[0]=xr_path("/user/hand/left");xr.paths[1]=xr_path("/user/hand/right");
    XrActionSetCreateInfo action_set={.type=XR_TYPE_ACTION_SET_CREATE_INFO};strcpy(action_set.actionSetName,"gameplay");strcpy(action_set.localizedActionSetName,"Gameplay and menus");
    if(!XR_OK(xr_CreateActionSet(xr.instance,&action_set,&xr.actions)))goto fail;
    if(!xr_action(&xr.pose,"hand_pose",XR_ACTION_TYPE_POSE_INPUT) || !xr_action(&xr.aim,"aim_pose",XR_ACTION_TYPE_POSE_INPUT) ||
       !xr_action(&xr.grip,"grip",XR_ACTION_TYPE_FLOAT_INPUT) || !xr_action(&xr.trigger,"trigger",XR_ACTION_TYPE_FLOAT_INPUT) ||
       !xr_action(&xr.primary,"select_jump",XR_ACTION_TYPE_BOOLEAN_INPUT) || !xr_action(&xr.secondary,"menu_back",XR_ACTION_TYPE_BOOLEAN_INPUT) ||
       !xr_action(&xr.stick,"thumbstick",XR_ACTION_TYPE_VECTOR2F_INPUT) || !xr_action(&xr.haptic,"feedback",XR_ACTION_TYPE_VIBRATION_OUTPUT))goto fail;
    if(!xr_bind("/interaction_profiles/oculus/touch_controller",0) || !xr_bind("/interaction_profiles/valve/index_controller",1))goto fail;
    XrSessionActionSetsAttachInfo attach={.type=XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,.countActionSets=1,.actionSets=&xr.actions};
    if(!XR_OK(xr_AttachSessionActionSets(xr.session,&attach)))goto fail;
    for(int h=0;h<2;h++){
        XrActionSpaceCreateInfo s={.type=XR_TYPE_ACTION_SPACE_CREATE_INFO,.action=xr.pose,.subactionPath=xr.paths[h],.poseInActionSpace={{0,0,0,1},{0,0,0}}};
        if(!XR_OK(xr_CreateActionSpace(xr.session,&s,&xr.hands[h])))goto fail;s.action=xr.aim;
        if(!XR_OK(xr_CreateActionSpace(xr.session,&s,&xr.aims[h])))goto fail;
    }
    uint32_t count=0;XrViewConfigurationView views[2]={{.type=XR_TYPE_VIEW_CONFIGURATION_VIEW},{.type=XR_TYPE_VIEW_CONFIGURATION_VIEW}};
    if(!XR_OK(xr_EnumerateViewConfigurationViews(xr.instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&count,views)) || count!=2)goto fail;
    int64_t formats[128];if(!XR_OK(xr_EnumerateSwapchainFormats(xr.session,128,&count,formats)))goto fail;
    int64_t format=0;for(uint32_t i=0;i<count;i++)if(formats[i]==0x8058)format=formats[i];
    for(uint32_t i=0;i<count;i++)if(formats[i]==0x8c43)format=formats[i]; /* Prefer sRGB storage; palette output is already encoded. */
    if(!format){snprintf(xr.error,sizeof xr.error,"OpenXR runtime does not support an RGBA8 OpenGL swapchain.");goto fail;}
    xr_glGenFramebuffers(1,&xr.fbo);
    for(int i=0;i<3;i++){
        XrTarget *t=xr.target+i;t->width=i<2?(int)views[i].recommendedImageRectWidth:1024;t->height=i<2?(int)views[i].recommendedImageRectHeight:768;
        XrSwapchainCreateInfo swap={.type=XR_TYPE_SWAPCHAIN_CREATE_INFO,.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT,.format=format,.sampleCount=1,.width=(uint32_t)t->width,.height=(uint32_t)t->height,.faceCount=1,.arraySize=1,.mipCount=1};
        if(!XR_OK(xr_CreateSwapchain(xr.session,&swap,&t->handle)) || !XR_OK(xr_EnumerateSwapchainImages(t->handle,0,&t->count,NULL)))goto fail;
        t->images=calloc(t->count,sizeof *t->images);if(!t->images)goto fail;
        for(uint32_t j=0;j<t->count;j++)t->images[j].type=XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
        if(!XR_OK(xr_EnumerateSwapchainImages(t->handle,t->count,&t->count,(XrSwapchainImageBaseHeader*)t->images)))goto fail;
        xr_glGenRenderbuffers(1,&t->depth);xr_glBindRenderbuffer(0x8d41,t->depth);xr_glRenderbufferStorage(0x8d41,0x81a6,t->width,t->height);
    }
    wchar_t endpoint[XR_MAX_AUDIO_DEVICE_STR_SIZE_OCULUS]={0};PFN_xrGetAudioOutputDeviceGuidOculus audio_guid=NULL;
    if(info.enabledExtensionCount==2 && XR_SUCCEEDED(xr_get(xr.instance,"xrGetAudioOutputDeviceGuidOculus",(PFN_xrVoidFunction*)&audio_guid)))
        if(XR_FAILED(audio_guid(xr.instance,endpoint)))endpoint[0]=0;
    audio_route(endpoint);xr.audio_routed=1;
    tomb_vr_reset(&vr,&play);return 1;
fail:xr_close();return 0;
}
static TombVrPose xr_pose(XrPosef p,int valid){return (TombVrPose){{p.position.x,p.position.y+xr.floor_offset,p.position.z},{p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w},valid};}
static TombVrPose xr_locate(XrSpace space){
    XrSpaceLocation location={.type=XR_TYPE_SPACE_LOCATION};
    if(!XR_OK(xr_LocateSpace(space,xr.space,xr.frame.predictedDisplayTime,&location)))return (TombVrPose){0};
    XrSpaceLocationFlags required=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    return xr_pose(location.pose,(location.locationFlags&required)==required);
}
static void xr_feedback(int hand,float strength){
    if(!xr.running || xr.state!=XR_SESSION_STATE_FOCUSED)return;
    XrHapticActionInfo info={.type=XR_TYPE_HAPTIC_ACTION_INFO,.action=xr.haptic,.subactionPath=xr.paths[hand]};
    XrHapticVibration vibration={.type=XR_TYPE_HAPTIC_VIBRATION,.duration=30000000,.frequency=XR_FREQUENCY_UNSPECIFIED,.amplitude=strength};
    xr_ApplyHapticFeedback(xr.session,&info,(const XrHapticBaseHeader*)&vibration);
}
static int xr_begin(TombVrInput *input){
    memset(input,0,sizeof *input);if(!xr.instance)return 0;
    XrEventDataBuffer event={.type=XR_TYPE_EVENT_DATA_BUFFER};XrResult result;
    while((result=xr_PollEvent(xr.instance,&event))==XR_SUCCESS){
        if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED){
            XrEventDataSessionStateChanged *state=(XrEventDataSessionStateChanged*)&event;xr.state=state->state;
            if(xr.state==XR_SESSION_STATE_READY){XrSessionBeginInfo begin={.type=XR_TYPE_SESSION_BEGIN_INFO,.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};xr.running=XR_OK(xr_BeginSession(xr.session,&begin));}
            if(xr.state==XR_SESSION_STATE_STOPPING){xr_EndSession(xr.session);xr.running=0;}
            if(xr.state==XR_SESSION_STATE_EXITING || xr.state==XR_SESSION_STATE_LOSS_PENDING){xr_close();return 0;}
        }
        if(event.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING){vr.ready=0;xr.panel_open=0;}
        if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING){xr_close();return 0;}
        event=(XrEventDataBuffer){.type=XR_TYPE_EVENT_DATA_BUFFER};
    }
    if(!xr.running)return 0;
    xr.frame=(XrFrameState){.type=XR_TYPE_FRAME_STATE};XrFrameWaitInfo wait={.type=XR_TYPE_FRAME_WAIT_INFO};XrFrameBeginInfo begin={.type=XR_TYPE_FRAME_BEGIN_INFO};
    if(!XR_OK(xr_WaitFrame(xr.session,&wait,&xr.frame)) || !XR_OK(xr_BeginFrame(xr.session,&begin)))return 0;xr.begun=1;
    xr.views[0]=(XrView){.type=XR_TYPE_VIEW};xr.views[1]=(XrView){.type=XR_TYPE_VIEW};
    XrViewLocateInfo locate={.type=XR_TYPE_VIEW_LOCATE_INFO,.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,.displayTime=xr.frame.predictedDisplayTime,.space=xr.space};
    XrViewState state={.type=XR_TYPE_VIEW_STATE};uint32_t count=0;
    xr.valid=XR_OK(xr_LocateViews(xr.session,&locate,&state,2,&count,xr.views)) && count==2 && (state.viewStateFlags&3)==3;
    input->head=xr_locate(xr.head);
    if(xr.state!=XR_SESSION_STATE_FOCUSED)return 1;
    XrActiveActionSet active={xr.actions,XR_NULL_PATH};XrActionsSyncInfo sync={.type=XR_TYPE_ACTIONS_SYNC_INFO,.countActiveActionSets=1,.activeActionSets=&active};
    if(!XR_OK(xr_SyncActions(xr.session,&sync)))return 1;
    for(int h=0;h<2;h++){
        XrActionStateGetInfo get={.type=XR_TYPE_ACTION_STATE_GET_INFO,.subactionPath=xr.paths[h]};
        XrAction floats[]={xr.grip,xr.trigger};float *values[]={input->grip+h,input->trigger+h};
        for(int i=0;i<2;i++){get.action=floats[i];XrActionStateFloat s={.type=XR_TYPE_ACTION_STATE_FLOAT};if(XR_SUCCEEDED(xr_GetActionStateFloat(xr.session,&get,&s)) && s.isActive)*values[i]=s.currentState;}
        XrAction booleans[]={xr.primary,xr.secondary};int *buttons[]={input->primary+h,input->secondary+h};
        for(int i=0;i<2;i++){get.action=booleans[i];XrActionStateBoolean s={.type=XR_TYPE_ACTION_STATE_BOOLEAN};if(XR_SUCCEEDED(xr_GetActionStateBoolean(xr.session,&get,&s)) && s.isActive)*buttons[i]=s.currentState;}
        get.action=xr.stick;XrActionStateVector2f stick={.type=XR_TYPE_ACTION_STATE_VECTOR2F};if(XR_SUCCEEDED(xr_GetActionStateVector2f(xr.session,&get,&stick)) && stick.isActive){input->stick[h][0]=stick.currentState.x;input->stick[h][1]=stick.currentState.y;}
        get.action=xr.pose;XrActionStatePose pose={.type=XR_TYPE_ACTION_STATE_POSE};if(XR_SUCCEEDED(xr_GetActionStatePose(xr.session,&get,&pose)) && pose.isActive)input->hand[h]=xr_locate(xr.hands[h]);
        get.action=xr.aim;pose=(XrActionStatePose){.type=XR_TYPE_ACTION_STATE_POSE};if(XR_SUCCEEDED(xr_GetActionStatePose(xr.session,&get,&pose)) && pose.isActive)input->aim[h]=xr_locate(xr.aims[h]);
    }
    return 1;
}
static int xr_target_begin(int index){
    XrTarget *t=xr.target+index;XrSwapchainImageAcquireInfo acquire={.type=XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    if(!XR_OK(xr_AcquireSwapchainImage(t->handle,&acquire,&t->index)))return 0;t->acquired=1;
    XrSwapchainImageWaitInfo wait={.type=XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,.timeout=XR_INFINITE_DURATION};if(!XR_OK(xr_WaitSwapchainImage(t->handle,&wait)))return 0;
    xr_glBindFramebuffer(0x8d40,xr.fbo);xr_glFramebufferTexture2D(0x8d40,0x8ce0,GL_TEXTURE_2D,t->images[t->index].image,0);
    glDisable(0x8db9); /* Do not encode the already sRGB palette a second time. */
    xr_glFramebufferRenderbuffer(0x8d40,0x8d00,0x8d41,t->depth);glDrawBuffer(0x8ce0);glReadBuffer(0x8ce0);
    return xr_glCheckFramebufferStatus(0x8d40)==0x8cd5;
}
static void xr_target_end(int index){
    XrTarget *t=xr.target+index;xr_glBindFramebuffer(0x8d40,0);glDrawBuffer(GL_BACK);glReadBuffer(GL_BACK);
    if(t->acquired){XrSwapchainImageReleaseInfo release={.type=XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};XR_OK(xr_ReleaseSwapchainImage(t->handle,&release));t->acquired=0;}
}
static int vr_hand_ring(void);
static void xr_end(int world,int panel){
    if(!xr.begun)return;
    XrCompositionLayerProjection projection={.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION,.space=xr.space,.viewCount=2,.views=xr.projection};
    XrCompositionLayerQuad quad={.type=XR_TYPE_COMPOSITION_LAYER_QUAD,.layerFlags=vr_hand_ring()?XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT:0,.space=xr.space,.eyeVisibility=XR_EYE_VISIBILITY_BOTH,.subImage={xr.target[2].handle,{{0,0},{xr.target[2].width,xr.target[2].height}},0},.pose=xr.panel,.size={vr_hand_ring()?.55f:1.6f,vr_hand_ring()?.4125f:1.2f}};
    const XrCompositionLayerBaseHeader *layers[2];uint32_t count=0;
    if(world)layers[count++]=(const XrCompositionLayerBaseHeader*)&projection;
    if(panel)layers[count++]=(const XrCompositionLayerBaseHeader*)&quad;
    XrFrameEndInfo end={.type=XR_TYPE_FRAME_END_INFO,.displayTime=xr.frame.predictedDisplayTime,.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE,.layerCount=count,.layers=layers};
    XR_OK(xr_EndFrame(xr.session,&end));xr.begun=0;xr.render_eye=-1;
}
