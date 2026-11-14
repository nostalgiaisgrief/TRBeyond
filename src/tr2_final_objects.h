/* Lara's extra animation uses a camera track relative to the initiating item. */
static void final_sequence(const Tr2Level *l,Tr2World *w,Tr2Actor *lara,const Tr2Actor *origin,int state,int camera_frame){
    const uint8_t *extra=tr2_model(l,12);if(!extra)return;
    lara->x=origin->x;lara->y=origin->y;lara->z=origin->z;lara->room=origin->room;
    lara->yaw=origin->yaw;lara->pitch=origin->pitch;lara->control.roll=origin->control.roll;
    lara->control.flags&=~8u;lara->speed=lara->fall_speed=0;lara->gun_status=1;
    tr2_set_animation(l,lara,tr2_u16(extra+16),0);
    lara->control.current=0;lara->control.goal=state;tr2_animate(l,lara);lara->extra_animation=1;
    w->cinematic_active=1;w->cinematic_frame=camera_frame;w->cinematic_yaw=origin->yaw;
    w->cinematic_origin=(Tr2Position){origin->x,origin->y,origin->z,origin->room,0};
    w->hit_direction=-1;
}
/* Front and back share their origin and corresponding animation frames (4182eb).
   Test the dagger against the back mesh, as in DragonCollision (417aa0). */
void tr2_dragon_contact(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *lara,uint32_t input,Tr2Contact *contact){
    if(lara->extra_animation)return;
    Tr2Actor back=o->pose;const uint8_t *front=tr2_model(l,22),*rear=tr2_model(l,23);
    if(!front || !rear)return;
    int offset=back.control.animation-tr2_u16(front+16);
    int frame=back.control.frame-tr2_i16(l->animations.data+back.control.animation*32+16);
    tr2_set_animation(l,&back,tr2_u16(rear+16)+offset,frame);
    int rear_touch=tr2_object_overlap(l,&back,lara,contact->radius) && tr2_touch(l,&back,23,lara,0);
    if(rear_touch && back.control.current==11 && (input&TR2_ACTION) && !(lara->control.flags&8) && o->effect_counter>=0 &&
       (offset==22 || (offset==23 && frame<=100))){
        int dx=lara->x-back.x,dz=lara->z-back.z,s=tr2_sin(back.yaw),c=tr2_cos(back.yaw);
        int x=tr2_asr(dx*c-dz*s,14),z=tr2_asr(dx*s+dz*c,14);
        int angle=tr2_word(lara->yaw-back.yaw);
        if(x>550 && x<=1600 && z> -350 && z<350 && angle>0x2aac && angle<0x5554){
            final_sequence(l,w,lara,&back,7,0);o->effect_counter=-1;return;
        }
    }
    if(contact->flags&8){
        if(rear_touch)tr2_object_push(l,&back,lara,contact);
        if(tr2_object_overlap(l,&o->pose,lara,contact->radius) && tr2_touch(l,&o->pose,22,lara,0))tr2_object_push(l,&o->pose,lara,contact);
    }
}
void tr2_start_shower(const Tr2Level *l,Tr2World *w,Tr2Object *counter,Tr2Actor *lara){
    if(lara->extra_animation && lara->control.current==10)return;
    counter->effect_counter=150;
    final_sequence(l,w,lara,&counter->pose,10,428);
}
static void final_attacker(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *lara){
    /* FinalLevelInitialise/Counter (4343e0/4344b0). Candidates are placed
       shotgun enemies. The PC routine defaults to the first candidate. */
    int quota=0,first=-1;
    for(unsigned i=0;i<w->count;i++){
        int id=w->objects[i].id;
        if(id==15 || id==16 || id==32)quota++;
        if(id==20 && first<0)first=i;
    }
    if(!o->effect_counter && w->inventory->kills==quota && first>=0){
        Tr2Object *boss=w->objects+first;
        boss->mesh_mask=0xffff1fff;boss->active=boss->status=1;
        tr2_enemy_enable(l,w,first);o->effect_counter=1;
    }else if(w->inventory->kills>quota && o->effect_counter<150 && ++o->effect_counter==150){
        tr2_start_shower(l,w,o,lara);
    }
}
void tr2_final_object(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *lara){
    Tr2Actor *p=&o->pose;
    if(o->id==262){final_attacker(l,w,o,lara);return;}
    if(o->id==263){
        for(unsigned i=0;i<w->count;i++)if(w->objects[i].id==262){
            o->status=w->objects[i].effect_counter>=150?1:3;
            if(o->status==1)tr2_object_animate(l,o);break;
        }
        return;
    }
    if(o->id==87){
        if(tr2_trigger_active(o->flags,&o->timer) && o->status!=2){
            Tr2Height h;Tr2Sector sector;
            if(tr2_query(l,p->x,p->y+5,p->z,p->room,&h,&sector)){
                if(h.floor<p->y+1029)o->status=2;
                else{p->y+=5;p->room=sector.room;tr2_world_room(w,(int)(o-w->objects),sector.room);}
                sound(p,204);
            }
        }
        if(tr2_object_overlap(l,p,lara,100) && tr2_touch(l,p,87,lara,0)){
            damage(lara,20);Tr2Position hit={lara->x,p->y+768,lara->z,lara->room,0};
            for(int i=0;i<3;i++)tr2_blood(w,&hit,1,p->yaw);sound(p,205);
        }
        return;
    }
    if(o->id==264){
        if(draw_random(w)<512){sound(p,298);w->camera_bounce=-200;}
        int r=control_random(w);if(r>=1024)return;
        for(int i=w->room_head[p->room];(unsigned)i<w->count;i=w->objects[i].next_room){
            Tr2Object *trap=w->objects+i;
            if(trap->id==(r<512?253:79) && trap->status!=1 && trap->status!=2){
                trap->active=trap->status=1;trap->flags=0x3e00;trap->timer=0;break;
            }
        }
        return;
    }
    if(!o->timer){if(abs(lara->x-p->x)<5120 && abs(lara->z-p->z)<5120)o->timer=1;return;}
    if(!(++o->timer&7))w->camera_bounce=o->timer;
    tr2_dragon_sparks(w,o);
    tr2_object_animate(l,o);
    if(o->timer==130 || o->timer==140 || o->timer==150){
        const int id=209+(o->timer-130)/10;const uint8_t *m=tr2_model(l,id);
        if(m)for(int i=0;i<TR2_MAX_DARTS;i++)if(!w->hazards->darts[i].active){
            Tr2Object *d=w->hazards->darts+i;*d=(Tr2Object){0};d->active=d->status=1;d->id=id;d->pose=*p;d->pose.y+=256;d->pose.yaw=0;
            tr2_set_animation(l,&d->pose,tr2_u16(m+16),0);break;
        }
    }
    if(o->timer>150){
        /* Reuse the transformation item's saved slot for the damageable front. */
        o->id=22;p->control.health=300;p->control.flags|=32;o->mesh_mask=UINT32_MAX;o->effect_counter=0;
        o->active=o->status=1;const uint8_t *m=tr2_model(l,22);
        tr2_set_animation(l,p,tr2_u16(m+16),0);p->control.current=p->control.goal=tr2_i16(l->animations.data+p->control.animation*32+6);
        tr2_enemy_enable(l,w,(int)(o-w->objects));
    }
}
