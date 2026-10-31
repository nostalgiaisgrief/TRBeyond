/* Portable source selection. Imported sources use the runtime's normalized
   TBL/BMV/WAV formats; raw PSX files are not yet supported by the loader. */
#include "asset.h"
static char install_root[MAX_PATH];
static int campaign_selected,campaign_for_load;
enum { CAMPAIGN_TR, CAMPAIGN_UB, CAMPAIGN_COMBINED };
static int campaign_level_count(void){return campaign_selected==CAMPAIGN_UB?4:campaign_selected==CAMPAIGN_COMBINED?20:16;}
static int campaign_level_at(int index){
 if(campaign_selected==CAMPAIGN_UB)return 16+index;
 if(campaign_selected==CAMPAIGN_COMBINED && index>=16){static const int tail[]={18,19,16,17};return tail[index-16];}
 return index;
}
static int campaign_level_index(int number){for(int i=0;i<campaign_level_count();i++)if(campaign_level_at(i)==number)return i;return 0;}
static void ub_path(char *path,const char *relative){snprintf(path,MAX_PATH,"%s/Source/UB/%s",install_root,relative);}
static int ub_available(void){
 const char *files[]={"DATA/EGYPT.TBL","DATA/CAT.TBL","DATA/END.TBL","DATA/END2.TBL","DATA/TITLEH.PCX","DATA/CRED0.PCX"};
 char path[MAX_PATH];for(size_t i=0;i<sizeof files/sizeof *files;i++){ub_path(path,files[i]);if(GetFileAttributesA(path)==INVALID_FILE_ATTRIBUTES)return 0;}return 1;
}
static const char *source_dirs[]={"PS1","DOS","TRR"};
static const char *source_names[]={"PS1","DOS","Remaster"};
static unsigned source_available,music_available,fmv_available;
static int music_selected=1,music_pending=1,fmv_selected=1,fmv_pending=1;
static int music_source(void){return music_selected==3?0:music_selected;}
static int source_selected=1,source_pending=1,source_legacy;
static void source_root_init(void){
 GetModuleFileNameA(NULL,install_root,MAX_PATH);
 char *end=strrchr(install_root,'\\');if(end)*end=0;
 end=strrchr(install_root,'\\');
 if(end && !_stricmp(end+1,"build"))*end=0;
}
static void source_base(char *path,int source){
 if(source_legacy && source==1)snprintf(path,MAX_PATH,"%s/work/reference-assets",install_root);
 else snprintf(path,MAX_PATH,"%s/Source/%s",install_root,source_dirs[source]);
}
static void source_path_for(char *path,const char *relative,int source){
 char base[MAX_PATH];source_base(base,source);
 snprintf(path,MAX_PATH,"%s/%s",base,relative);
}
static void source_path(char *path,const char *relative){source_path_for(path,relative,source_selected);}
static int source_valid(int source){
 static const char *levels[]={"GYM","TITLE","CUT1","CUT2","CUT3","CUT4","LEVEL1","LEVEL2","LEVEL3A","LEVEL3B","LEVEL4","LEVEL5","LEVEL6","LEVEL7A","LEVEL7B","LEVEL8A","LEVEL8B","LEVEL8C","LEVEL10A","LEVEL10B","LEVEL10C"};
 char base[MAX_PATH],path[MAX_PATH];source_base(base,source);
 for(size_t i=0;i<sizeof levels/sizeof *levels;i++){
  snprintf(path,sizeof path,"%s/DATA/%s.%s",base,levels[i],source_legacy && source==1?"PHD":"TBL");
  FILE *f=fopen(path,"rb");unsigned char version[64];
  if(!f)return 0;size_t n=fread(version,1,sizeof version,f);fclose(f);
  int legacy=source_legacy && source==1;
  int ok=legacy?n>=4 && tomb_asset_u32(version)==32:n==64 && !memcmp(version,"TRBLVL\0\0",8) && tomb_asset_u32(version+8)==1 && tomb_asset_u32(version+12)==0 && tomb_asset_u32(version+16)==(source==0?2u:1u);
  if(!ok)return 0;
 }
 snprintf(path,sizeof path,"%s/DATA/TITLEH.%s",base,source==0?"tbi":"PCX");
 return GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES;
}
static int source_choose(unsigned available,int preferred){
 if(preferred>=0 && preferred<3 && (available&(1u<<preferred)))return preferred;
 static const int order[]={1,0,2};
 for(int i=0;i<3;i++)if(available&(1u<<order[i]))return order[i];
 return 1;
}
static int source_choose_media(unsigned available,int preferred){
 if(preferred>=0 && preferred<4 && (available&(1u<<preferred)))return preferred;
 static const int order[]={0,2,1};
 for(int i=0;i<3;i++)if(available&(1u<<order[i]))return order[i];
 return 1;
}
static int source_preference(const char *path,const char *key,const char *fallback){
 char value[32];GetPrivateProfileStringA("Game",key,fallback,value,sizeof value,path);
 if(!strcmp(key,"music_speech_source") && !strcmp(value,"PS1+DOS"))return 3;
 for(int i=0;i<3;i++)if(!strcmp(value,source_dirs[i]))return i;return -1;
}
static void source_scan(void){
 source_available=0;source_legacy=0;
 for(int i=0;i<3;i++)if(source_valid(i))source_available|=1u<<i;
 if(!(source_available&2)){source_legacy=1;if(source_valid(1))source_available|=2;else source_legacy=0;}
 char path[MAX_PATH],value[32];snprintf(path,sizeof path,"%s/settings.ini",install_root);
 GetPrivateProfileStringA("Game","source","",value,sizeof value,path);
 source_selected=source_pending=source_choose(source_available,source_preference(path,"level_source",value));
 music_available=fmv_available=0;
 for(int i=0;i<3;i++){
  char file[MAX_PATH];source_path_for(file,"AUDIO/cd02.wav",i);
  if(GetFileAttributesA(file)!=INVALID_FILE_ATTRIBUTES)music_available|=1u<<i;
  source_path_for(file,i==0?"FMV/CORELOGO.bmv":"FMV/CORE.bmv",i);
  if(GetFileAttributesA(file)!=INVALID_FILE_ATTRIBUTES)fmv_available|=1u<<i;
 }
 if((music_available&3)==3)music_available|=8;
 music_selected=music_pending=source_choose_media(music_available,source_preference(path,"music_speech_source",*value?value:"PS1+DOS"));
 fmv_selected=fmv_pending=source_choose_media(fmv_available,source_preference(path,"fmv_source",value));
}
static int source_cycle_mask(int current,int direction,unsigned available){
 int previous=current;
 static const int order[]={1,0,3,2};int index=0;while(index<4 && order[index]!=current)index++;if(index==4)index=0;
 for(int n=0;n<4;n++){index=(index+direction+4)%4;current=order[index];if(available&(1u<<current))return current;}
 return previous;
}
static int source_cycle(int current,int direction){return source_cycle_mask(current,direction,source_available);}
