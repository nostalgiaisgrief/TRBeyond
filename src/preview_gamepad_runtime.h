/* Included after menu and level-loading functions. */
static void pad_cycle_weapon(int direction){
 static const int types[]={1,4,2,3};int at=0;
 for(int i=0;i<4;i++)if(types[i]==play.requested_weapon)at=i;
 for(int i=1;i<=4;i++){int next=(at+direction*i+8)%4;if(play.inventory.counts[next]){play.requested_weapon=types[next];return;}}
}
static unsigned pad_game_input(void){
 if(!pad_active)return 0;
 static const unsigned flags[PAD_ACTIONS]={1,2,4,8,1024,2048,128,16,64,0,4096,512,0,0,0,0,0,0};
 unsigned in=0;for(int i=0;i<PAD_ACTIONS;i++)if(pad_action(pad_buttons,i))in|=flags[i];
 int mode=pad_mode();
 if(mode){
  if(!(in&15)){
   int state=play.lara.actor.current,hanging=state==10 || state==30 || state==31;
   if(!hanging || fabs(pad_move_x)>fabs(pad_move_z)){
    if(pad_move_x<-.01)in|=4;if(pad_move_x>.01)in|=8;
   }
   if(!hanging || fabs(pad_move_x)<=fabs(pad_move_z)){
    if(pad_move_z>.01)in|=1;if(pad_move_z<-.01)in|=2;
   }
  }
  /* TPS uses the Action input for both interactions and firing. */
  if(pad_action(pad_buttons,PAD_SHOOT))in|=mode==2?TOMB_INPUT_SHOOT:64;
  if(pad_action(pad_buttons,mode==2?PAD_MODE:PAD_DRAW))in|=TOMB_INPUT_AIM;
 }
 return in;
}
static void pad_frame(double dt,int focused){
 if(!pad_initialized)return;
 SDL_Event e;while(SDL_PollEvent(&e))if(e.type==SDL_EVENT_GAMEPAD_ADDED)pad_open(e.gdevice.which);
 uint64_t raw=0,edges=0;
 for(int i=0;i<16;i++){
  PadDevice *d=pad_devices+i;if(!d->handle)continue;
  if(!SDL_GamepadConnected(d->handle)){
   if(pad_active==d->handle){pad_active=NULL;pad_buttons=pad_pressed=0;pad_move_x=pad_move_z=pad_look_x=pad_look_y=0;draw_requested=0;pad_replacing=1;if(!title_mode && !coop_mode)paused=1;}
   SDL_CloseGamepad(d->handle);memset(d,0,sizeof *d);continue;
  }
  uint64_t bits=pad_read(d->handle),pressed=bits&~d->buttons;d->buttons=bits;
  int menu=title_mode || inventory_open || paused;
  int menu_mode=inventory_open && inventory_ring.motion.status==8 && inventory_ring.items[inventory_ring.current].object==97?controls_page:pad_mode();
  int moved=0;
  if(!pad_active && menu_mode>0 && menu_mode<3){
   double left=hypot(SDL_GetGamepadAxis(d->handle,SDL_GAMEPAD_AXIS_LEFTX),SDL_GetGamepadAxis(d->handle,SDL_GAMEPAD_AXIS_LEFTY));
   double right=hypot(SDL_GetGamepadAxis(d->handle,SDL_GAMEPAD_AXIS_RIGHTX),SDL_GetGamepadAxis(d->handle,SDL_GAMEPAD_AXIS_RIGHTY));
   moved=menu?left>20000:(left>32767.0*pad_profiles[menu_mode].move_deadzone/100 || right>32767.0*pad_profiles[menu_mode].look_deadzone/100);
  }
  int video_skip=media_busy && (pressed&((UINT64_C(1)<<SDL_GAMEPAD_BUTTON_EAST)|(UINT64_C(1)<<SDL_GAMEPAD_BUTTON_START)));
  if(!pad_active && focused && play.shooter.mode<3 && (media_busy?video_skip:(pressed || moved)) && (!pad_replacing || video_skip || (menu && (pressed&(UINT64_C(1)<<SDL_GAMEPAD_BUTTON_SOUTH))))){
   pad_active=d->handle;pad_context=-1;pad_mode_old=-1;
   if(pad_replacing && !video_skip){pad_blocked=bits;pad_replacing=0;pressed=0;}else {pad_blocked=0;pad_replacing=0;}
  }
  if(d->handle==pad_active){raw=bits;edges=pressed;}
 }
 int mode=pad_mode(),context=media_busy?4:inventory_open?2:title_mode?3:paused?1:objects.progress->complete?5:0;
 if(context!=pad_context || mode!=pad_mode_old || !focused){
  if(pad_context>=0)pad_blocked|=(focused && context==0)?(raw&~edges):raw;
  pad_context=context;pad_mode_old=mode;pad_stick_blocked=inventory_open;pad_nav_old=0;pad_fraction_x=pad_fraction_y=0;
 }
 pad_blocked&=raw;pad_buttons=raw&~pad_blocked;pad_pressed=edges&~pad_blocked;
 pad_move_x=pad_move_z=pad_look_x=pad_look_y=0;
 if(!focused || !pad_active){pad_silence();return;}
 PadProfile *p=pad_profiles+mode;
 /* The input page being edited owns menu stick behaviour. Original never
    accepts either stick, including when its bindings are being edited. */
 int controls=inventory_open && inventory_ring.motion.status==8 && inventory_ring.items[inventory_ring.current].object==97;
 int menu_mode=controls?controls_page:mode;
 double x=0,z=0,lx=0,ly=0;
 if(menu_mode>0 && menu_mode<3){
  x=SDL_GetGamepadAxis(pad_active,SDL_GAMEPAD_AXIS_LEFTX)/32767.0;
  z=-SDL_GetGamepadAxis(pad_active,SDL_GAMEPAD_AXIS_LEFTY)/32767.0;
  lx=SDL_GetGamepadAxis(pad_active,SDL_GAMEPAD_AXIS_RIGHTX)/32767.0;
  ly=SDL_GetGamepadAxis(pad_active,SDL_GAMEPAD_AXIS_RIGHTY)/32767.0;
  pad_deadzone(&x,&z,pad_profiles[menu_mode].move_deadzone);pad_deadzone(&lx,&ly,pad_profiles[menu_mode].look_deadzone);
 }
 if(inventory_open && pad_stick_blocked){if(x==0 && z==0)pad_stick_blocked=0;x=z=0;}
 if(coop_mode){if(pad_action(pad_pressed,PAD_DRAW))draw_requested=1;return;}
 if(binding_wait){
  pad_silence();if(!pad_wait)return;
  for(int c=0;c<PAD_CODES;c++)if(pad_pressed&(UINT64_C(1)<<c)){
   PadProfile *profile=pad_profiles+controls_page;int action=pad_rows[controls_page][option_row-controls_binding_offset()],old=profile->binding[action];
   for(int a=0;a<PAD_ACTIONS;a++)if(a!=action && profile->binding[a]==c)profile->binding[a]=old;
   profile->binding[action]=c;binding_wait=pad_wait=0;pad_blocked|=raw;settings_write();break;
  }
  return;
 }
 if(media_busy){pad_silence();if(pad_pressed&((UINT64_C(1)<<SDL_GAMEPAD_BUTTON_EAST)|(UINT64_C(1)<<SDL_GAMEPAD_BUTTON_START)))media_skip=1;return;}
 if(pad_action(pad_pressed,PAD_PAUSE) && !title_mode && !inventory_open){paused=!paused;pad_blocked|=raw;pad_stick_blocked=1;pad_silence();return;}
 if(objects.progress->complete && !title_mode){
  pad_silence();if(!paused && (pad_pressed&(UINT64_C(1)<<SDL_GAMEPAD_BUTTON_SOUTH))){pad_blocked|=raw;load_next_level();}return;
 }
 if(inventory_open){
  pad_silence();unsigned nav=0;
  if(pad_buttons&(UINT64_C(1)<<SDL_GAMEPAD_BUTTON_DPAD_UP) || z>.5)nav|=16;
  if(pad_buttons&(UINT64_C(1)<<SDL_GAMEPAD_BUTTON_DPAD_DOWN) || z<-.5)nav|=32;
  if(pad_buttons&(UINT64_C(1)<<SDL_GAMEPAD_BUTTON_DPAD_LEFT) || x<-.5)nav|=TOMB_RING_LEFT;
  if(pad_buttons&(UINT64_C(1)<<SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || x>.5)nav|=TOMB_RING_RIGHT;
  pad_nav=nav;
  if(nav!=pad_nav_old){inventory_input|=nav;pad_repeat=.35;pad_nav_old=nav;}
  else if(nav){pad_repeat-=dt;if(pad_repeat<=0){inventory_input|=nav;pad_repeat=.10;}}
  if(pad_pressed&(UINT64_C(1)<<SDL_GAMEPAD_BUTTON_SOUTH))inventory_input|=TOMB_RING_SELECT;
  if(pad_pressed&(UINT64_C(1)<<SDL_GAMEPAD_BUTTON_EAST) || pad_action(pad_pressed,PAD_INVENTORY))inventory_input|=TOMB_RING_BACK;
  int right=(pad_pressed&(UINT64_C(1)<<SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))!=0,left=(pad_pressed&(UINT64_C(1)<<SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))!=0;
  if(right || left){
   if(controls){controls_page=(controls_page+(right?1:3))%4;option_row=-1;pad_stick_blocked=1;}
   else if((new_game_menu==3) || (inventory_ring.motion.status==8 && inventory_ring.items[inventory_ring.current].object==73 && !title_mode)){
    int *row=new_game_menu==3?&new_game_row:&option_row;*row=-1;inventory_input|=right?TOMB_RING_RIGHT:TOMB_RING_LEFT;
   }
  }return;
 }
 if(pad_action(pad_pressed,PAD_INVENTORY) && (play.lara.health>0 || play.dead_ticks>60 || title_mode)){death_menu=play.lara.health<=0;inventory_begin();pad_blocked|=raw;pad_silence();return;}
 if(paused || title_mode || play.shooter.mode==TOMB_MODE_VR){pad_silence();return;}
 pad_move_x=x;pad_move_z=z;pad_look_x=lx;pad_look_y=ly;
 if(!mode && pad_action(pad_pressed,PAD_DRAW))draw_requested=1;
 if(mode==2 && play.lara.health>0){if(pad_action(pad_pressed,PAD_PREVIOUS))pad_cycle_weapon(-1);if(pad_action(pad_pressed,PAD_NEXT))pad_cycle_weapon(1);}
 if(mode && play.lara.health>0){
  double speed=150.0*p->sensitivity/50.0*65536.0/360.0/24.0;
  pad_fraction_x+=lx*speed*dt;pad_fraction_y+=ly*speed*dt*(p->invert?-1:1);
  int dx=(int)pad_fraction_x,dy=(int)pad_fraction_y;pad_fraction_x-=dx;pad_fraction_y-=dy;
  tomb_shooter_mouse(&play,dx,dy);
 }
}
static void pad_feedback(int health,uint32_t shots,int falling,int speed){
 if(play.shooter.mode==TOMB_MODE_VR || !pad_active || !pad_profiles[pad_mode()].vibration)return;
 int damage=play.lara.health<health,shot=play.pistols.shots!=shots;
 int landed=falling && !(play.lara.actor.flags&8) && speed>100;
 if(damage || landed)SDL_RumbleGamepad(pad_active,40000,24000,180);
 else if(shot)SDL_RumbleGamepad(pad_active,12000,28000,75);
}
