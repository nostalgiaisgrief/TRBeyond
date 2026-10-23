/* Inventory add/remove/use reconstructed from 0x2371c..0x23ff0, 0x28b0c. */
#include "inventory.h"
#include "fixed.h"
#include <string.h>
/* DOS Item_Replace 0x24d00: all placed copies, including later acquisitions. */
void tomb_pickups_replace_owned(TombObjects *w,const TombInventory *inv){
    if(!w)return;
    for(size_t i=0;i<w->count;i++){
        TombObject *o=w->items+i;
        if(o->object>=85 && o->object<=87 && inv->counts[o->object-84])o->object+=4;
    }
}
int tomb_quest_slot(int id){if(id>=110 && id<=117)return (id-110)%4;if(id>=129 && id<=136)return 4+(id-129)%4;return -1;}
int tomb_pickup_object(int id){return id==126 || id==143 || id==144 || (id>=110 && id<=113) || (id>=129 && id<=132) || (id>=84 && id<=87) || (id>=89 && id<=91) || id==93 || id==94;}
/* World sprite IDs and their inventory-ring model counterparts. */
int tomb_pickup_model(int id){
    if((id>=84 && id<=87) || (id>=89 && id<=91) || id==93 || id==94)return id+15;
    if((id>=110 && id<=113) || (id>=129 && id<=132))return id+4;
    if(id==126)return 127;
    if(id==143 || id==144)return 150;
    return -1;
}
void tomb_inventory_init(TombInventory *i){memset(i,0,sizeof *i);i->counts[0]=1;i->ammo[0]=1000;i->last_pickup=-1;i->chosen=-1;}
int tomb_inventory_add(TombInventory *i,int id) {
    if(id==126 || id==127){++i->counts[8];return 1;}
    if(id==143 || id==144 || id==150){++i->scion;return 1;}
    int quest=tomb_quest_slot(id);if(quest>=0){++i->quest[quest];return 1;}
    if(id>=99 && id<=109)id-=15;
    if(!tomb_pickup_object(id))return 0;
    int slot=id-84;
    if(i->counts[slot]){i->counts[slot]=tomb_word(i->counts[slot]+1);return 1;}
    if(id>=85 && id<=87) {
        int box=id+4-84,amount=id==85?12:id==86?50:100,gun=id-84;
        i->ammo[gun]+=(i->counts[box]+1)*amount;i->counts[box]=0;i->counts[slot]=1;return 0;
    }
    if(id>=89 && id<=91) {
        int gun=id-88;
        if(i->counts[id-4-84])i->ammo[gun]+=id==89?12:id==90?50:100;
        else i->counts[slot]=1;
        return 0;
    }
    i->counts[slot]=1;return 1;
}
int tomb_inventory_use(TombInventory *i,int id,int16_t *health) {
    if(id==127){if(!i->counts[8])return 0;i->chosen=127;return 2;}
    int quest=tomb_quest_slot(id);if(quest>=0){if(!i->quest[quest])return 0;i->chosen=quest<4?114+quest:133+quest-4;return 2;}
    if(id==108 || id==109)id-=15;
    if((id!=93 && id!=94) || !i->counts[id-84] || *health<=0 || *health>=1000)return 0;
    int value=*health+(id==93?500:1000);*health=(int16_t)(value>1000?1000:value);--i->counts[id-84];return 1;
}

/* DOS ModifyStartInfo 3547a and InitialiseLaraInventory 28e9c. */
void tomb_inventory_confiscate(TombInventory *i){
    const int rounds[]={0,12,50,100};
    for(int gun=1;gun<4;gun++){if(i->counts[gun])i->counts[gun+4]=(int16_t)(i->ammo[gun]/rounds[gun]);i->counts[gun]=0;i->ammo[gun]=0;}
    i->counts[0]=0;i->scion=0;
}
