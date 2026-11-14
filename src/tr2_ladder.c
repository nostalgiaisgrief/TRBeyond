#include "tr2_runtime.h"
#include <stdlib.h>

int tr2_ladder_marked(const Tr2Level *l,const Tr2Actor *a){
    Tr2Height h;if(!tr2_query(l,a->x,a->y,a->z,a->room,&h,NULL))return 0;unsigned at=h.trigger;if(!at || at>=l->floors.count)return 0;
    int w=tr2_u16(l->floors.data+at*2);if((w&255)==5 && !(w&32768) && ++at<l->floors.count)w=tr2_u16(l->floors.data+at*2);
    return (w&255)==6 && (w&(1<<((((a->yaw+8192)&65535)>>14)+8)));
}
int tr2_ladder_test(const Tr2Level *l,int x,int y,int z,int dx,int dz,int height,int room,int *shift){
    *shift=0;Tr2Sector sector;Tr2Height h;
    if(!tr2_find_sector(l,x,y-128,z,room,&sector) || !tr2_heights(l,sector,x,y,z,0,&h) || h.floor==TR2_NO_HEIGHT)return 0;
    int floor=h.floor-y-height-128;if(floor<-70)return 0;if(floor<0)*shift=floor;
    int ceiling=h.ceiling-y;if(ceiling>70)return 0;if(ceiling>0){if(*shift)return 0;*shift=ceiling;}
    int hang=floor+height>=900;room=sector.room;
    if(!tr2_find_sector(l,x+dx,y,z+dz,room,&sector) || !tr2_heights(l,sector,x+dx,y,z+dz,0,&h))return 0;
    floor=h.floor==TR2_NO_HEIGHT?TR2_NO_HEIGHT:h.floor-y;
    if(floor>70){
        ceiling=h.ceiling-y;if(ceiling>=512)return 1;
        if(ceiling>442){if(*shift<=0){*shift=ceiling-512;return 1;}return hang?-1:0;}
        if(ceiling>0)return hang?-1:0;
        if(ceiling>-70 && hang && *shift<=0){if(*shift>ceiling)*shift=ceiling;return -1;}return 0;
    }
    if(floor>0){if(*shift<0)return 0;if(floor>*shift)*shift=floor;}
    if(!tr2_find_sector(l,x,y+height,z,room,&sector) || !tr2_find_sector(l,x+dx,y+height,z+dz,sector.room,&sector) || !tr2_heights(l,sector,x+dx,y+height,z+dz,0,&h))return 0;
    if(h.ceiling==TR2_NO_HEIGHT)return 1;ceiling=h.ceiling-y;if(ceiling<=floor || ceiling>=512)return 1;
    if(ceiling>442 && *shift<=0){*shift=ceiling-512;return 1;}return hang?-1:0;
}
static void coordinates(const Tr2Actor *a,int front,int right,int *x,int *z,int *dx,int *dz){
    int q=((a->yaw+8192)&65535)>>14;*x=a->x;*z=a->z;*dx=*dz=0;
    if(q==0){*x+=right;*z+=front;*dz=2;}else if(q==1){*x+=front;*z-=right;*dx=2;}else if(q==2){*x-=right;*z-=front;*dz=-2;}else{*x-=front;*z+=right;*dx=-2;}
}
int tr2_ladder_position(const Tr2Level *l,const Tr2Actor *a,int front,int right,int origin,int height,int *shift){
    int x,z,dx,dz;coordinates(a,front,right,&x,&z,&dx,&dz);return tr2_ladder_test(l,x,a->y+origin,z,dx,dz,height,a->room,shift);
}
int tr2_ladder_up(const Tr2Level *l,const Tr2Actor *a,int front,int right,int *shift,int *ledge){
    int x,z,dx,dz,y=a->y-768;coordinates(a,front,right,&x,&z,&dx,&dz);*shift=0;*ledge=TR2_NO_HEIGHT;Tr2Sector sec;Tr2Height h;
    if(!tr2_query(l,x,y,z,a->room,&h,&sec))return 0;int ceiling=h.ceiling-y+256;if(ceiling>70)return 0;if(ceiling>0)*shift=ceiling;
    if(!tr2_query(l,x+dx,y,z+dz,sec.room,&h,&sec))return 0;if(h.floor==TR2_NO_HEIGHT)return 1;
    int floor=h.floor-y;*ledge=floor;
    if(floor>128){ceiling=h.ceiling-y;if(ceiling>=512)return 1;if(floor-ceiling>762){*shift=floor;return -1;}return 0;}
    if(floor>0 && floor>*shift)*shift=floor;
    if(!tr2_find_sector(l,x,y+512,z,a->room,&sec) || !tr2_find_sector(l,x+dx,y+512,z+dz,sec.room,&sec) || !tr2_heights(l,sec,x+dx,y+512,z+dz,0,&h))return 0;
    ceiling=h.ceiling-y;return ceiling<=floor || ceiling>=512;
}
static int combine(int left,int right,int *shift){
    if(left && right && (left<0)!=(right<0))return 0;
    *shift=!left?right:!right?left:left<0?(left<right?left:right):(left>right?left:right);return 1;
}
int tr2_ladder_stance(const Tr2Level *l,Tr2Actor *a,int radius){
    int left,right,shift;if(tr2_ladder_position(l,a,radius,radius+120,-700,512,&right)!=1 || tr2_ladder_position(l,a,radius,-80-radius,-700,512,&left)!=1 || !combine(left,right,&shift))return 0;
    a->y+=shift;return 1;
}
int tr2_ladder_hang(const Tr2Level *l,Tr2Actor *a,const Tr2Contact *c){
    if(!tr2_ladder_marked(l,a) || a->fall_speed<0)return 0;
    int q=((a->yaw+8192)&65535)>>14;if(q&1)a->x+=c->shift[0];else a->z+=c->shift[2];
    int b[6],shift=0;if(!tr2_actor_bounds(l,a,b))return 0;
    if(!tr2_ladder_position(l,a,c->radius,c->radius,b[2],b[3]-b[2],&shift) || !tr2_ladder_position(l,a,c->radius,-c->radius,b[2],b[3]-b[2],&shift))return 0;
    int result=tr2_ladder_position(l,a,c->radius,0,b[2],b[3]-b[2],&shift);if(!result)return 0;if(result!=1)a->y+=shift;return 1;
}
int tr2_ladder_control(Tr2Actor *a,uint32_t input){
    int state=a->control.current;if(state<56 || state>61)return 0;
    if(state==56){if(input&(TR2_LEFT|1024))a->control.goal=58;else if(input&(TR2_RIGHT|2048))a->control.goal=60;else if(input&TR2_JUMP){a->control.goal=25;a->move_angle=tr2_word(a->yaw+32768);a->gun_status=0;}}
    else if(state==58 || state==60){if(!(input&(state==58?(TR2_LEFT|1024):(TR2_RIGHT|2048))))a->control.goal=56;}
    return 1;
}
static int release(const Tr2Level *l,Tr2Actor *a,Tr2Contact *c,uint32_t input){
    a->control.flags&=(uint16_t)~8u;a->fall_speed=0;Tr2Height h;if(tr2_query(l,a->x,a->y,a->z,a->room,&h,NULL))c->trigger=h.trigger;
    if((input&TR2_ACTION) && a->control.health>0)return 0;
    tr2_set_animation(l,a,34,0);a->control.current=a->control.goal=3;a->control.flags|=8;a->speed=2;a->fall_speed=1;a->gun_status=0;return 1;
}
int tr2_ladder_collision(const Tr2Level *l,Tr2Actor *a,Tr2Contact *c,uint32_t input){
    Tr2Control *s=&a->control;int state=s->current;if(state<56 || state>61)return 0;if(state==59)return 1;
    if(release(l,a,c,input))return 1;int left=0,right=0,ledge_left=0,ledge_right=0,radius=c->radius,rl,rr,shift=0;
    if(state==58 || state==60){
        a->move_angle=tr2_word(a->yaw+(state==58?-16384:16384));
        int result=tr2_ladder_position(l,a,radius,state==58?-80-radius:radius+120,-512,512,&shift);
        if(result==1){s->goal=(input&TR2_LEFT)?58:(input&TR2_RIGHT)?60:56;a->y+=shift;}
        else if(!result){a->x=c->old[0];a->z=c->old[2];s->current=s->goal=56;
            if(c->old_state!=56)tr2_set_animation(l,a,164,0);else{s->animation=c->old_animation;s->frame=c->old_frame;tr2_animate(l,a);}}
        else{s->goal=10;for(int i=0;i<120 && s->current!=10;i++)tr2_animate(l,a);a->x=c->old[0];a->z=c->old[2];}return 1;
    }
    if(state==56 && s->animation==164){
        if((input&TR2_FORWARD) && s->goal!=19){s->goal=56;
            rr=tr2_ladder_up(l,a,radius,radius+120,&right,&ledge_right);rl=tr2_ladder_up(l,a,radius,-80-radius,&left,&ledge_left);
            if(!rr || !rl)return 1;if(rr<0 || rl<0){if(abs(ledge_left-ledge_right)<=120){s->goal=19;a->y+=(ledge_left+ledge_right)/2-256;}}
            else if(combine(left,right,&shift)){s->goal=57;a->y+=shift;}
        }else if((input&TR2_BACK) && s->goal!=10){s->goal=56;a->y+=256;
            rr=tr2_ladder_position(l,a,radius,radius+120,-512,512,&right);rl=tr2_ladder_position(l,a,radius,-80-radius,-512,512,&left);a->y-=256;
            if(!rr || !rl || !combine(left,right,&shift))return 1;if(rr==1 && rl==1){s->goal=61;a->y+=shift;}else s->goal=10;
        }return 1;
    }
    if((state==57 && s->animation==161) || (state==61 && s->animation==168)){
        int frame=s->frame-tr2_i16(l->animations.data+s->animation*32+16),offset;
        if(!frame)offset=0;else if(frame==28 || frame==29)offset=256;else if(frame==57)offset=512;else return 1;
        if(state==57){offset=-offset;a->y+=offset-256;rr=tr2_ladder_up(l,a,radius,radius+120,&right,&ledge_right);rl=tr2_ladder_up(l,a,radius,-80-radius,&left,&ledge_left);a->y+=256;
            if(!rr || !rl || !(input&TR2_FORWARD)){s->goal=56;if(offset)tr2_animate(l,a);}
            else if(rr>0 && rl>0){s->goal=57;a->y-=offset;}
            else{s->goal=56;tr2_animate(l,a);if(abs(ledge_left-ledge_right)<=120){s->goal=19;a->y+=(ledge_left+ledge_right)/2-256;}}
        }else{a->y+=offset+256;rr=tr2_ladder_position(l,a,radius,radius+120,-512,512,&right);rl=tr2_ladder_position(l,a,radius,-80-radius,-512,512,&left);a->y-=256;
            if(!rr || !rl || !(input&TR2_BACK)){s->goal=56;if(offset)tr2_animate(l,a);}
            else if(!combine(left,right,&shift)){s->goal=56;tr2_animate(l,a);}
            else if(rr!=-1 && rl!=-1){s->goal=61;a->y-=offset;}
            else{s->current=56;s->goal=10;tr2_set_animation(l,a,164,0);tr2_animate(l,a);}
        }
    }return 1;
}
