/* Derived from DOS 0x28820 and transition helper 0x172B8, not TRX. */
#include "step.h"
#include "fixed.h"

static const TombAnim *get_anim(const TombAnimContext *c, int16_t n)
{
    return n >= 0 && (size_t)n < c->animation_count ? c->animations+n : NULL;
}
static int32_t sine(const TombAnimContext *c, int32_t angle)
{
    uint32_t a = (uint32_t)angle & 65535;
    int negative = a >= 32768;
    if (negative) a -= 32768;
    if (a > 16384) a = 32768-a;
    int32_t value = c->sine_quarter[a >> 4];
    return negative ? -value : value;
}

static int change(TombActor *a, const TombAnim *anim, const TombAnimContext *c)
{
    if (a->current == a->goal) return 0;
    for (int i = 0; i < anim->change_count; ++i) {
        int index = anim->change_index+i;
        if (index < 0 || (size_t)index >= c->change_count || !c->changes) return -1;
        const TombChange *ch = c->changes+index;
        if (ch->goal != a->goal) continue;
        for (int j = 0; j < ch->count; ++j) {
            int n = ch->index+j;
            if (n < 0 || (size_t)n >= c->range_count || !c->ranges) return -1;
            const TombRange *r = c->ranges+n;
            if (a->frame >= r->first && a->frame <= r->last) {
                a->animation = r->animation;
                a->frame = r->frame;
                return 1;
            }
        }
    }
    return 0;
}

static int commands(TombActor *a, const TombAnim *anim, TombAnimContext *c, int end)
{
    int index = anim->command_index;
    for (int i = 0; i < anim->command_count; ++i) {
        if (!c->commands || index < 0 || (size_t)index >= c->command_count) return 0;
        int16_t op = c->commands[index++];
        int count = op == 1 ? 3 : (op == 2 || op == 5 || op == 6) ? 2 : 0;
        if ((size_t)index+(size_t)count > c->command_count) return 0;
        const int16_t *p = c->commands+index;
        if (end && op == 1) {
            int32_t sn = sine(c,a->yaw), cs = sine(c,(int32_t)a->yaw+16384);
            int32_t dx = tomb_long((int64_t)cs*p[0]+(int64_t)sn*p[2]);
            int32_t dz = tomb_long((int64_t)cs*p[2]-(int64_t)sn*p[0]);
            a->x = tomb_long((int64_t)a->x+tomb_asr(dx,14));
            a->y = tomb_long((int64_t)a->y+p[1]);
            a->z = tomb_long((int64_t)a->z+tomb_asr(dz,14));
        } else if (end && op == 2) {
            a->fall_speed = p[0]; a->speed = p[1]; a->flags |= 8;
            if (c->fall_override) {
                a->fall_speed = c->fall_override;
                c->fall_override = 0;
            }
        } else if (end && op == 3) {
            c->weapon_status = 0;
        } else if (!end && (op == 5 || op == 6) && a->frame == p[0]) {
            if (!c->event) return 0;
            c->event(c->user,op,p[1],a);
        }
        index += count;
    }
    return 1;
}

int tomb_animate(TombActor *a, TombAnimContext *c)
{
    if (!a || !c || !c->animations || !c->sine_quarter) return 0;
    const TombAnim *anim = get_anim(c,a->animation);
    if (!anim) return 0;
    a->frame = tomb_word((int32_t)a->frame+1);
    if (anim->change_count > 0) {
        int result = change(a,anim,c);
        if (result < 0) return 0;
        if (result) {
            anim = get_anim(c,a->animation);
            if (!anim) return 0;
            a->current = anim->state;
        }
    }
    if (a->frame > anim->last_frame) {
        if (!commands(a,anim,c,1)) return 0;
        a->animation = anim->next_animation; a->frame = anim->next_frame;
        anim = get_anim(c,a->animation);
        if (!anim) return 0;
        a->current = anim->state;
    }
    if (!commands(a,anim,c,0)) return 0;
    int32_t frames = (int32_t)a->frame-anim->first_frame;
    if (!(a->flags & 8)) {
        int32_t v = tomb_long((int64_t)anim->velocity+(int64_t)frames*anim->acceleration);
        a->speed = tomb_word(tomb_asr(v,16));
    } else {
        int32_t before = tomb_long((int64_t)anim->velocity+(int64_t)(frames-1)*anim->acceleration);
        a->speed = tomb_word((int32_t)a->speed-tomb_asr(before,16));
        int32_t after = tomb_long((int64_t)before+anim->acceleration);
        a->speed = tomb_word((int32_t)a->speed+tomb_asr(after,16));
        a->fall_speed = tomb_word((int32_t)a->fall_speed+(a->fall_speed < 128 ? 6 : 1));
        a->y = tomb_long((int64_t)a->y+a->fall_speed);
    }
    int32_t dx = tomb_long((int64_t)sine(c,c->move_angle)*a->speed);
    int32_t dz = tomb_long((int64_t)sine(c,(int32_t)c->move_angle+16384)*a->speed);
    a->x = tomb_long((int64_t)a->x+tomb_asr(dx,14));
    a->z = tomb_long((int64_t)a->z+tomb_asr(dz,14));
    return 1;
}
