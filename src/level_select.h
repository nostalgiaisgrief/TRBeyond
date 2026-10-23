/* Development launcher; separate from the reconstructed DOS interface. */
#ifndef TOMB_LEVEL_SELECT_H
#define TOMB_LEVEL_SELECT_H
struct TombLevelChoice { const char *name,*file; };
static const struct TombLevelChoice level_choices[] = {
 {"Lara's Home","GYM.PHD"},
 {"Caves","LEVEL1.PHD"},
 {"City of Vilcabamba","LEVEL2.PHD"},
 {"Lost Valley","LEVEL3A.PHD"},
 {"Tomb of Qualopec","LEVEL3B.PHD"},
 {"St. Francis' Folly","LEVEL4.PHD"},
 {"Colosseum","LEVEL5.PHD"},
 {"Palace Midas","LEVEL6.PHD"},
 {"The Cistern","LEVEL7A.PHD"},
 {"Tomb of Tihocan","LEVEL7B.PHD"},
 {"City of Khamoon","LEVEL8A.PHD"},
 {"Obelisk of Khamoon","LEVEL8B.PHD"},
 {"Sanctuary of the Scion","LEVEL8C.PHD"},
 {"Natla's Mines","LEVEL10A.PHD"},
 {"Atlantis","LEVEL10B.PHD"},
 {"The Great Pyramid","LEVEL10C.PHD"}
};
#define LEVEL_CHOICE_COUNT ((int)(sizeof level_choices/sizeof level_choices[0]))
static HWND level_list;
static int level_choice_done,level_choice_result;
static void choose_level(HWND w) {
 LRESULT selected=SendMessageA(level_list,LB_GETCURSEL,0,0);
 if(selected!=LB_ERR){level_choice_result=(int)selected;level_choice_done=1;DestroyWindow(w);}
}
static LRESULT CALLBACK level_picker_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
 if(msg==WM_COMMAND){
  if(LOWORD(wp)==1 || (LOWORD(wp)==100 && HIWORD(wp)==LBN_DBLCLK)){choose_level(w);return 0;}
  if(LOWORD(wp)==2){level_choice_done=1;DestroyWindow(w);return 0;}
 }
 if(msg==WM_CLOSE){level_choice_done=1;DestroyWindow(w);return 0;}
 return DefWindowProcA(w,msg,wp,lp);
}
static int level_picker(int test) {
 HINSTANCE instance=GetModuleHandleA(NULL);
 WNDCLASSA cls={0};cls.lpfnWndProc=level_picker_proc;cls.hInstance=instance;
 cls.hCursor=LoadCursor(NULL,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);cls.lpszClassName="TombLevelPicker";
 if(!RegisterClassA(&cls))return -1;
 RECT r={0,0,440,440};AdjustWindowRect(&r,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,0);
 HWND w=CreateWindowA(cls.lpszClassName,"Tomb Raider - choose a level",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
  (GetSystemMetrics(SM_CXSCREEN)-(r.right-r.left))/2,(GetSystemMetrics(SM_CYSCREEN)-(r.bottom-r.top))/2,
  r.right-r.left,r.bottom-r.top,NULL,NULL,instance,NULL);
 if(!w)return -1;
 HWND label=CreateWindowA("STATIC","Choose a level. Later-level gameplay is still incomplete.",WS_CHILD|WS_VISIBLE,16,14,410,24,w,NULL,instance,NULL);
 level_list=CreateWindowExA(WS_EX_CLIENTEDGE,"LISTBOX","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,16,44,408,340,w,(HMENU)(INT_PTR)100,instance,NULL);
 HWND play_button=CreateWindowA("BUTTON","Play",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,234,400,90,28,w,(HMENU)(INT_PTR)1,instance,NULL);
 HWND cancel=CreateWindowA("BUTTON","Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP,334,400,90,28,w,(HMENU)(INT_PTR)2,instance,NULL);
 HWND controls[]={label,level_list,play_button,cancel};
 for(int i=0;i<4;i++)SendMessageA(controls[i],WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
 for(int i=0;i<LEVEL_CHOICE_COUNT;i++)SendMessageA(level_list,LB_ADDSTRING,0,(LPARAM)level_choices[i].name);
 SendMessageA(level_list,LB_SETCURSEL,1,0);level_choice_done=0;level_choice_result=-1;
 if(!test){ShowWindow(w,SW_SHOW);SetFocus(level_list);}
 else {
  if(SendMessageA(level_list,LB_GETCOUNT,0,0)!=LEVEL_CHOICE_COUNT){DestroyWindow(w);return -2;}
  SendMessageA(level_list,LB_SETCURSEL,LEVEL_CHOICE_COUNT-1,0);
  PostMessageA(level_list,WM_KEYDOWN,test==1?VK_RETURN:VK_ESCAPE,0);
 }
 MSG msg;
 while(!level_choice_done && GetMessageA(&msg,NULL,0,0)>0){
  if(msg.message==WM_KEYDOWN && msg.wParam==VK_RETURN){choose_level(w);continue;}
  if(msg.message==WM_KEYDOWN && msg.wParam==VK_ESCAPE){SendMessageA(w,WM_CLOSE,0,0);continue;}
  if(!IsDialogMessageA(w,&msg)){TranslateMessage(&msg);DispatchMessageA(&msg);}
 }
 UnregisterClassA(cls.lpszClassName,instance);return level_choice_result;
}
#endif
