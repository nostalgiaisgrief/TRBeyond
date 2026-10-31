/* DOS inventory information 0x22ef0; HUD 0x209b8..0x20d3e;
   health/air primitives 0x10668/0x1083c; completion text 0x20124. */
#include "interface.h"
#include <stdio.h>
#include <string.h>
static int clamp(int v,int max){return v<0?0:v>max?max:v;}
void tomb_hud_init(TombHud *h,int health){memset(h,0,sizeof *h);h->last_health=h->health=clamp(health,1000);h->timer=100;}
void tomb_hud_tick(TombHud *h,int health){h->health=clamp(health,1000);if(h->timer>0)--h->timer;for(int i=0;i<3;i++)if(h->pickup_time[i]>0)--h->pickup_time[i];}
void tomb_hud_pickup(TombHud *h,int object){for(int i=0;i<3;i++)if(h->pickup_time[i]<=0){h->pickup_id[i]=object;h->pickup_time[i]=75;return;}}
int tomb_hud_health(TombHud *h,int health,int weapon,int medipack){
 health=clamp(health,1000);if(health!=h->last_health){h->last_health=health;h->timer=40;}
 if(medipack)h->timer=40;if(h->timer<0)h->timer=0;
 return h->timer>0 || !health || weapon==4?health/10:-1;
}
int tomb_hud_air(int air,int water){return water==1 || water==2?clamp(air,1800)*100/1800:-1;}
void tomb_hud_bar(int percent,int air,int width,TombBarLine line,void *u){
 if(!line || percent<0)return;percent=clamp(percent,100);int x=air?width-110:8;
 for(int y=7;y<=13;y++)line(u,x-1,y,x+101,y,0);
 line(u,x-2,14,x+102,14,17);line(u,x+102,6,x+102,14,17);
 line(u,x-2,6,x+102,6,19);line(u,x-2,6,x-2,14,19);
 const int health_colours[5]={8,11,8,6,24},air_colours[5]={32,41,32,19,21};
 if(percent)for(int y=0;y<5;y++)line(u,x,8+y,x+percent,8+y,air?air_colours[y]:health_colours[y]);
}
const char *tomb_inventory_name(int id){switch(id){case 127:return "Lead Bar";case 150:return "Scion";case 114:return "Gold Idol";case 133:return "Silver Key";case 72:return "Compass";case 99:return "Pistols";case 100:return "Shotgun";case 101:return "Magnums";case 102:return "Uzis";case 104:return "Shotgun Shells";case 105:return "Magnum Clips";case 106:return "Uzi Clips";case 108:return "Small Medi Pack";case 109:return "Large Medi Pack";default:return "";}}
const char *tomb_inventory_level_name(int level,int id){
 /* Original level-indexed strings at c1b70..c1ee0. */
 static const char *names[20][8]={
  {0},{0},{"Gold Idol",0,0,0,"Silver Key"},{"Machine Cog"},{0},
  {0,0,0,0,"Neptune Key","Atlas Key","Damocles Key","Thor Key"},
  {0,0,0,0,"Rusty Key"},{"Gold Bar"},
  {0,0,0,0,"Gold Key","Silver Key","Rusty Key"},
  {0,0,0,0,"Gold Key","Rusty Key","Rusty Key"},
  {0,0,0,0,"Saphire Key"},
  {"Eye of Horus","Scarab","Seal of Anubis","Ankh","Saphire Key"},
  {"Ankh","Scarab",0,0,"Gold Key"},{"Fuse","Pyramid Key"},{0},{0},
  {0,0,0,0,"Gold Key"},{0,0,0,0,"Ornate Key"},{0},{0}
 };
 int slot=tomb_quest_slot(id);
 if(slot>=0 && level>=0 && level<20)return names[level][slot]?names[level][slot]:"";
 return tomb_inventory_name(id);
}
static void number_glyphs(char *s){for(;*s;s++)if(*s!=' ')*s=(char)((unsigned char)*s-(*s>='A'?0x35:0x2f));}
void tomb_ui_ammo(int amount,int weapon,char out[64]){out[0]=0;if(weapon<1 || weapon>3)return;snprintf(out,64,"%5d %c",weapon==1?amount/6:amount,'A'+weapon-1);number_glyphs(out);}
int tomb_inventory_label(const TombInventory *inv,int id,char out[64]){
 out[0]=0;if(id==127){if(inv->counts[8]<=1)return 0;snprintf(out,64,"%d",inv->counts[8]);number_glyphs(out);return 1;}int quest=tomb_quest_slot(id);if(quest>=0){if(inv->quest[quest]<=1)return 0;snprintf(out,64,"%d",inv->quest[quest]);number_glyphs(out);return 1;}if(id>=100 && id<=102){tomb_ui_ammo(inv->ammo[id-99],id-99,out);return 1;}
 int amount=0;if(id>=104 && id<=106)amount=inv->counts[id-99]*2;
 else if(id==108 || id==109){amount=inv->counts[id-99];if(amount<=1)return 0;}
 else return 0;
 snprintf(out,64,"%d",amount);number_glyphs(out);return 1;
}
/* DOS completion screen reads the level-indexed table at 0xc1992. */
int tomb_level_secret_total(int level){
 static const unsigned char totals[20]={0,3,3,5,3,4,3,3,3,2,3,3,1,3,3,3,3,4,2,1};
 return level>=0 && level<20?totals[level]:0;
}
void tomb_statistics(unsigned ticks,unsigned kills,unsigned pickups,unsigned secrets,int total,char out[4][80]){
 unsigned n=0;for(unsigned bits=secrets&65535;bits;bits>>=1)n+=bits&1;
 unsigned secs=ticks/30,hours=secs/3600;
 snprintf(out[0],80,"KILLS %u",kills);snprintf(out[1],80,"PICKUPS %u",pickups&255);
 snprintf(out[2],80,"SECRETS %u of %d",n,total);
 if(hours)snprintf(out[3],80,"TIME TAKEN %u:%02u:%02u",hours,secs/60%60,secs%60);
 else snprintf(out[3],80,"TIME TAKEN %u:%02u",secs/60,secs%60);
}

/* ControlPhase 16dc4: death passport after 300 ticks, or input after 60. */
int tomb_death_menu_due(int ticks,unsigned input){return ticks>300 || (ticks>60 && input!=0);}
