#ifndef FALLOUT_GAME_STAT_H_
#define FALLOUT_GAME_STAT_H_

#include "game/object_types.h"
#include "game/proto_types.h"
#include "game/stat_defs.h"
#include "plib/db/db.h"

namespace fallout {

#define STAT_ERR_INVALID_STAT -5

int stat_init();
int stat_reset();
int stat_exit();
int stat_load(DB_FILE* stream);
int stat_save(DB_FILE* stream);
int stat_level(Object* critter, int stat);
int stat_get_base(Object* critter, int stat);
int stat_get_base_direct(Object* critter, int stat);
int stat_get_bonus(Object* critter, int stat);
int stat_set_base(Object* critter, int stat, int value);
int inc_stat(Object* critter, int stat);
int dec_stat(Object* critter, int stat);
int stat_set_bonus(Object* critter, int stat, int value);
void stat_set_defaults(CritterProtoData* data);
void stat_recalc_derived(Object* critter);
char* stat_name(int stat);
char* stat_description(int stat);
char* stat_level_description(int value);
int stat_pc_get(int pcStat);
int stat_pc_set(int pcStat, int value);
void stat_pc_set_defaults();
int stat_pc_min_exp();
char* stat_pc_name(int pcStat);
char* stat_pc_description(int pcStat);
int stat_picture(int stat);
int stat_result(Object* critter, int stat, int modifier, int* howMuch);
int stat_pc_add_experience(int xp);

} // namespace fallout

#endif /* FALLOUT_GAME_STAT_H_ */
