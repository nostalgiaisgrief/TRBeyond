#include "tr2_runtime.h"
#include <stdio.h>
#include <string.h>
static int16_t sine[1025];
static int16_t angles[2049];
static int32_t octants[8];
int16_t tr2_word(int n){unsigned v=(unsigned)n&65535;return (int16_t)(v<32768?(int)v:(int)v-65536);}
int32_t tr2_long(int64_t n){uint32_t v=(uint32_t)n;return (int32_t)(v<=INT32_MAX?v:(int64_t)v-4294967296LL);}
int tr2_asr(int64_t n,int bits){int64_t d=(int64_t)1<<bits;return (int)(n>=0?n/d:-((-n+d-1)/d));}
void tr2_math_set(const int16_t *table){memcpy(sine,table,sizeof sine);}
int tr2_math_load(const char *path){
    FILE *f=fopen(path,"rb");if(!f)return 0;uint8_t data[2050];size_t n=fread(data,1,sizeof data,f);int extra=fgetc(f);fclose(f);
    if(n!=sizeof data || extra!=EOF)return 0;
    for(int i=0;i<1025;i++)sine[i]=tr2_i16(data+i*2);
    char companion[1024];size_t length=strlen(path);if(length+10>=sizeof companion)return 0;
    memcpy(companion,path,length+1);char *base=companion;
    for(char *p=companion;*p;p++)if(*p=='/' || *p=='\\')base=p+1;
    strcpy(base,"atan.bin");f=fopen(companion,"rb");if(!f)return 0;
    uint8_t atan_data[4130];n=fread(atan_data,1,sizeof atan_data,f);extra=fgetc(f);fclose(f);
    if(n!=sizeof atan_data || extra!=EOF)return 0;
    for(int i=0;i<8;i++)octants[i]=tr2_i32(atan_data+i*4);
    for(int i=0;i<2049;i++)angles[i]=tr2_i16(atan_data+32+i*2);
    return sine[0]==0 && sine[1024]==16384;
}
int tr2_sin(int angle){unsigned a=(unsigned)angle&65535;int sign=1;if(a>=32768){a-=32768;sign=-1;}if(a>16384)a=32768-a;return sign*sine[a>>4];}
int tr2_cos(int angle){return tr2_sin(angle+16384);}
int tr2_atan(int z,int x){
    if(!x && !z)return 0;int octant=0;int64_t major=z,minor=x;
    if(major<0){octant+=4;major=-major;}if(minor<0){octant+=2;minor=-minor;}
    if(minor>major){octant++;int64_t temp=major;major=minor;minor=temp;}
    int angle=angles[(minor*2048)/major]+octants[octant];return angle<0?-angle:angle;
}
