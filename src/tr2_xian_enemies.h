/* Object-specific decisions use the shared LOT, animation, touch and missile systems. */
static void xian_animation(const Tr2Level *l,Tr2Object *o,int offset,int state){
    const uint8_t *m=tr2_model(l,o->id);if(!m)return;
    tr2_set_animation(l,&o->pose,tr2_u16(m+16)+offset,0);
    o->pose.control.current=o->pose.control.goal=state;
}
static void xian_missile(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *lara,int joint){
    int p[3]={0,0,0};
    if(o->id==22){p[0]=35;p[1]=171;p[2]=1168;}
    if(tr2_joint(l,&o->pose,o->id,joint,p)){
        Tr2Position from={p[0],p[1],p[2],o->pose.room,0};
        if(o->id==22)tr2_flame_missile(l,w,&from,lara);
        else tr2_knife_missile(l,w,&from,lara);
    }
}
static void knife_control(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Creature *c,Tr2Actor *lara,const Tr2EnemyInfo *i){
    Tr2Control *a=&o->pose.control;int can=tr2_enemy_targetable(l,&o->pose,lara,i);
    switch(a->current){
    case 1:
        c->maximum_turn=0;
        if(c->mood==2)a->goal=3;else if(can)a->goal=8;
        else if(c->mood)a->goal=i->ahead && i->distance<0x1000000?2:3;
        else if(!i->ahead || i->distance>0x2400000)a->goal=2;
        break;
    case 2:
        c->maximum_turn=546;
        if(c->mood==2)a->goal=3;
        else if(can)a->goal=i->distance<0x640000 || i->zone!=i->enemy_zone?1:enemy_random(&w->random_state)<16384?4:6;
        else if(!c->mood){if(i->ahead && i->distance<0x2400000)a->goal=1;}
        else if(!i->ahead || i->distance>0x1000000)a->goal=3;
        break;
    case 3:
        c->maximum_turn=1092;
        if(c->mood!=2){if(can)a->goal=2;else if(!c->mood)a->goal=i->ahead && i->distance<0x2400000?1:2;
            else if(i->ahead && i->distance<0x1000000)a->goal=2;}
        break;
    case 4:case 6:case 8:c->attack=0;a->goal=can?a->current+1:a->current==8?1:2;break;
    case 5:case 7:case 9:
        if(!c->attack){if(a->current!=7)xian_missile(l,w,o,lara,5);if(a->current!=5)xian_missile(l,w,o,lara,8);c->attack=1;}break;
    }
}
static void statue_control(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Creature *c,Tr2Actor *lara,const Tr2EnemyInfo *i,uint32_t touch){
    Tr2Control *a=&o->pose.control;int s=a->current;
    int near=i->bite && i->distance<0x100000,reach=i->bite && i->distance<0x400000;
    int sword=o->id==43;
    if(s==(sword?9:18)){
        if(c->attack)c->attack--;else{o->mesh_mask=(uint32_t)(2*o->mesh_mask+1);c->attack=3;}
        return;
    }
    o->mesh_mask=UINT32_MAX;
    if(sword){
        switch(s){
        case 1:c->maximum_turn=0;a->goal=lara->control.health<=0?7:near?(enemy_random(&w->random_state)<16384?3:5):i->zone!=i->enemy_zone?8:2;break;
        case 2:c->maximum_turn=910;if(lara->control.health<=0 || i->zone!=i->enemy_zone)a->goal=1;else if(reach)a->goal=10;break;
        case 8:c->maximum_turn=728;if(!c->lot.fly)a->goal=1;break;
        case 3:case 5:case 10:c->attack=0;a->goal=(s==10?reach:near)?s+1:s==10?2:1;break;
        case 4:case 6:case 11:
            if(!c->attack && (touch&0xc000)){tibet_damage(lara,300);bite_blood(l,w,o,15,0,37,550);c->attack=1;}break;
        }
    }else{
        int close=i->ahead && i->distance<0x100000,medium=i->ahead && i->distance<0x400000;
        switch(s){
        case 1:case 2:c->maximum_turn=0;
            if(!c->mood){int r=enemy_random(&w->random_state);if(r<512)a->goal=3-s;else if(r<1024)a->goal=3;}
            else a->goal=close?(s==1?5:13):3;break;
        case 3:c->maximum_turn=546;
            if(c->mood==2)a->goal=4;
            else if(!c->mood){int r=enemy_random(&w->random_state);if(r<1024)a->goal=r<512?1:2;}
            else if(medium)a->goal=i->distance<0x240000?7:enemy_random(&w->random_state)<16384?9:11;
            else if(!i->ahead || i->distance>0x900000)a->goal=4;break;
        case 4:c->maximum_turn=910;if(c->mood!=2 && medium)a->goal=15;else if(!c->mood)a->goal=1;break;
        case 5:case 7:case 9:case 11:case 13:case 15:
            c->attack=0;
            if(s==5 || s==13)a->goal=close?s+1:s==5?1:2;
            else a->goal=i->ahead && i->distance<=(s==7?0x240000:0x400000)?(s==15?16:8):3;
            break;
        case 6:case 8:case 10:case 12:case 14:case 16:{
            int alive=lara->control.health>0,damage=s==16?120:75;
            if(!(c->attack&1) && (touch&0x40000)){tibet_damage(lara,damage);bite_blood(l,w,o,18,0,0,0);c->attack|=1;}
            if(!(c->attack&2) && (touch&0x800)){tibet_damage(lara,damage);bite_blood(l,w,o,11,0,0,0);c->attack|=2;}
            if(alive && lara->control.health<=0)creature_kill(l,w,&o->pose,lara,41,49,19,2);
            break;
        }}
    }
}
static void dragon_control(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Creature *c,Tr2Actor *lara,const Tr2EnemyInfo *i,int turn,uint32_t touch){
    Tr2Control *a=&o->pose.control;
    int flame=i->ahead && i->distance>0x900000 && i->distance<0x2400000;
    if(touch)tibet_damage(lara,10);
    switch(a->current){
    case 1:c->attack=0;if(flame || (i->ahead && i->distance<0x2400000))a->goal=6;else if(turn< -182)a->goal=2;else if(turn>182)a->goal=3;break;
    case 2:if(turn> -182 || flame)a->goal=1;break;
    case 3:if(turn<182 || flame)a->goal=1;break;
    case 6:if(flame)a->goal=4;else if(!i->ahead || i->distance>0x2400000)a->goal=1;
        else if(i->distance<0x900000 && !c->attack){c->attack=1;a->goal=i->angle<0?9:10;}
        else a->goal=i->angle<0?7:8;break;
    case 7:case 8:c->attack=0;o->pose.yaw=tr2_word(o->pose.yaw-turn+(a->current==7?-182:182));break;
    case 9:case 10:if(touch&(a->current==9?0x7f000000:0xfe)){tibet_damage(lara,250);c->attack=0;}break;
    case 4:o->pose.yaw=tr2_word(o->pose.yaw-turn);if(flame){c->attack=30;a->goal=5;}else c->attack=0;break;
    case 5:o->pose.yaw=tr2_word(o->pose.yaw-turn);
        if(c->attack){if(i->ahead)xian_missile(l,w,o,lara,12);c->attack--;}else a->goal=6;break;
    }
}
static void xian_enemy_tick(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *lara){
    Tr2Object *o=w->objects+index;Tr2Actor *a=&o->pose;Tr2Creature *c=w->creatures+index;
    int box=box_at(l,a),turn=0,state=a->control.current;
    if(a->control.health<=0){
        if(o->id==41 || o->id==43){
            a->control.current=o->id==41?17:7;o->mesh_mask=(uint32_t)o->mesh_mask>>1;
            if(!o->mesh_mask){int id=o->id;o->id++;o->mesh_mask=UINT32_MAX;tr2_object_fragments(l,w,o,UINT32_MAX);o->id=id;o->status=2;tr2_enemy_finish(w,index);tr2_world_kill(w,index);}
            return;
        }
        if(o->id==22){
            if(o->effect_counter<0){
                if(o->effect_counter> -20){
                    uint32_t *rng=&w->hazards->random_draw;
                    int intensity=10+o->effect_counter/2+enemy_random(rng)/8192;
                    int falloff=12+o->effect_counter/2+enemy_random(rng)/8192;
                    tr2_dynamic_light(w,a->x,a->y,a->z,falloff,intensity);
                }
                if(o->effect_counter==-100){
                    /* Leave the skeleton at the original floor height as the body sinks. */
                    o->lift_origin=a->y;
                }
                if(o->effect_counter< -100 && o->effect_counter> -200)a->y+=10;
                if(o->effect_counter--<=-200){o->id=218;a->y=o->lift_origin;o->active=0;o->status=0;o->mesh_mask=0xff3fffff;xian_animation(l,o,0,1);tr2_enemy_disable(w,index);}
                return;
            }
            c->head=tr2_creature_head(c->head,0);
            if(state!=11 && !o->effect_counter)xian_animation(l,o,21,11);
            else{tr2_dragon_sparks(w,o);
                if(++o->effect_counter==330)a->control.goal=6;
                else if(o->effect_counter>=430){a->control.health=150;o->effect_counter=0;}
            }
        }else if(o->id==37 && state!=7)xian_animation(l,o,2,7);
        else if(o->id==19 && state!=10)xian_animation(l,o,23,10);
    }else{
        Tr2EnemyInfo i;int eb;
        if(o->id==43){c->lot.step=256;c->lot.drop=-256;c->lot.fly=0;}
        if(!tr2_enemy_info(l,&c->lot,a,lara,o->id==19?50:o->id==22?300:0,&i,&box,&eb))return;
        if(o->id==43 && state==8 && i.zone!=i.enemy_zone){
            c->lot.step=20480;c->lot.drop=-20480;c->lot.fly=64;
            tr2_enemy_info(l,&c->lot,a,lara,0,&i,&box,&eb);
        }
        tr2_enemy_target(l,c,a,lara,box,eb,o->id!=19,i.distance,&w->random_state);
        turn=tr2_creature_turn(a,c->target[0],c->target[2],o->id==37?728:o->id==22?364:c->maximum_turn);
        uint32_t touch=tr2_object_overlap(l,a,lara,100)?tr2_touch(l,a,o->id,lara,0):0;
        if(o->id==19)knife_control(l,w,o,c,lara,&i);
        else if(o->id==22)dragon_control(l,w,o,c,lara,&i,turn,touch);
        else if(o->id==37){
            if(state==1){c->attack=0;if(c->mood)a->control.goal=i.ahead && touch?4:c->mood==3?2:3;}
            else if(state==2 && c->mood){if(i.ahead && touch)a->control.goal=1;else if(c->mood!=3)a->control.goal=3;}
            else if(state==3){c->attack=0;if(i.ahead && touch)a->control.goal=1;else if(!c->mood || c->mood==3)a->control.goal=2;}
            else if(state==4 && !c->attack && touch){tibet_damage(lara,100);bite_blood(l,w,o,1,0,0,41);c->attack=1;}
        }else statue_control(l,w,o,c,lara,&i,touch);
        int angle=i.ahead?i.angle:0,head=0,neck=0;
        if(o->id==22){if(state==4 || state==5)head=-angle;}
        else if(o->id==19 || o->id==41 || o->id==43){
            int looking=o->id==19?state>=1 && state<=3:o->id==41?state>=1 && state<=4:state==1 || state==2 || state==8;
            if(looking)neck=angle;
            else if((o->id==19 && state>=4 && state<=9) || (o->id==41 && state>=5 && state<=16 && state!=6) ||
                    (o->id==43 && (state==3 || state==4 || state==5 || state==6 || state==10 || state==11)))head=angle;
        }
        c->head=tr2_creature_head(c->head,head);
        c->neck=tr2_creature_head(c->neck,neck);
    }
    a->move_angle=a->yaw;
    tr2_creature_move(l,o,&c->lot,box,turn,o->id==37 || o->id==22?341:o->id==19?102:204,c->target[1]);
    if(o->status==2)tr2_enemy_finish(w,index);
    a->control.flags&=(uint16_t)~16u;
}
