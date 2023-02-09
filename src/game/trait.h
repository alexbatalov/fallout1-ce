#ifndef FALLOUT_GAME_TRAIT_H_
#define FALLOUT_GAME_TRAIT_H_

#include "plib/db/db.h"

namespace fallout {

// The maximum number of traits a player is allowed to select.
#define PC_TRAIT_MAX 2

// Available traits.
typedef enum Trait {
    TRAIT_FAST_METABOLISM,
    TRAIT_BRUISER,
    TRAIT_SMALL_FRAME,
    TRAIT_ONE_HANDER,
    TRAIT_FINESSE,
    TRAIT_KAMIKAZE,
    TRAIT_HEAVY_HANDED,
    TRAIT_FAST_SHOT,
    TRAIT_BLOODY_MESS,
    TRAIT_JINXED,
    TRAIT_GOOD_NATURED,
    TRAIT_CHEM_RELIANT,
    TRAIT_CHEM_RESISTANT,
    TRAIT_NIGHT_PERSON,
    TRAIT_SKILLED,
    TRAIT_GIFTED,
    TRAIT_COUNT,
} Trait;

int trait_init();
void trait_reset();
void trait_exit();
int trait_load(DB_FILE* stream);
int trait_save(DB_FILE* stream);
void trait_set(int trait1, int trait2);
void trait_get(int* trait1, int* trait2);
char* trait_name(int trait);
char* trait_description(int trait);
int trait_pic(int trait);
int trait_level(int trait);
int trait_adjust_stat(int stat);
int trait_adjust_skill(int skill);

} // namespace fallout

#endif /* FALLOUT_GAME_TRAIT_H_ */
