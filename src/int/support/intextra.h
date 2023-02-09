#ifndef FALLOUT_INT_SUPPORT_INTEXTRA_H_
#define FALLOUT_INT_SUPPORT_INTEXTRA_H_

#include "game/object_types.h"
#include "int/intrpret.h"

namespace fallout {

typedef enum ScriptError {
    SCRIPT_ERROR_NOT_IMPLEMENTED,
    SCRIPT_ERROR_OBJECT_IS_NULL,
    SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID,
    SCRIPT_ERROR_FOLLOWS,
    SCRIPT_ERROR_COUNT,
} ScriptError;

void dbg_error(Program* program, const char* name, int error);
int correctDeath(Object* critter, int anim, bool a3);
void intExtraClose();
void initIntExtra();
void updateIntExtra();
void intExtraRemoveProgramReferences(Program* program);

} // namespace fallout

#endif /* FALLOUT_INT_SUPPORT_INTEXTRA_H_ */
