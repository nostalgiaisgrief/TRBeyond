#ifndef TR2_HOME_H
#define TR2_HOME_H
#include <stdint.h>
#include "tr2_level.h"
#include "tr2_stopwatch.h"
void tr2_home_score_path(const char *path);
const Tr2AssaultScores *tr2_home_scores(void);
unsigned tr2_home_stopwatch_ticks(void);
int tr2_home_flare_count(void);
int tr2_home_use_flare(void);
#include "tr2_world.h"
const Tr2World *tr2_level_world(void);
int tr2_level_use(int item,int request);
void tr2_level_sunset(int enabled);
int tr2_level_keys_requested(void);
int tr2_level_save(const char *path,int index);
int tr2_level_restore(const char *root,const char *file,int index,const char *save);
void tr2_level_rewards(const int rewards[27]);
void tr2_level_remember(void);
void tr2_level_completed_stats(Tr2Inventory *v,Tr2Combat *g);
void tr2_level_cut_angle(int angle);
Tr2CampaignTotals tr2_campaign_totals(void);
void tr2_level_skip(void);
int tr2_unarmed_shower_test(void);
int tr2_level_finished(void);
int tr2_level_take_autosave(void);
int tr2_wall_key_test(int key,int phase);
int tr2_wall_reward_test(void);
int tr2_wall_ammo_hud_test(void);
int tr2_wall_water_fog_test(void);
int tr2_home_campaign_exit(void);
int tr2_cut_swap_test(void);
int tr2_cut_actor_test(void);
int tr2_cut1_pistols_test(int frame);
int tr2_home_gate_test(int campaign);
int tr2_venice_exit_test(void);
int tr2_bartoli_detonator_test(int finish);
int tr2_fathoms_shark_death_test(void);
int tr2_final_level_test(int index);
int tr2_tibet_level_test(int index);
int tr2_creature_contact_test(const int *ids,unsigned count);
int tr2_venice_water_test(void);
int tr2_wall_effect_test(void);
int tr2_wall_exit_test(void);
int tr2_home_flare_pickup_test(void);
int tr2_home_flare_use_test(void);
/* Borrowed menu assets, valid only during the current level session. */
Tr2Level *tr2_home_menu_assets(unsigned pages[256]);
int tr2_level_start(const char *root,const char *filename,int index,int flags,int item,int extra,int campaign_home,int carry,unsigned options);
int tr2_level_open(const char *root,const char *filename,int level_index);
void tr2_home_close(void);
void tr2_home_update(double seconds,uint32_t input);
void tr2_home_interpolation_reset(void);
int tr2_home_interpolation_test(int fps,uint64_t *digest);
int tr2_home_dead(void);
int tr2_home_presentation_test(int death);
void tr2_home_death_input(void);
int tr2_home_return_title(void);
void tr2_draw_ammo(const Tr2Level *font,const unsigned *pages,int width,int height,int count);
void tr2_home_render(int width,int height);
void tr2_home_resolution(int setting);
void tr2_home_force43(int enabled);
void tr2_home_water_style(int style);
void tr2_home_draw_distance(int setting);
int tr2_home_distance_test(void);
void tr2_home_texture_filtering(int enabled);
void tr2_home_menu_sound(int sound);
int tr2_home_check(int moved);
int tr2_home_depth_check(int width,int height);
int tr2_home_assault_test(void);
int tr2_home_score_test(void);
int tr2_home_water_test(void);
#endif
