/* Versioned installation layout. Never overwrite an existing destination. */
static int layout_move(const char *from,const char *to){
 char old[MAX_PATH],next[MAX_PATH];
 snprintf(old,sizeof old,"%s/%s",install_root,from);
 snprintf(next,sizeof next,"%s/%s",install_root,to);
 if(GetFileAttributesA(old)==INVALID_FILE_ATTRIBUTES)return 1;
 if(GetFileAttributesA(next)!=INVALID_FILE_ATTRIBUTES)return 0;
 return MoveFileA(old,next)!=0;
}
static void layout_directory(const char *relative){
 char path[MAX_PATH];snprintf(path,sizeof path,"%s/%s",install_root,relative);CreateDirectoryA(path,NULL);
}
static int layout_prepare(void){
 layout_directory("Source");layout_directory("Source/TR1");
 layout_directory("Config");layout_directory("Custom");
 for(int i=1;i<=5;i++){
  char path[40];snprintf(path,sizeof path,"Source/TR%d",i);layout_directory(path);
  snprintf(path,sizeof path,"Config/TR%d",i);layout_directory(path);
  snprintf(path,sizeof path,"Custom/TR%d",i);layout_directory(path);
 }
 const char *editions[]={"DOS","PS1","TRR","UB"};
 for(int i=0;i<4;i++){
  char old[64],next[64];snprintf(old,sizeof old,"Source/%s",editions[i]);snprintf(next,sizeof next,"Source/TR1/%s",editions[i]);
  if(!layout_move(old,next))return 0;
 }
 layout_directory("Config");layout_directory("Config/TR1");
 char config[MAX_PATH];snprintf(config,sizeof config,"%s/Config/TR1/settings.ini",install_root);
 /* Retain the old settings as a backup; subsequent runs use only TR1's copy. */
 if(GetFileAttributesA(config)==INVALID_FILE_ATTRIBUTES){
  char old[MAX_PATH];snprintf(old,sizeof old,"%s/settings.ini",install_root);
  if(GetFileAttributesA(old)!=INVALID_FILE_ATTRIBUTES && !CopyFileA(old,config,TRUE))return 0;
 }
 layout_directory("Custom");layout_directory("Custom/TR1");
 char pattern[MAX_PATH];snprintf(pattern,sizeof pattern,"%s/Custom/*",install_root);
 WIN32_FIND_DATAA entry;HANDLE scan=FindFirstFileA(pattern,&entry);int ok=1;
 if(scan!=INVALID_HANDLE_VALUE){do{
  /* Existing custom campaigns are keyed by their numeric catalogue ID. */
  if(entry.cFileName[0]<'0' || entry.cFileName[0]>'9')continue;
  char old[MAX_PATH],next[MAX_PATH];snprintf(old,sizeof old,"Custom/%s",entry.cFileName);snprintf(next,sizeof next,"Custom/TR1/%s",entry.cFileName);
  if(!layout_move(old,next)){ok=0;break;}
 }while(FindNextFileA(scan,&entry));FindClose(scan);}
 return ok;
}
