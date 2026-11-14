#include "tr2_world.h"
#include <string.h>

static void sound(Tr2Actor *a,int id){if(a->event_count<16){int n=a->event_count++;a->event_kind[n]=5;a->event_value[n]=id;}}
static int random_draw(Tr2World *w){w->hazards->random_draw=w->hazards->random_draw*0x41c64e6du+12345;return (w->hazards->random_draw>>10)&32767;}
void tr2_dynamic_light(Tr2World *w,int x,int y,int z,int falloff,int intensity){
    if(!w->hazards)return;int i=w->hazards->light_count<10?w->hazards->light_count++:0;
    w->hazards->lights[i]=(Tr2DynamicLight){x,y,z,falloff,intensity};
}
/* DoFlareLight 0x42f840; the original consumes the draw RNG, not gameplay RNG. */
int tr2_flare_light(Tr2World *w,const int p[3],int age){
    if(age>=1800)return 0;int random=random_draw(w),falloff=12,intensity=11,on=1;
    if(age<30)falloff+=12*(age-30)/60;
    else if(age>=1740){if(random>8192)falloff-=random&3;else{intensity=5;on=0;}}
    tr2_dynamic_light(w,p[0]+(random&15),p[1],p[2],falloff,intensity);return on;
}
int tr2_flare_state(int state){
    static const int states[]={0,2,4,6,7,16,20,22,21,65,39,40,41};
    for(unsigned i=0;i<sizeof states/sizeof states[0];i++)if(state==states[i])return 1;return 0;
}
static void arm(const Tr2Level *l,Tr2Combat *g,int frame){
    const uint8_t *m=tr2_model(l,9);if(!m)return;
    g->arms[0].frame=tr2_word(frame);g->rifle.control.animation=tr2_word(tr2_u16(m+16)+(frame<1?0:frame<33?1:frame<72?2:frame<95?3:4));g->rifle.control.frame=tr2_word(frame);
}
static void in_hand(const Tr2Level *l,Tr2World *w,Tr2Actor *a){
    Tr2Combat *g=w->combat;int p[3]={11,32,41};if(tr2_weapon_joint(l,a,g,13,p))g->arms[0].flash=(int16_t)tr2_flare_light(w,p,g->flare_age);
    if(g->flare_age<1800){g->flare_age++;sound(a,12);if((l->rooms[a->room].flags&1) && random_draw(w)<16384)tr2_bubble(w,p,a->room);}else if(!a->gun_status)a->gun_status=3;
}
void tr2_flare_create(const Tr2Level *l,Tr2World *w,Tr2Actor *a,int thrown){
    const uint8_t *m=tr2_model(l,152);if(!m)return;
    for(int i=0;i<TR2_MAX_DARTS;i++)if(!w->hazards->darts[i].active){
        Tr2Object *o=w->hazards->darts+i;memset(o,0,sizeof *o);o->id=152;o->active=o->status=1;
        int p[3]={-16,32,42};tr2_weapon_joint(l,a,w->combat,13,p);Tr2Actor *f=&o->pose;
        f->x=p[0];f->y=p[1];f->z=p[2];f->room=a->room;f->yaw=tr2_word(a->yaw-(thrown?0:8192));
        Tr2Height h;Tr2Sector s;if(tr2_query(l,f->x,f->y,f->z,f->room,&h,&s)){f->room=(int16_t)s.room;if(h.floor<f->y){f->x=a->x;f->z=a->z;f->yaw=tr2_word(-a->yaw);f->room=a->room;}}
        tr2_set_animation(l,f,tr2_u16(m+16),0);f->speed=tr2_word(a->speed+(thrown?50:10));f->fall_speed=tr2_word(a->fall_speed+(thrown?-50:50));
        p[0]=f->x;p[1]=f->y;p[2]=f->z;o->timer=tr2_word(w->combat->flare_age|(tr2_flare_light(w,p,w->combat->flare_age)?32768:0));return;
    }
}
static void restore(Tr2Combat *g,Tr2Actor *a){
    g->weapon=g->requested=g->last_weapon;a->gun_status=0;g->target=-1;g->meshes=0;g->flare_control=g->flare_timer=0;memset(g->arms,0,sizeof g->arms);
}
/* DrawFlare, UndrawFlare and LaraGun's held-flare branches. */
void tr2_flare_tick(const Tr2Level *l,Tr2World *w,Tr2Actor *a){
    Tr2Combat *g=w->combat;int f=g->arms[0].frame;
    if(a->gun_status==2 || a->gun_status==5){
        if(a->control.current==67 || a->control.current==39){in_hand(l,w,a);g->flare_control=0;arm(l,g,93);return;}
        g->flare_control=1;if(++f<33 || f>94)f=33;
        if(f==46){g->meshes|=1;if(w->inventory->count[151]>0)w->inventory->count[151]--;}
        else if(f>=72 && f<=93){if(f==72){sound(a,11);g->flare_age=0;}in_hand(l,w,a);}
        else if(f==94){a->gun_status=0;memset(g->arms,0,sizeof g->arms);g->target=-1;in_hand(l,w,a);f=0;}
        arm(l,g,f);return;
    }
    if(a->gun_status==3){
        g->flare_control=1;
        if(a->control.goal==2){
            if(a->control.animation==103){tr2_set_animation(l,a,189,f);g->flare_timer=a->control.frame;}
            if(a->control.animation==189){
                g->flare_control=0;int first=tr2_i16(l->animations.data+189*32+16);
                if(g->flare_timer>=first+31){restore(g,a);tr2_set_animation(l,a,11,0);a->control.current=a->control.goal=2;return;}g->flare_timer++;
            }
        }else if(a->control.current==2)tr2_set_animation(l,a,11,0);
        if(!f)f=1;
        else if(f>=72 && f<95){if(++f==94)f=1;}
        else if(f>=1 && f<33){
            if(++f==21){tr2_flare_create(l,w,a,1);g->meshes&=(uint16_t)~1u;}
            else if(f==33){restore(g,a);f=0;}
        }else if(f>=95 && f<110){if(++f==110)f=1;}
        if(f>=1 && f<21)in_hand(l,w,a);arm(l,g,f);return;
    }
    int control=!a->extra_animation && (w->vehicle>=0 || tr2_flare_state(a->control.current));
    if(!a->gun_status && control){if(!g->flare_control)f=95;else if(f && ++f==110)f=0;}
    g->flare_control=control;in_hand(l,w,a);arm(l,g,f);
}
/* FlareControl 0x430110. A spent flare remains until it comes to rest. */
void tr2_flare_world(const Tr2Level *l,Tr2World *w,Tr2Object *o){
    Tr2Actor *a=&o->pose;int x=a->x,y=a->y,z=a->z,room=a->room;
    if(a->fall_speed){a->pitch=tr2_word(a->pitch+546);a->control.roll=tr2_word(a->control.roll+910);}else a->pitch=a->control.roll=0;
    a->z+=tr2_asr(a->speed*tr2_cos(a->yaw),14);a->x+=tr2_asr(a->speed*tr2_sin(a->yaw),14);
    if(l->rooms[a->room].flags&1){a->fall_speed=tr2_word(a->fall_speed+(5-a->fall_speed)/2);a->speed=tr2_word(a->speed+(5-a->speed)/2);}else a->fall_speed=tr2_word(a->fall_speed+6);
    a->y+=a->fall_speed;Tr2Height h;Tr2Sector s;if(!tr2_query(l,a->x,a->y,a->z,a->room,&h,&s)){o->active=0;return;}
    if(a->y>=h.floor){
        if(y>h.floor){a->yaw=tr2_word(a->yaw+32768);a->speed=(int16_t)(a->speed/2);a->x=x;a->y=y;a->z=z;s.room=room;}
        else{if(a->fall_speed>40){a->fall_speed=tr2_word(40-a->fall_speed);if(a->fall_speed< -100)a->fall_speed=-100;}else{a->speed=tr2_word(a->speed-3);if(a->speed<0)a->speed=0;a->fall_speed=0;}a->y=h.floor;}
    }else if(a->y<h.ceiling){a->fall_speed=tr2_word(-a->fall_speed);a->y=h.ceiling;}
    a->room=(int16_t)s.room;int age=(uint16_t)o->timer&32767;
    if(age>=1800 && !a->fall_speed && !a->speed){o->active=0;o->status=2;return;}
    if(age<1800)age++;int p[3]={a->x,a->y,a->z};int on=tr2_flare_light(w,p,age);o->timer=tr2_word(age|(on?32768:0));if(on){sound(a,12);if((l->rooms[a->room].flags&1) && random_draw(w)<16384)tr2_bubble(w,p,a->room);}
}
