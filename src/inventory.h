#ifndef TOMB_INVENTORY_H
#define TOMB_INVENTORY_H
#include "objects.h"
typedef struct TombInventory {
    int16_t counts[11]; /* World IDs 84..94; ammunition boxes until weapon owned. */
    int16_t scion; /* Uses the former alignment padding; saved layout unchanged. */
    /* Unused original ammo slot counts[8] stores lead bars. */
    int32_t ammo[4]; /* Pistols, shotgun pellets, magnums, uzis. */
    unsigned pickups,ticks;
    int last_pickup,pickup_ticks;
    int16_t quest[8]; /* Four puzzles, then four keys. */
    int chosen;
} TombInventory;
TOMB_EXPORT int tomb_quest_slot(int);
TOMB_EXPORT int tomb_pickup_model(int id);
TOMB_EXPORT int tomb_pickup_object(int);
TOMB_EXPORT void tomb_inventory_init(TombInventory *);
TOMB_EXPORT void tomb_inventory_confiscate(TombInventory *);
TOMB_EXPORT int tomb_inventory_add(TombInventory *,int object);
TOMB_EXPORT int tomb_inventory_use(TombInventory *,int object,int16_t *health);
/* Original PickupCollision: 0 no action, 1 animation entered, 2 collected. */
TOMB_EXPORT int tomb_pickup_contact(TombObject *,TombActor *,TombAnimContext *,uint32_t input,int water,int16_t *pitch,int16_t *roll);
struct TombPlaytest;
int tomb_pickups_tick(struct TombPlaytest *,uint32_t);
TOMB_EXPORT void tomb_pickups_replace_owned(TombObjects *,const TombInventory *);
int tomb_pickup_fixture(struct TombPlaytest *,TombObjects *,const int16_t *,int large);
#endif
