#ifndef TOMB_ASSET_H
#define TOMB_ASSET_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* Beyond level v1. Sections are independent, bounded, little-endian records.
   DOS-compatible record schemas preserve exact integer/animation semantics;
   the container is not a PHD wrapper and section placement is independent. */
enum { TOMB_ASSET_SECTIONS=11 };
typedef struct TombAssetSpan { size_t offset,size; } TombAssetSpan;
typedef struct TombAsset { int version; TombAssetSpan span[TOMB_ASSET_SECTIONS]; } TombAsset;
enum { BA_TEX8,BA_ROOM,BA_FLOR,BA_MESH,BA_ANIM,BA_MODL,BA_TEXI,BA_WRLD,BA_PALT,BA_CINE,BA_SFXB };
static inline uint32_t tomb_asset_u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline uint32_t tomb_asset_checksum(const unsigned char *p,size_t n){uint32_t h=2166136261u;for(size_t i=0;i<n;i++)h=(h^p[i])*16777619u;return h;}
static inline int tomb_asset_open(TombAsset *a,const unsigned char *p,size_t n){
    static const char tags[TOMB_ASSET_SECTIONS][5]={"TEX8","ROOM","FLOR","MESH","ANIM","MODL","TEXI","WRLD","PALT","CINE","SFXB"};
    memset(a,0,sizeof *a);
    if(n<8 || memcmp(p,"TRBLVL\0\0",8))return 0;
    if(n<64 || tomb_asset_u32(p+8)!=1 || tomb_asset_u32(p+12)!=0 || tomb_asset_u32(p+16)!=1 || tomb_asset_u32(p+24)!=n || tomb_asset_u32(p+28))return -1;
    unsigned count=tomb_asset_u32(p+20),seen=0;
    if(count<TOMB_ASSET_SECTIONS || count>64 || count>(n-64)/24)return -1;
    size_t total=64+(size_t)count*24;
    for(unsigned i=0;i<count;i++){
        const unsigned char *d=p+64+i*24;size_t off=tomb_asset_u32(d+8),size=tomb_asset_u32(d+12);
        unsigned flags=tomb_asset_u32(d+20);if(flags>1 || off<64+(size_t)count*24 || off>n || size>n-off || !size || size>n-total)return -1;
        for(unsigned j=0;j<i;j++){const unsigned char *e=p+64+j*24;size_t other=tomb_asset_u32(e+8),length=tomb_asset_u32(e+12);if(off<other+length && other<off+size)return -1;}
        if(tomb_asset_checksum(p+off,size)!=tomb_asset_u32(d+16))return -1;
        int k=0;while(k<TOMB_ASSET_SECTIONS && memcmp(d,tags[k],4))k++;
        if(k==TOMB_ASSET_SECTIONS){if(flags)return -1;}
        else {if((seen&(1u<<k)) || tomb_asset_u32(d+4)!=1)return -1;seen|=1u<<k;a->span[k]=(TombAssetSpan){off,size};}
        total+=size;
    }
    if(total!=n || seen!=(1u<<TOMB_ASSET_SECTIONS)-1)return -1;
    a->version=1;return 1;
}
#endif
