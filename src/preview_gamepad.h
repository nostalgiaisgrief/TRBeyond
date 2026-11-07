/* SDL owns controllers only; Win32 continues to own the window and audio. */
#include <SDL3/SDL.h>
enum { PAD_FORWARD,PAD_BACK,PAD_LEFT,PAD_RIGHT,PAD_STEP_LEFT,PAD_STEP_RIGHT,
 PAD_WALK,PAD_JUMP,PAD_ACTION,PAD_DRAW,PAD_ROLL,PAD_LOOK,PAD_INVENTORY,
 PAD_PAUSE,PAD_SHOOT,PAD_MODE,PAD_PREVIOUS,PAD_NEXT,PAD_ACTIONS };
enum { PAD_LT=SDL_GAMEPAD_BUTTON_COUNT,PAD_RT,PAD_CODES };
/* enabled is the legacy INI name for showing the gamepad bindings page.
   It does not gate either device's gameplay input. */
typedef struct PadProfile { int enabled,vibration,sensitivity,invert,move_deadzone,look_deadzone; int binding[PAD_ACTIONS]; } PadProfile;
typedef struct PadDevice { SDL_Gamepad *handle; SDL_JoystickID id; uint64_t buttons; } PadDevice;
static PadProfile pad_profiles[3];
static int tps_aim_methods[2]={0,1},tps_aim_device;
static PadDevice pad_devices[16];
static SDL_Gamepad *pad_active;
static uint64_t pad_buttons,pad_pressed,pad_blocked;
static double pad_move_x,pad_move_z,pad_look_x,pad_look_y,pad_repeat,pad_fraction_x,pad_fraction_y;
static unsigned pad_nav,pad_nav_old;
static int pad_initialized,pad_replacing,pad_wait,pad_context=-1,pad_mode_old=-1,pad_stick_blocked;
static const char *const pad_keys[PAD_ACTIONS]={"forward","back","left","right","step_left","step_right","walk","jump","action","draw_weapon","roll","look","inventory","pause","shoot","weapon_mode","previous_weapon","next_weapon"};
static const char *const pad_names[PAD_ACTIONS]={"Forward","Back","Left","Right","Step Left","Step Right","Walk","Jump","Action","Draw Weapon","Roll","Look","Inventory","Pause","Shoot","Weapon Mode","Previous Weapon","Next Weapon"};
static const int pad_rows[3][18]={
 {PAD_FORWARD,PAD_BACK,PAD_LEFT,PAD_RIGHT,PAD_STEP_LEFT,PAD_STEP_RIGHT,PAD_WALK,PAD_JUMP,PAD_ACTION,PAD_DRAW,PAD_ROLL,PAD_LOOK,PAD_INVENTORY,PAD_PAUSE},
 {PAD_FORWARD,PAD_BACK,PAD_LEFT,PAD_RIGHT,PAD_WALK,PAD_JUMP,PAD_ACTION,PAD_DRAW,PAD_SHOOT,PAD_ROLL,PAD_INVENTORY,PAD_PAUSE},
 {PAD_FORWARD,PAD_BACK,PAD_LEFT,PAD_RIGHT,PAD_WALK,PAD_JUMP,PAD_ACTION,PAD_SHOOT,PAD_MODE,PAD_PREVIOUS,PAD_NEXT,PAD_INVENTORY,PAD_PAUSE}
};
static const int pad_counts[3]={14,12,13};
static int pad_mode(void){return play.shooter.mode<3?play.shooter.mode:0;}
static void pad_defaults(void){
 tps_aim_methods[0]=tps_aim_device=0;tps_aim_methods[1]=1;
 for(int m=0;m<3;m++){
  PadProfile *p=pad_profiles+m;memset(p,0,sizeof *p);p->vibration=1;p->sensitivity=50;p->move_deadzone=18;p->look_deadzone=15;
  for(int i=0;i<PAD_ACTIONS;i++)p->binding[i]=-1;
  p->binding[PAD_FORWARD]=SDL_GAMEPAD_BUTTON_DPAD_UP;p->binding[PAD_BACK]=SDL_GAMEPAD_BUTTON_DPAD_DOWN;
  p->binding[PAD_LEFT]=SDL_GAMEPAD_BUTTON_DPAD_LEFT;p->binding[PAD_RIGHT]=SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
  p->binding[PAD_INVENTORY]=SDL_GAMEPAD_BUTTON_BACK;p->binding[PAD_PAUSE]=SDL_GAMEPAD_BUTTON_START;
  p->binding[PAD_WALK]=m==2?SDL_GAMEPAD_BUTTON_EAST:SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
  p->binding[PAD_JUMP]=m?SDL_GAMEPAD_BUTTON_SOUTH:SDL_GAMEPAD_BUTTON_WEST;
  p->binding[PAD_ACTION]=m?SDL_GAMEPAD_BUTTON_WEST:SDL_GAMEPAD_BUTTON_SOUTH;
  if(m==TOMB_MODE_SHOOTER){
   p->binding[PAD_WALK]=SDL_GAMEPAD_BUTTON_WEST;
   p->binding[PAD_ACTION]=SDL_GAMEPAD_BUTTON_NORTH;
  }
  if(!m){p->binding[PAD_DRAW]=SDL_GAMEPAD_BUTTON_NORTH;p->binding[PAD_LOOK]=SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;p->binding[PAD_STEP_LEFT]=PAD_LT;p->binding[PAD_STEP_RIGHT]=PAD_RT;}
  else {p->binding[PAD_SHOOT]=PAD_RT;p->binding[m==1?PAD_DRAW:PAD_MODE]=PAD_LT;}
  if(m<2)p->binding[PAD_ROLL]=SDL_GAMEPAD_BUTTON_EAST;
  else {p->binding[PAD_PREVIOUS]=SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;p->binding[PAD_NEXT]=SDL_GAMEPAD_BUTTON_NORTH;}
 }
}
static void pad_settings(const char *path,int write){
 for(int device=0;device<2;device++){
  const char *section=device?"Gamepad TPS":"Shooter";
  if(write)WritePrivateProfileStringA(section,"aim_method",tps_aim_methods[device]?"Lock On":"Free Aim",path);
  else {char value[32];GetPrivateProfileStringA(section,"aim_method",device?"Lock On":"Free Aim",value,sizeof value,path);tps_aim_methods[device]=!strcmp(value,"Lock On");}
 }
 const char *sections[]={"Gamepad Original","Gamepad TPS","Gamepad FPS"};
 const char *keys[]={"enabled","vibration","look_sensitivity","invert_vertical_look","movement_deadzone","look_deadzone"};
 for(int m=0;m<3;m++){
  PadProfile *p=pad_profiles+m;int *values[]={&p->enabled,&p->vibration,&p->sensitivity,&p->invert,&p->move_deadzone,&p->look_deadzone};
  for(int i=0;i<6+PAD_ACTIONS;i++){
   int *v=i<6?values[i]:p->binding+i-6;const char *key=i<6?keys[i]:pad_keys[i-6];
   if(write){char text[24];snprintf(text,sizeof text,"%d",*v);WritePrivateProfileStringA(sections[m],key,text,path);}
   else {int n=(int)GetPrivateProfileIntA(sections[m],key,*v,path);int min=i>=6?-1:i==2?1:0,max=i>=6?PAD_CODES-1:i==2?100:i>=4?50:1;if(n>=min && n<=max)*v=n;}
  }
 }
}
static void pad_label(int code,char text[40]){
 int type=pad_active?SDL_GetGamepadType(pad_active):SDL_GAMEPAD_TYPE_UNKNOWN;
 int ps=type==SDL_GAMEPAD_TYPE_PS3 || type==SDL_GAMEPAD_TYPE_PS4 || type==SDL_GAMEPAD_TYPE_PS5;
 int xbox=type==SDL_GAMEPAD_TYPE_XBOX360 || type==SDL_GAMEPAD_TYPE_XBOXONE;
 const char *name="Unassigned";
 if(code>=0 && code<4){
  static const char *neutral[]={"Bottom Face","Right Face","Left Face","Top Face"};
  static const char *labels[]={NULL,"A","B","X","Y","Cross","Circle","Square","Triangle"};
  SDL_GamepadButtonLabel label=pad_active && type!=SDL_GAMEPAD_TYPE_UNKNOWN && type!=SDL_GAMEPAD_TYPE_STANDARD?SDL_GetGamepadButtonLabel(pad_active,(SDL_GamepadButton)code):SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
  name=label>0 && label<=SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE?labels[label]:neutral[code];
 }else switch(code){
  case SDL_GAMEPAD_BUTTON_BACK:name=ps?(type==SDL_GAMEPAD_TYPE_PS5?"Create":type==SDL_GAMEPAD_TYPE_PS3?"Select":"Share"):xbox?"View":"Back";break;
  case SDL_GAMEPAD_BUTTON_START:name=ps?(type==SDL_GAMEPAD_TYPE_PS3?"Start":"Options"):xbox?"Menu":"Start";break;
  case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:name=ps?"L1":xbox?"LB":"Left Shoulder";break;
  case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:name=ps?"R1":xbox?"RB":"Right Shoulder";break;
  case PAD_LT:name=ps?"L2":xbox?"LT":"Left Trigger";break;
  case PAD_RT:name=ps?"R2":xbox?"RT":"Right Trigger";break;
  case SDL_GAMEPAD_BUTTON_DPAD_UP:name="D-pad Up";break;case SDL_GAMEPAD_BUTTON_DPAD_DOWN:name="D-pad Down";break;
  case SDL_GAMEPAD_BUTTON_DPAD_LEFT:name="D-pad Left";break;case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:name="D-pad Right";break;
  case SDL_GAMEPAD_BUTTON_LEFT_STICK:name="Left Stick Press";break;case SDL_GAMEPAD_BUTTON_RIGHT_STICK:name="Right Stick Press";break;
  default:if(code>=0){snprintf(text,40,"Extra %d",code);return;}break;
 }snprintf(text,40,"%s",name);
}
static uint64_t pad_read(SDL_Gamepad *p){
 uint64_t bits=0;for(int i=0;i<SDL_GAMEPAD_BUTTON_COUNT;i++)if(SDL_GetGamepadButton(p,(SDL_GamepadButton)i))bits|=UINT64_C(1)<<i;
 if(SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_LEFT_TRIGGER)>8192)bits|=UINT64_C(1)<<PAD_LT;
 if(SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)>8192)bits|=UINT64_C(1)<<PAD_RT;
 return bits;
}
static int pad_action(uint64_t bits,int action){int c=pad_profiles[pad_mode()].binding[action];return c>=0 && (bits&(UINT64_C(1)<<c))!=0;}
static void pad_deadzone(double *x,double *y,int percent){
 double r=hypot(*x,*y),d=percent/100.0;if(r<=d){*x=*y=0;return;}
 double scale=(fmin(r,1)-d)/((1-d)*r);*x*=scale;*y*=scale;
}
static void pad_silence(void){if(pad_active)SDL_RumbleGamepad(pad_active,0,0,0);}
static void pad_open(SDL_JoystickID id){
 for(int i=0;i<16;i++)if(pad_devices[i].id==id)return;
 if(!SDL_IsGamepad(id))return;
 for(int i=0;i<16;i++)if(!pad_devices[i].handle){SDL_Gamepad *p=SDL_OpenGamepad(id);if(p)pad_devices[i]=(PadDevice){p,id,0};return;}
}
static void pad_init(void){
 if(!SDL_InitSubSystem(SDL_INIT_GAMEPAD)){fprintf(stderr,"Gamepads unavailable: %s\n",SDL_GetError());return;}
 pad_initialized=1;int n=0;SDL_JoystickID *ids=SDL_GetGamepads(&n);for(int i=0;i<n;i++)pad_open(ids[i]);SDL_free(ids);
}
static void pad_close(void){
 pad_silence();for(int i=0;i<16;i++)if(pad_devices[i].handle)SDL_CloseGamepad(pad_devices[i].handle);
 memset(pad_devices,0,sizeof pad_devices);pad_active=NULL;if(pad_initialized)SDL_QuitSubSystem(SDL_INIT_GAMEPAD);pad_initialized=0;
}
static void pad_frame(double dt,int focused);
