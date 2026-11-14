#ifndef TR2_PRESENTATION_H
#define TR2_PRESENTATION_H
/* Tomb2.exe 421b50/421c00: -1 hides a bar, 0 draws its empty frame. */
static inline int tr2_health_bar(int health,int timer,int guns,int flash){
 if(health<0)health=0;if(health>1000)health=1000;
 if(health<=250)return flash?health/10:0;
 return timer>0 || guns==4?health/10:-1;
}
static inline int tr2_air_bar(int air,int water,int flash){
 if(water!=1 && water!=2)return -1;
 if(air<0)air=0;if(air>1800)air=1800;
 return air<=450 && !flash?0:air*100/1800;
}
/* ControlPhase 414438: Home (level zero) returns to title, not load menu. */
static inline int tr2_home_death_ready(int ticks,unsigned input){return ticks>300 || (ticks>60 && input!=0);}
/* S_PrintShadow 451040: flat octagon from animation bounds, in item space. */
static inline void tr2_shadow_points(const int b[6],int size,int p[8][2]){
 int x=(b[0]+b[1])/2,z=(b[4]+b[5])/2,dx=(b[1]-b[0])*size/1024,dz=(b[5]-b[4])*size/1024;
 const int shape[8][2]={{-1,2},{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1}};
 for(int i=0;i<8;i++){p[i][0]=x+shape[i][0]*dx;p[i][1]=z+shape[i][1]*dz;}
}
#endif
