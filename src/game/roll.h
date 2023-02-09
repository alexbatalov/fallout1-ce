#ifndef FALLOUT_GAME_ROLL_H_
#define FALLOUT_GAME_ROLL_H_

#include "plib/db/db.h"

namespace fallout {

typedef enum Roll {
    ROLL_CRITICAL_FAILURE,
    ROLL_FAILURE,
    ROLL_SUCCESS,
    ROLL_CRITICAL_SUCCESS,
} Roll;

void roll_init();
int roll_reset();
int roll_exit();
int roll_save(DB_FILE* stream);
int roll_load(DB_FILE* stream);
int roll_check(int difficulty, int criticalSuccessModifier, int* howMuchPtr);
int roll_check_critical(int delta, int criticalSuccessModifier);
int roll_random(int min, int max);
void roll_set_seed(int seed);

} // namespace fallout

#endif /* FALLOUT_GAME_ROLL_H_ */
