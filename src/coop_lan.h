#ifndef TOMB_COOP_LAN_H
#define TOMB_COOP_LAN_H
#include <stdint.h>
enum { COOP_SMALL_PACK=1u<<13, COOP_LARGE_PACK=1u<<14, COOP_MOVEMENT_MASK=0x1fff };
int coop_lan_open(const char *address);
int coop_lan_setup(uint32_t build,unsigned stage,uint32_t value,uint32_t *remote);
int coop_lan_frame(uint32_t signature,uint32_t state,uint32_t input,uint32_t *local,uint32_t *remote);
const char *coop_lan_error(void);
void coop_lan_close(void);
#endif
