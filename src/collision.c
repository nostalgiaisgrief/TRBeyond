/* DOS collision helpers and walking orchestration. Room sampling, vaulting and
   sliding are explicit service boundaries; they must be supplied by the engine. */
#include "step.h"
#include "fixed.h"

void tomb_contact_shift(TombActor *a, TombContact *c)
{
    a->x = tomb_long((int64_t)a->x+c->shift_x);
    a->y = tomb_long((int64_t)a->y+c->shift_y);
    a->z = tomb_long((int64_t)a->z+c->shift_z);
    c->shift_x = c->shift_y = c->shift_z = 0;
}
int tomb_ceiling_response(TombActor *a, const TombContact *c)
{
    if (c->type != 8 && c->type != 32) return 0;
    a->x = c->old_x; a->y = c->old_y; a->z = c->old_z;
    a->goal = a->current = 2; a->animation = 11; a->frame = 185;
    a->speed = a->fall_speed = 0; a->flags &= (uint8_t)~8u;
    return 1;
}
int tomb_wall_response(TombActor *a, TombContact *c)
{
    if (c->type == 1 || c->type == 16) {
        tomb_contact_shift(a,c);
        a->goal = a->current = 2; a->speed = 0; a->flags &= (uint8_t)~8u;
        return 1;
    }
    if (c->type == 2 || c->type == 4) {
        tomb_contact_shift(a,c);
        a->yaw = tomb_word((int32_t)a->yaw+(c->type == 2 ? 910 : -910));
    }
    return 0;
}
int tomb_walk_collision(TombActor *a, TombContact *c, TombWalkContext *ctx)
{
    if (!a || !c || !ctx || !ctx->query || !ctx->vault || !ctx->slide) return 0;
    a->fall_speed = 0; a->flags &= (uint8_t)~8u;
    c->positive_limit = 384; c->negative_limit = -384; c->ceiling_limit = 0;
    c->facing = a->yaw; c->flags |= 7; ctx->move_angle = a->yaw;
    ctx->query(ctx->user,a,c,762);
    if (tomb_ceiling_response(a,c) || ctx->vault(ctx->user,a,c)) return 1;
    if (tomb_wall_response(a,c)) {
        if (a->frame >= 29 && a->frame <= 47) { a->animation = 3; a->frame = 74; }
        else if ((a->frame >= 22 && a->frame <= 28) || (a->frame >= 48 && a->frame <= 57)) {
            a->animation = 2; a->frame = 58;
        } else { a->animation = 11; a->frame = 185; }
    }
    if (c->floor > 384) {
        a->animation = 34; a->frame = 492; a->current = a->goal = 3;
        a->fall_speed = 0; a->flags |= 8; return 1;
    }
    if (c->floor > 128) {
        if (a->frame >= 28 && a->frame <= 45) { a->animation = 59; a->frame = 874; }
        else { a->animation = 60; a->frame = 887; }
    }
    if (c->floor >= -384 && c->floor < -128) {
        if (a->frame >= 27 && a->frame <= 44) { a->animation = 58; a->frame = 858; }
        else { a->animation = 57; a->frame = 844; }
    }
    if (!ctx->slide(ctx->user,a,c)) a->y = tomb_long((int64_t)a->y+c->floor);
    return 1;
}
