#include "game/reaction.h"

#include "game/game.h"
#include "game/object.h"
#include "game/perk.h"
#include "game/scripts.h"
#include "game/stat.h"

namespace fallout {

static int compat_scr_set_local_var(int sid, int var, int value);
static int compat_scr_get_local_var(int sid, int var, int* value);

// 0x490C40
int reaction_set(Object* critter, int value)
{
    compat_scr_set_local_var(critter->sid, 0, value);
    return 0;
}

// 0x490C54
int level_to_reaction()
{
    return 0;
}

// 0x490C58
int reaction_to_level_internal(int sid, int reaction)
{
    int level;

    if (reaction > 75) {
        compat_scr_set_local_var(sid, 1, 3);
        level = 2;
    } else if (reaction > 25) {
        compat_scr_set_local_var(sid, 1, 2);
        level = 1;
    } else {
        compat_scr_set_local_var(sid, 1, 1);
        level = 0;
    }

    return 0;
}

// 0x490CA0
int reaction_to_level(int reaction)
{
    if (reaction > 75) {
        return 2;
    } else if (reaction > 25) {
        return 1;
    } else {
        return 0;
    }
}

// 0x490CA8
int reaction_roll(int a1, int a2, int a3)
{
    return 0;
}

// 0x490CA8
int reaction_influence(int a1, int a2, int a3)
{
    return 0;
}

// 0x490CBC
int reaction_get(Object* critter)
{
    int sid;
    int v1 = 0;
    int v2 = 0;

    sid = critter->sid;
    if (compat_scr_get_local_var(sid, 2, &v1) == -1) {
        return -1;
    }

    if (v1 != 0) {
        if (compat_scr_get_local_var(sid, 0, &v2) == -1) {
            return -1;
        }

        return v2;
    }

    compat_scr_set_local_var(sid, 0, 50);
    compat_scr_set_local_var(sid, 1, 2);
    compat_scr_set_local_var(sid, 2, 1);

    if (compat_scr_get_local_var(sid, 0, &v2) == -1) {
        return -1;
    }

    compat_scr_set_local_var(sid, 0, v2 + 5 * stat_level(obj_dude, STAT_CHARISMA) - 25);

    if (compat_scr_get_local_var(sid, 0, &v2) == -1) {
        return -1;
    }

    compat_scr_set_local_var(sid, 0, v2 + 10 * perk_level(PERK_PRESENCE));

    if (perk_level(PERK_CULT_OF_PERSONALITY) > 0) {
        if (compat_scr_get_local_var(sid, 0, &v2) == -1) {
            return -1;
        }

        if (game_get_global_var(GVAR_PLAYER_REPUATION) > 0) {
            compat_scr_set_local_var(sid, 0, game_get_global_var(GVAR_PLAYER_REPUATION) + v2);
        } else {
            compat_scr_set_local_var(sid, 0, v2 - game_get_global_var(GVAR_PLAYER_REPUATION));
        }
    } else {
        if (compat_scr_get_local_var(sid, 3, &v1) == -1) {
            return -1;
        }

        if (compat_scr_get_local_var(sid, 0, &v2) == -1) {
            return -1;
        }

        if (v1 != 1) {
            compat_scr_set_local_var(sid, 0, game_get_global_var(GVAR_PLAYER_REPUATION) + v2);
        } else {
            compat_scr_set_local_var(sid, 0, v2 - game_get_global_var(GVAR_PLAYER_REPUATION));
        }
    }

    if (game_get_global_var(GVAR_CHILDKILLER_REPUATION) > 2) {
        if (compat_scr_get_local_var(sid, 0, &v2) == -1) {
            return -1;
        }

        compat_scr_set_local_var(sid, 0, v2 - 30);
    }

    if (game_get_global_var(GVAR_BAD_MONSTER) > 3 * game_get_global_var(GVAR_GOOD_MONSTER) || game_get_global_var(GVAR_CHAMPION_REPUTATION) == 1) {
        if (compat_scr_get_local_var(sid, 0, &v2) == -1) {
            return -1;
        }

        compat_scr_set_local_var(sid, 0, v2 + 20);
    }

    if (game_get_global_var(GVAR_GOOD_MONSTER) > 2 * game_get_global_var(GVAR_BAD_MONSTER) || game_get_global_var(GVAR_BERSERKER_REPUTATION) == 1) {
        if (compat_scr_get_local_var(sid, 0, &v2) == -1) {
            return -1;
        }

        compat_scr_set_local_var(sid, 0, v2 - 20);
    }

    if (compat_scr_get_local_var(sid, 0, &v2) == -1) {
        return -1;
    }

    reaction_to_level_internal(sid, v2);

    if (compat_scr_get_local_var(sid, 0, &v2) == -1) {
        return -1;
    }

    return v2;
}

static int compat_scr_set_local_var(int sid, int var, int value)
{
    ProgramValue programValue;
    programValue.opcode = VALUE_TYPE_INT;
    programValue.integerValue = value;
    return scr_set_local_var(sid, var, programValue);
}

static int compat_scr_get_local_var(int sid, int var, int* value)
{
    ProgramValue programValue;
    if (scr_get_local_var(sid, var, programValue) != 0) {
        *value = -1;
        return -1;
    }

    *value = programValue.integerValue;

    return 0;
}

} // namespace fallout
