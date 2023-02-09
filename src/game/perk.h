#ifndef FALLOUT_GAME_PERK_H_
#define FALLOUT_GAME_PERK_H_

#include "game/object_types.h"
#include "game/perk_defs.h"
#include "plib/db/db.h"

namespace fallout {

int perk_init();
int perk_reset();
int perk_exit();
int perk_load(DB_FILE* stream);
int perk_save(DB_FILE* stream);
int perk_add(int perk);
int perk_sub(int perk);
int perk_make_list(int* perks);
int perk_level(int perk);
char* perk_name(int perk);
char* perk_description(int perk);
void perk_add_effect(Object* critter, int perk);
void perk_remove_effect(Object* critter, int perk);
int perk_adjust_skill(int skill);

} // namespace fallout

#endif /* FALLOUT_GAME_PERK_H_ */
