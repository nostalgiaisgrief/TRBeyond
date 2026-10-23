/* Portable source selection. Imported sources use the runtime's normalized
   TBL/BMV/WAV formats; raw PSX files are not yet supported by the loader. */
#include "asset.h"
static char install_root[MAX_PATH];
static const char *source_dirs[]={"PS1","DOS","TRR"};
static const char *source_names[]={"PS1","DOS","Remaster"};
static unsigned source_available;
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
static void source_path(char *path,const char *relative){
 char base[MAX_PATH];source_base(base,source_selected);
 snprintf(path,MAX_PATH,"%s/%s",base,relative);
}
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
 for(int i=0;i<2;i++){
  snprintf(path,sizeof path,"%s/%s",base,i?"AUDIO":"FMV");DWORD attr=GetFileAttributesA(path);if(attr==INVALID_FILE_ATTRIBUTES || !(attr&FILE_ATTRIBUTE_DIRECTORY))return 0;
 }
 snprintf(path,sizeof path,"%s/DATA/TITLEH.%s",base,source==0?"tbi":"PCX");
 return GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES;
}
static int source_choose(unsigned available,int preferred){
 if(preferred>=0 && preferred<3 && (available&(1u<<preferred)))return preferred;
 for(int i=0;i<3;i++)if(available&(1u<<i))return i;
 return 1;
}
static void source_scan(void){
 source_available=0;source_legacy=0;
 for(int i=0;i<3;i++)if(source_valid(i))source_available|=1u<<i;
 if(!(source_available&2)){source_legacy=1;if(source_valid(1))source_available|=2;else source_legacy=0;}
 char path[MAX_PATH],value[32];snprintf(path,sizeof path,"%s/settings.ini",install_root);
 GetPrivateProfileStringA("Game","source","",value,sizeof value,path);
 int preferred=-1;for(int i=0;i<3;i++)if(!strcmp(value,source_dirs[i]))preferred=i;
 source_selected=source_pending=source_choose(source_available,preferred);
}
static int source_cycle(int current,int direction){
 for(int n=0;n<3;n++){current=(current+direction+3)%3;if(source_available&(1u<<current))return current;}
 return source_pending;
}
