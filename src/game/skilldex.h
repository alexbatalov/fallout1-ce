#ifndef FALLOUT_GAME_SKILLDEX_H_
#define FALLOUT_GAME_SKILLDEX_H_

namespace fallout {

typedef enum SkilldexRC {
    SKILLDEX_RC_ERROR = -1,
    SKILLDEX_RC_CANCELED,
    SKILLDEX_RC_SNEAK,
    SKILLDEX_RC_LOCKPICK,
    SKILLDEX_RC_STEAL,
    SKILLDEX_RC_TRAPS,
    SKILLDEX_RC_FIRST_AID,
    SKILLDEX_RC_DOCTOR,
    SKILLDEX_RC_SCIENCE,
    SKILLDEX_RC_REPAIR,
} SkilldexRC;

int skilldex_select();

} // namespace fallout

#endif /* FALLOUT_GAME_SKILLDEX_H_ */
