#ifndef TOMB_INTERFACE_H
#define TOMB_INTERFACE_H
#include "inventory.h"
#include "text.h"
typedef struct TombHud {int health,last_health,timer;int pickup_id[3],pickup_time[3];} TombHud;
typedef void (*TombBarLine)(void *,int,int,int,int,int);
TOMB_EXPORT void tomb_hud_init(TombHud *,int health);
TOMB_EXPORT void tomb_hud_tick(TombHud *,int health);
TOMB_EXPORT void tomb_hud_pickup(TombHud *,int object);
TOMB_EXPORT int tomb_hud_health(TombHud *,int health,int weapon_status,int medipack);
TOMB_EXPORT int tomb_hud_air(int air,int water);
TOMB_EXPORT void tomb_hud_bar(int percent,int air,int width,TombBarLine,void *);
TOMB_EXPORT const char *tomb_inventory_name(int object);
TOMB_EXPORT const char *tomb_inventory_level_name(int level,int object);
TOMB_EXPORT int tomb_inventory_label(const TombInventory *,int object,char out[64]);
TOMB_EXPORT void tomb_ui_ammo(int amount,int weapon,char out[64]);
TOMB_EXPORT int tomb_level_secret_total(int level);
TOMB_EXPORT void tomb_statistics(unsigned ticks,unsigned kills,unsigned pickups,unsigned secrets,int total,char out[4][80]);
TOMB_EXPORT int tomb_death_menu_due(int,unsigned);
#endif
