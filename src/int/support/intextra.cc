#include "int/support/intextra.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "game/actions.h"
#include "game/anim.h"
#include "game/combat.h"
#include "game/combatai.h"
#include "game/critter.h"
#include "game/display.h"
#include "game/endgame.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/gdialog.h"
#include "game/gmovie.h"
#include "game/gsound.h"
#include "game/intface.h"
#include "game/item.h"
#include "game/light.h"
#include "game/loadsave.h"
#include "game/map.h"
#include "game/object.h"
#include "game/palette.h"
#include "game/perk.h"
#include "game/protinst.h"
#include "game/proto.h"
#include "game/queue.h"
#include "game/reaction.h"
#include "game/roll.h"
#include "game/scripts.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/textobj.h"
#include "game/tile.h"
#include "game/trait.h"
#include "game/worldmap.h"
#include "int/dialog.h"
#include "plib/color/color.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/input.h"
#include "plib/gnw/rect.h"
#include "plib/gnw/vcr.h"

namespace fallout {

typedef enum Metarule {
    METARULE_SIGNAL_END_GAME = 13,
    METARULE_FIRST_RUN = 14,
    METARULE_ELEVATOR = 15,
    METARULE_PARTY_COUNT = 16,
    METARULE_IS_LOADGAME = 22,
} Metarule;

typedef enum CritterTrait {
    CRITTER_TRAIT_PERK = 0,
    CRITTER_TRAIT_OBJECT = 1,
    CRITTER_TRAIT_TRAIT = 2,
} CritterTrait;

typedef enum CritterTraitObject {
    CRITTER_TRAIT_OBJECT_AI_PACKET = 5,
    CRITTER_TRAIT_OBJECT_TEAM = 6,
    CRITTER_TRAIT_OBJECT_ROTATION = 10,
    CRITTER_TRAIT_OBJECT_IS_INVISIBLE = 666,
    CRITTER_TRAIT_OBJECT_GET_INVENTORY_WEIGHT = 669,
} CritterTraitObject;

// See `op_critter_state`.
typedef enum CritterState {
    CRITTER_STATE_NORMAL = 0x00,
    CRITTER_STATE_DEAD = 0x01,
    CRITTER_STATE_PRONE = 0x02,
} CritterState;

enum {
    INVEN_TYPE_WORN = 0,
    INVEN_TYPE_RIGHT_HAND = 1,
    INVEN_TYPE_LEFT_HAND = 2,
    INVEN_TYPE_INV_COUNT = -2,
};

typedef enum FloatingMessageType {
    FLOATING_MESSAGE_TYPE_WARNING = -2,
    FLOATING_MESSAGE_TYPE_COLOR_SEQUENCE = -1,
    FLOATING_MESSAGE_TYPE_NORMAL = 0,
    FLOATING_MESSAGE_TYPE_BLACK,
    FLOATING_MESSAGE_TYPE_RED,
    FLOATING_MESSAGE_TYPE_GREEN,
    FLOATING_MESSAGE_TYPE_BLUE,
    FLOATING_MESSAGE_TYPE_PURPLE,
    FLOATING_MESSAGE_TYPE_NEAR_WHITE,
    FLOATING_MESSAGE_TYPE_LIGHT_RED,
    FLOATING_MESSAGE_TYPE_YELLOW,
    FLOATING_MESSAGE_TYPE_WHITE,
    FLOATING_MESSAGE_TYPE_GREY,
    FLOATING_MESSAGE_TYPE_DARK_GREY,
    FLOATING_MESSAGE_TYPE_LIGHT_GREY,
    FLOATING_MESSAGE_TYPE_COUNT,
} FloatingMessageType;

typedef enum OpRegAnimFunc {
    OP_REG_ANIM_FUNC_BEGIN = 1,
    OP_REG_ANIM_FUNC_CLEAR = 2,
    OP_REG_ANIM_FUNC_END = 3,
} OpRegAnimFunc;

// TODO: Remove.
// 0x4F4144
char _aCritter[] = "<Critter>";

// NOTE: This value is a little bit odd. It's used to handle 2 operations:
// [op_start_gdialog] and [op_dialogue_reaction]. It's not used outside those
// functions.
//
// When used inside [op_start_gdialog] this value stores [Fidget] constant
// (1 - Good, 4 - Neutral, 7 - Bad).
//
// When used inside [op_dialogue_reaction] this value contains specified
// reaction (-1 - Good, 0 - Neutral, 1 - Bad).
//
// 0x5970D0
static int dialogue_mood;

// 0x44B5A8
void dbg_error(Program* program, const char* name, int error)
{
    // 0ч5054C0
    static const char* dbg_error_strs[SCRIPT_ERROR_COUNT] = {
        "unimped",
        "obj is NULL",
        "can't match program to sid",
        "follows",
    };

    char string[260];

    snprintf(string, sizeof(string), "Script Error: %s: op_%s: %s", program->name, name, dbg_error_strs[error]);

    debug_printf(string);
}

// 0x44B5E4
static void int_debug(const char* format, ...)
{
    char string[260];

    va_list argptr;
    va_start(argptr, format);
    vsnprintf(string, sizeof(string), format, argptr);
    va_end(argptr);

    debug_printf(string);
}

// 0x44B624
static int scripts_tile_is_visible(int tile)
{
    if (abs(tile_center_tile - tile) % 200 < 5) {
        return 1;
    }

    if (abs(tile_center_tile - tile) / 200 < 5) {
        return 1;
    }

    return 0;
}

// 0x44B674
static int correctFidForRemovedItem(Object* critter, Object* item, int flags)
{
    if (critter == obj_dude) {
        intface_update_items(true);
    }

    int fid = critter->fid;
    int anim = (fid & 0xF000) >> 12;
    int newFid = -1;

    if ((flags & OBJECT_IN_ANY_HAND) != 0) {
        if (critter == obj_dude) {
            if (intface_is_item_right_hand()) {
                if ((flags & OBJECT_IN_RIGHT_HAND) != 0) {
                    anim = 0;
                }
            } else {
                if ((flags & OBJECT_IN_LEFT_HAND) != 0) {
                    anim = 0;
                }
            }
        } else {
            if ((flags & OBJECT_IN_RIGHT_HAND) != 0) {
                anim = 0;
            }
        }

        if (anim == 0) {
            newFid = art_id(FID_TYPE(fid), fid & 0xFFF, FID_ANIM_TYPE(fid), 0, (fid & 0x70000000) >> 28);
        }
    } else {
        if (critter == obj_dude) {
            newFid = art_id(FID_TYPE(fid), art_vault_guy_num, FID_ANIM_TYPE(fid), anim, (fid & 0x70000000) >> 28);
            adjust_ac(obj_dude, item, NULL);
        }
    }

    if (newFid != -1) {
        Rect rect;
        obj_change_fid(critter, newFid, &rect);
        tile_refresh_rect(&rect, map_elevation);
    }

    return 0;
}

// 0x44B78C
static void op_give_exp_points(Program* program)
{
    int xp = programStackPopInteger(program);

    if (stat_pc_add_experience(xp) != 0) {
        int_debug("\nScript Error: %s: op_give_exp_points: stat_pc_set failed");
    }
}

// 0x44B7E0
static void op_scr_return(Program* program)
{
    int data = programStackPopInteger(program);

    int sid = scr_find_sid_from_program(program);

    Script* script;
    if (scr_ptr(sid, &script) != -1) {
        script->field_28 = data;
    }
}

// 0x44B838
static void op_play_sfx(Program* program)
{
    char* name = programStackPopString(program);

    gsound_play_sfx_file(name);
}

// 0x44B888
static void op_set_map_start(Program* program)
{
    int rotation = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    if (map_set_elevation(elevation) != 0) {
        int_debug("\nScript Error: %s: op_set_map_start: map_set_elevation failed", program->name);
        return;
    }

    int tile = 200 * y + x;
    if (tile_set_center(tile, TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS) != 0) {
        int_debug("\nScript Error: %s: op_set_map_start: tile_set_center failed", program->name);
        return;
    }

    map_set_entrance_hex(tile, elevation, rotation);
}

// 0x44B94C
static void op_override_map_start(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    int rotation = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int y = programStackPopInteger(program);
    int x = programStackPopInteger(program);

    char text[60];
    snprintf(text, sizeof(text), "OVERRIDE_MAP_START: x: %d, y: %d", x, y);
    debug_printf(text);

    int tile = 200 * y + x;
    int previousTile = tile_center_tile;
    if (tile != -1) {
        if (obj_set_rotation(obj_dude, rotation, NULL) != 0) {
            int_debug("\nError: %s: obj_set_rotation failed in override_map_start!", program->name);
        }

        if (obj_move_to_tile(obj_dude, tile, elevation, NULL) != 0) {
            int_debug("\nError: %s: obj_move_to_tile failed in override_map_start!", program->name);

            if (obj_move_to_tile(obj_dude, previousTile, elevation, NULL) != 0) {
                int_debug("\nError: %s: obj_move_to_tile RECOVERY Also failed!");
                exit(1);
            }
        }

        tile_set_center(tile, TILE_SET_CENTER_REFRESH_WINDOW);
        tile_refresh_display();
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x44BAA4
static void op_has_skill(Program* program)
{
    int skill = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int result = 0;
    if (object != NULL) {
        if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
            result = skill_level(object, skill);
        }
    } else {
        dbg_error(program, "has_skill", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, result);
}

// 0x44BB50
static void op_using_skill(Program* program)
{
    int skill = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    // NOTE: In the original source code this value is left uninitialized, that
    // explains why garbage is returned when using something else than dude and
    // SKILL_SNEAK as arguments.
    int result = 0;

    if (skill == SKILL_SNEAK && object == obj_dude) {
        result = is_pc_flag(PC_FLAG_SNEAKING);
    }

    programStackPushInteger(program, result);
}

// 0x44BBE4
static void op_roll_vs_skill(Program* program)
{
    int modifier = programStackPopInteger(program);
    int skill = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int roll = ROLL_CRITICAL_FAILURE;
    if (object != NULL) {
        if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
            int sid = scr_find_sid_from_program(program);

            Script* script;
            if (scr_ptr(sid, &script) != -1) {
                roll = skill_result(object, skill, modifier, &(script->howMuch));
            }
        }
    } else {
        dbg_error(program, "roll_vs_skill", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, roll);
}

// 0x44BCAC
static void op_skill_contest(Program* program)
{
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    dbg_error(program, "skill_contest", SCRIPT_ERROR_NOT_IMPLEMENTED);
    programStackPushInteger(program, 0);
}

// 0x44BD48
static void op_do_check(Program* program)
{
    int mod = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int roll = 0;
    if (object != NULL) {
        int sid = scr_find_sid_from_program(program);

        Script* script;
        if (scr_ptr(sid, &script) != -1) {
            switch (stat) {
            case STAT_STRENGTH:
            case STAT_PERCEPTION:
            case STAT_ENDURANCE:
            case STAT_CHARISMA:
            case STAT_INTELLIGENCE:
            case STAT_AGILITY:
            case STAT_LUCK:
                roll = stat_result(object, stat, mod, &(script->howMuch));
                break;
            default:
                int_debug("\nScript Error: %s: op_do_check: Stat out of range", program->name);
                break;
            }
        }
    } else {
        dbg_error(program, "do_check", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, roll);
}

// 0x44BE3C
static void op_is_success(Program* program)
{
    int data = programStackPopInteger(program);

    int result = -1;

    switch (data) {
    case ROLL_CRITICAL_FAILURE:
    case ROLL_FAILURE:
        result = 0;
        break;
    case ROLL_SUCCESS:
    case ROLL_CRITICAL_SUCCESS:
        result = 1;
        break;
    }

    programStackPushInteger(program, result);
}

// 0x44BEB4
static void op_is_critical(Program* program)
{
    int data = programStackPopInteger(program);

    int result = -1;

    switch (data) {
    case ROLL_CRITICAL_FAILURE:
    case ROLL_CRITICAL_SUCCESS:
        result = 1;
        break;
    case ROLL_FAILURE:
    case ROLL_SUCCESS:
        result = 0;
        break;
    }

    programStackPushInteger(program, result);
}

// 0x44BF1C
static void op_how_much(Program* program)
{
    int data = programStackPopInteger(program);

    int result = 0;

    int sid = scr_find_sid_from_program(program);

    Script* script;
    if (scr_ptr(sid, &script) != -1) {
        result = script->howMuch;
    } else {
        dbg_error(program, "how_much", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushInteger(program, result);
}

// 0x44BFA0
static void op_reaction_roll(Program* program)
{
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    programStackPushInteger(program, reaction_roll(data[2], data[1], data[0]));
}

// 0x44C024
static void op_reaction_influence(Program* program)
{
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    programStackPushInteger(program, reaction_influence(data[2], data[1], data[0]));
}

// 0x44C0A8
static void op_random(Program* program)
{
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    int result;
    if (vcr_status() == VCR_STATE_TURNED_OFF) {
        result = roll_random(data[1], data[0]);
    } else {
        result = (data[0] - data[1]) / 2;
    }

    programStackPushInteger(program, result);
}

// 0x44C13C
static void op_roll_dice(Program* program)
{
    int data[2];

    for (int arg = 0; arg < 2; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    dbg_error(program, "roll_dice", SCRIPT_ERROR_NOT_IMPLEMENTED);

    programStackPushInteger(program, 0);
}

// 0x44C1BC
static void op_move_to(Program* program)
{
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int newTile;

    if (object != NULL) {
        if (object == obj_dude) {
            bool tileLimitingEnabled = tile_get_scroll_limiting();
            bool tileBlockingEnabled = tile_get_scroll_blocking();

            if (tileLimitingEnabled) {
                tile_disable_scroll_limiting();
            }

            if (tileBlockingEnabled) {
                tile_disable_scroll_blocking();
            }

            Rect rect;
            newTile = obj_move_to_tile(object, tile, elevation, &rect);
            if (newTile != -1) {
                tile_set_center(object->tile, TILE_SET_CENTER_REFRESH_WINDOW);
            }

            if (tileLimitingEnabled) {
                tile_enable_scroll_limiting();
            }

            if (tileBlockingEnabled) {
                tile_enable_scroll_blocking();
            }
        } else {
            Rect before;
            obj_bound(object, &before);

            if (object->elevation != elevation && PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
                combat_delete_critter(object);
            }

            Rect after;
            newTile = obj_move_to_tile(object, tile, elevation, &after);
            if (newTile != -1) {
                rect_min_bound(&before, &after, &before);
                tile_refresh_rect(&before, map_elevation);
            }
        }
    } else {
        dbg_error(program, "move_to", SCRIPT_ERROR_OBJECT_IS_NULL);
        newTile = -1;
    }

    programStackPushInteger(program, newTile);
}

// 0x44C31C
static void op_create_object_sid(Program* program)
{
    int data[4];

    for (int arg = 0; arg < 4; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    int pid = data[3];
    int tile = data[2];
    int elevation = data[1];
    int sid = data[0];

    Object* object = NULL;

    if (isLoadingGame() != 0) {
        debug_printf("\nError: attempt to Create critter in load/save-game: %s!", program->name);
        goto out;
    }

    Proto* proto;
    if (proto_ptr(pid, &proto) != -1) {
        if (obj_new(&object, proto->fid, pid) != -1) {
            if (tile == -1) {
                tile = 0;
            }

            Rect rect;
            if (obj_move_to_tile(object, tile, elevation, &rect) != -1) {
                tile_refresh_rect(&rect, object->elevation);
            }
        }
    }

    if (sid != -1) {
        int scriptType = 0;
        switch (PID_TYPE(object->pid)) {
        case OBJ_TYPE_CRITTER:
            scriptType = SCRIPT_TYPE_CRITTER;
            break;
        case OBJ_TYPE_ITEM:
        case OBJ_TYPE_SCENERY:
            scriptType = SCRIPT_TYPE_ITEM;
            break;
        }

        if (object->sid != -1) {
            scr_remove(object->sid);
            object->sid = -1;
        }

        if (scr_new(&(object->sid), scriptType) == -1) {
            goto out;
        }

        Script* script;
        if (scr_ptr(object->sid, &script) == -1) {
            goto out;
        }

        script->scr_script_idx = sid - 1;

        object->id = new_obj_id();
        script->scr_oid = object->id;
        script->owner = object;
        scr_find_str_run_info(sid - 1, &(script->run_info_flags), object->sid);
    };

out:

    programStackPushPointer(program, object);
}

// 0x44C4FC
static void op_destroy_object(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        dbg_error(program, "destroy_object", SCRIPT_ERROR_OBJECT_IS_NULL);
        program->flags &= ~PROGRAM_FLAG_0x20;
        return;
    }

    if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
        if (isLoadingGame()) {
            debug_printf("\nError: attempt to destroy critter in load/save-game: %s!", program->name);
            program->flags &= ~PROGRAM_FLAG_0x20;
            return;
        }
    }

    bool isSelf = object == scr_find_obj_from_program(program);

    if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
        combat_delete_critter(object);
    }

    Object* owner = obj_top_environment(object);
    if (owner != NULL) {
        int quantity = item_count(owner, object);
        item_remove_mult(owner, object, quantity);

        if (owner == obj_dude) {
            intface_update_items(true);
        }

        obj_connect(object, 1, 0, NULL);

        if (isSelf) {
            object->sid = -1;
            object->flags |= (OBJECT_HIDDEN | OBJECT_NO_SAVE);
        } else {
            register_clear(object);
            obj_erase_object(object, NULL);
        }
    } else {
        register_clear(object);

        Rect rect;
        obj_erase_object(object, &rect);
        tile_refresh_rect(&rect, map_elevation);
    }

    program->flags &= ~PROGRAM_FLAG_0x20;

    if (isSelf) {
        program->flags |= PROGRAM_FLAG_0x0100;
    }
}

// 0x44C668
static void op_display_msg(Program* program)
{
    char* string = programStackPopString(program);
    display_print(string);

    bool showScriptMessages = false;
    configGetBool(&game_config, GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_SHOW_SCRIPT_MESSAGES_KEY, &showScriptMessages);

    if (showScriptMessages) {
        debug_printf("\n");
        debug_printf(string);
    }
}

// 0x44C6F8
static void op_script_overrides(Program* program)
{
    int sid = scr_find_sid_from_program(program);

    Script* script;
    if (scr_ptr(sid, &script) != -1) {
        script->scriptOverrides = 1;
    } else {
        dbg_error(program, "script_overrides", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }
}

// 0x44C738
static void op_obj_is_carrying_obj_pid(Program* program)
{
    int pid = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int result = 0;
    if (obj != NULL) {
        result = inven_pid_quantity_carried(obj, pid);
    } else {
        dbg_error(program, "obj_is_carrying_obj_pid", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, result);
}

// 0x44C7D0
static void op_tile_contains_obj_pid(Program* program)
{
    int pid = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);

    int result = 0;

    Object* object = obj_find_first_at(elevation);
    while (object) {
        if (object->tile == tile && object->pid == pid) {
            result = 1;
            break;
        }
        object = obj_find_next_at();
    }

    programStackPushInteger(program, result);
}

// 0x44C87C
static void op_self_obj(Program* program)
{
    Object* object = scr_find_obj_from_program(program);
    programStackPushPointer(program, object);
}

// 0x44C8A0
static void op_source_obj(Program* program)
{
    Object* object = NULL;

    int sid = scr_find_sid_from_program(program);

    Script* script;
    if (scr_ptr(sid, &script) != -1) {
        object = script->source;
    } else {
        dbg_error(program, "source_obj", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushPointer(program, object);
}

// 0x44C8F4
static void op_target_obj(Program* program)
{
    Object* object = NULL;

    int sid = scr_find_sid_from_program(program);

    Script* script;
    if (scr_ptr(sid, &script) != -1) {
        object = script->target;
    } else {
        dbg_error(program, "target_obj", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushPointer(program, object);
}

// 0x44C948
static void op_dude_obj(Program* program)
{
    programStackPushPointer(program, obj_dude);
}

// NOTE: The implementation is the same as in [op_target_obj].
//
// 0x44C968
static void op_obj_being_used_with(Program* program)
{
    Object* object = NULL;

    int sid = scr_find_sid_from_program(program);

    Script* script;
    if (scr_ptr(sid, &script) != -1) {
        object = script->target;
    } else {
        dbg_error(program, "obj_being_used_with", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushPointer(program, object);
}

// 0x44C9BC
static void op_local_var(Program* program)
{
    int data = programStackPopInteger(program);

    ProgramValue value;
    value.opcode = VALUE_TYPE_INT;
    value.integerValue = -1;

    int sid = scr_find_sid_from_program(program);
    scr_get_local_var(sid, data, value);

    programStackPushValue(program, value);
}

// 0x44CA28
static void op_set_local_var(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    int variable = programStackPopInteger(program);

    int sid = scr_find_sid_from_program(program);
    scr_set_local_var(sid, variable, value);
}

// 0x44CA9C
static void op_map_var(Program* program)
{
    int data = programStackPopInteger(program);

    ProgramValue value;
    if (map_get_global_var(data, value) == -1) {
        value.opcode = VALUE_TYPE_INT;
        value.integerValue = -1;
    }

    programStackPushValue(program, value);
}

// 0x44CAF0
static void op_set_map_var(Program* program)
{
    ProgramValue value = programStackPopValue(program);
    int variable = programStackPopInteger(program);

    map_set_global_var(variable, value);
}

// 0x44CB5C
static void op_global_var(Program* program)
{
    int data = programStackPopInteger(program);

    int value = -1;
    if (num_game_global_vars != 0) {
        value = game_get_global_var(data);
    } else {
        int_debug("\nScript Error: %s: op_global_var: no global vars found!", program->name);
    }

    programStackPushInteger(program, value);
}

// 0x44CBD8
static void op_set_global_var(Program* program)
{
    int value = programStackPopInteger(program);
    int variable = programStackPopInteger(program);

    if (num_game_global_vars != 0) {
        game_set_global_var(variable, value);
    } else {
        int_debug("\nScript Error: %s: op_set_global_var: no global vars found!", program->name);
    }
}

// 0x44CC5C
static void op_script_action(Program* program)
{
    int action = 0;

    int sid = scr_find_sid_from_program(program);

    Script* script;
    if (scr_ptr(sid, &script) != -1) {
        action = script->action;
    } else {
        dbg_error(program, "script_action", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushInteger(program, action);
}

// 0x44CCB0
static void op_obj_type(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int objectType = -1;
    if (object != NULL) {
        objectType = FID_TYPE(object->fid);
    }

    programStackPushInteger(program, objectType);
}

// 0x44CD14
static void op_obj_item_subtype(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int itemType = -1;
    if (obj != NULL) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_ITEM) {
            Proto* proto;
            if (proto_ptr(obj->pid, &proto) != -1) {
                itemType = item_get_type(obj);
            }
        }
    }

    programStackPushInteger(program, itemType);
}

// 0x44CD9C
static void op_get_critter_stat(Program* program)
{
    int stat = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int value = -1;
    if (object != NULL) {
        value = stat_level(object, stat);
    } else {
        dbg_error(program, "get_critter_stat", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, value);
}

// NOTE: Despite it's name it does not actually "set" stat, but "adjust". So
// it's last argument is amount of adjustment, not it's final value.
//
// 0x44CE3C
static void op_set_critter_stat(Program* program)
{
    int value = programStackPopInteger(program);
    int stat = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int result = 0;
    if (object != NULL) {
        if (object == obj_dude) {
            int currentValue = stat_get_base(object, stat);
            stat_set_base(object, stat, currentValue + value);
        } else {
            dbg_error(program, "set_critter_stat", SCRIPT_ERROR_FOLLOWS);
            debug_printf(" Can't modify anyone except obj_dude!");
            result = -1;
        }
    } else {
        dbg_error(program, "set_critter_stat", SCRIPT_ERROR_OBJECT_IS_NULL);
        result = -1;
    }

    programStackPushInteger(program, result);
}

// 0x44CF1C
static void op_animate_stand_obj(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    if (object == NULL) {
        int sid = scr_find_sid_from_program(program);

        Script* script;
        if (scr_ptr(sid, &script) == -1) {
            dbg_error(program, "animate_stand_obj", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
            return;
        }

        object = scr_find_obj_from_program(program);
    }

    if (!isInCombat()) {
        register_begin(ANIMATION_REQUEST_UNRESERVED);
        register_object_animate(object, ANIM_STAND, 0);
        register_end();
    }
}

// 0x44CFB4
static void op_animate_stand_reverse_obj(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    if (object == NULL) {
        int sid = scr_find_sid_from_program(program);

        Script* script;
        if (scr_ptr(sid, &script) == -1) {
            dbg_error(program, "animate_stand_reverse_obj", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
            return;
        }

        object = scr_find_obj_from_program(program);
    }

    if (!isInCombat()) {
        register_begin(ANIMATION_REQUEST_UNRESERVED);
        register_object_animate_reverse(object, ANIM_STAND, 0);
        register_end();
    }
}

// 0x44D04C
static void op_animate_move_obj_to_tile(Program* program)
{
    int flags = programStackPopInteger(program);
    ProgramValue tileValue = programStackPopValue(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        dbg_error(program, "animate_move_obj_to_tile", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    // CE: There is a bug in `sinthia` script. It's supposed that Sinthia moves
    // to `dest_tile`, but this function is passed `self_obj` as tile.
    int tile;
    if (tileValue.opcode == VALUE_TYPE_INT) {
        tile = tileValue.integerValue;
    } else {
        dbg_error(program, "animate_move_obj_to_tile", SCRIPT_ERROR_FOLLOWS);
        debug_printf("Invalid tile type.");
        tile = -1;
    }

    if (tile <= -1) {
        return;
    }

    int sid = scr_find_sid_from_program(program);

    Script* script;
    if (scr_ptr(sid, &script) == -1) {
        dbg_error(program, "animate_move_obj_to_tile", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
        return;
    }

    if (!critter_is_active(object)) {
        return;
    }

    if (isInCombat()) {
        return;
    }

    if ((flags & 0x10) != 0) {
        register_clear(object);
        flags &= ~0x10;
    }

    register_begin(ANIMATION_REQUEST_UNRESERVED);

    if (flags == 0) {
        register_object_move_to_tile(object, tile, object->elevation, -1, 0);
    } else {
        register_object_run_to_tile(object, tile, object->elevation, -1, 0);
    }

    register_end();
}

// 0x44D17
static void op_animate_jump(Program* program)
{
    int_debug("\nScript Error: %s: op_animate_jump: INVALID ACTION!");
}

// 0x44D18C
static void op_make_daytime(Program* program)
{
}

// 0x44D190
static void op_tile_distance(Program* program)
{
    int tile2 = programStackPopInteger(program);
    int tile1 = programStackPopInteger(program);

    int distance;

    if (tile1 != -1 && tile2 != -1) {
        distance = tile_dist(tile1, tile2);
    } else {
        distance = 9999;
    }

    programStackPushInteger(program, distance);
}

// 0x44D224
static void op_tile_distance_objs(Program* program)
{
    Object* object2 = static_cast<Object*>(programStackPopPointer(program));
    Object* object1 = static_cast<Object*>(programStackPopPointer(program));

    int distance = 9999;
    if (object1 != NULL && object2 != NULL) {
        if ((uintptr_t)object2 >= HEX_GRID_SIZE && (uintptr_t)object1 >= HEX_GRID_SIZE) {
            if (object1->elevation == object2->elevation) {
                if (object1->tile != -1 && object2->tile != -1) {
                    distance = tile_dist(object1->tile, object2->tile);
                }
            }
        } else {
            dbg_error(program, "tile_distance_objs", SCRIPT_ERROR_FOLLOWS);
            debug_printf(" Passed a tile # instead of an object!!!BADBADBAD!");
        }
    }

    programStackPushInteger(program, distance);
}

// 0x44D304
static void op_tile_num(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int tile = -1;
    if (obj != NULL) {
        tile = obj->tile;
    } else {
        dbg_error(program, "tile_num", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, tile);
}

// 0x44D374
static void op_tile_num_in_direction(Program* program)
{
    int distance = programStackPopInteger(program);
    int rotation = programStackPopInteger(program);
    int origin = programStackPopInteger(program);

    int tile = -1;

    if (origin != -1) {
        if (rotation < ROTATION_COUNT) {
            if (distance != 0) {
                tile = tile_num_in_direction(origin, rotation, distance);
                if (tile < -1) {
                    debug_printf("\nError: %s: op_tile_num_in_direction got #: %d", program->name, tile);
                    tile = -1;
                }
            }
        } else {
            dbg_error(program, "tile_num_in_direction", SCRIPT_ERROR_FOLLOWS);
            debug_printf(" rotation out of Range!");
        }
    } else {
        dbg_error(program, "tile_num_in_direction", SCRIPT_ERROR_FOLLOWS);
        debug_printf(" tileNum is -1!");
    }

    programStackPushInteger(program, tile);
}

// 0x44D448
static void op_pickup_obj(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        return;
    }

    int sid = scr_find_sid_from_program(program);

    Script* script;
    if (scr_ptr(sid, &script) == 1) {
        dbg_error(program, "pickup_obj", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
        return;
    }

    if (script->target == NULL) {
        dbg_error(program, "pickup_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    action_get_an_object(script->target, object);
}

// 0x44D4D8
static void op_drop_obj(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        return;
    }

    int sid = scr_find_sid_from_program(program);

    Script* script;
    if (scr_ptr(sid, &script) == -1) {
        // FIXME: Should be SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID.
        dbg_error(program, "drop_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (script->target == NULL) {
        // FIXME: Should be SCRIPT_ERROR_OBJECT_IS_NULL.
        dbg_error(program, "drop_obj", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
        return;
    }

    obj_drop(script->target, object);
}

// 0x44D568
static void op_add_obj_to_inven(Program* program)
{
    Object* item = static_cast<Object*>(programStackPopPointer(program));
    Object* owner = static_cast<Object*>(programStackPopPointer(program));

    if (owner == NULL || item == NULL) {
        return;
    }

    if (item->owner == NULL) {
        if (item_add_force(owner, item, 1) == 0) {
            Rect rect;
            obj_disconnect(item, &rect);
            tile_refresh_rect(&rect, item->elevation);
        }
    } else {
        dbg_error(program, "add_obj_to_inven", SCRIPT_ERROR_FOLLOWS);
        debug_printf(" Item was already attached to something else!");
    }
}

// 0x44D624
static void op_rm_obj_from_inven(Program* program)
{
    Object* item = static_cast<Object*>(programStackPopPointer(program));
    Object* owner = static_cast<Object*>(programStackPopPointer(program));

    if (owner == NULL || item == NULL) {
        return;
    }

    bool updateFlags = false;
    int flags = 0;

    if ((item->flags & OBJECT_EQUIPPED) != 0) {
        if ((item->flags & OBJECT_IN_LEFT_HAND) != 0) {
            flags |= OBJECT_IN_LEFT_HAND;
        }

        if ((item->flags & OBJECT_IN_RIGHT_HAND) != 0) {
            flags |= OBJECT_IN_RIGHT_HAND;
        }

        if ((item->flags & OBJECT_WORN) != 0) {
            flags |= OBJECT_WORN;
        }

        updateFlags = true;
    }

    if (item_remove_mult(owner, item, 1) == 0) {
        Rect rect;
        obj_connect(item, 1, 0, &rect);
        tile_refresh_rect(&rect, item->elevation);

        if (updateFlags) {
            correctFidForRemovedItem(owner, item, flags);
        }
    }
}

// 0x44D718
static void op_wield_obj_critter(Program* program)
{
    Object* item = static_cast<Object*>(programStackPopPointer(program));
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == NULL) {
        dbg_error(program, "wield_obj_critter", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (item == NULL) {
        dbg_error(program, "wield_obj_critter", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        dbg_error(program, "wield_obj_critter", SCRIPT_ERROR_FOLLOWS);
        debug_printf(" Only works for critters!  ERROR ERROR ERROR!");
        return;
    }

    int hand = HAND_RIGHT;

    bool shouldAdjustArmorClass = false;
    Object* oldArmor = NULL;
    Object* newArmor = NULL;
    if (critter == obj_dude) {
        if (intface_is_item_right_hand() == HAND_LEFT) {
            hand = HAND_LEFT;
        }

        if (item_get_type(item) == ITEM_TYPE_ARMOR) {
            oldArmor = inven_worn(obj_dude);
            shouldAdjustArmorClass = true;
            newArmor = item;
        }
    }

    inven_wield(critter, item, hand);

    if (critter == obj_dude) {
        if (shouldAdjustArmorClass) {
            adjust_ac(critter, oldArmor, newArmor);
        }
    }
}

// 0x44D870
static void op_use_obj(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        dbg_error(program, "use_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    int sid = scr_find_sid_from_program(program);

    Script* script;
    if (scr_ptr(sid, &script) == -1) {
        // FIXME: Should be SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID.
        dbg_error(program, "use_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (script->target == NULL) {
        dbg_error(program, "use_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    Object* self = scr_find_obj_from_program(program);
    if (PID_TYPE(self->pid) == OBJ_TYPE_CRITTER) {
        action_use_an_object(script->target, object);
    } else {
        obj_use(self, object);
    }
}

// 0x44D944
static void op_obj_can_see_obj(Program* program)
{
    Object* object2 = static_cast<Object*>(programStackPopPointer(program));
    Object* object1 = static_cast<Object*>(programStackPopPointer(program));

    int result = 0;

    if (object1 != NULL && object2 != NULL) {
        if (object2->tile != -1) {
            // NOTE: Looks like dead code, I guess these checks were incorporated
            // into higher level functions, but this code left intact.
            if (object2 == obj_dude) {
                is_pc_flag(0);
            }

            stat_level(object1, STAT_PERCEPTION);

            if (is_within_perception(object1, object2)) {
                Object* a5;
                make_straight_path(object1, object1->tile, object2->tile, NULL, &a5, 16);
                if (a5 == object2) {
                    result = 1;
                }
            }
        }
    } else {
        dbg_error(program, "obj_can_see_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, result);
}

// 0x44DA5
static int dbg_print_com_data(Object* attacker, Object* defender)
{
    debug_printf("\nScripts [Combat]: %s(Team %d) requests attack on %s(Team %d)",
        object_name(attacker),
        attacker->data.critter.combat.team,
        object_name(defender),
        defender->data.critter.combat.team);
    return 0;
}

// 0x44DA94
static void op_attack(Program* program)
{
    int data[8];

    for (int arg = 0; arg < 7; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    Object* target = static_cast<Object*>(programStackPopPointer(program));
    if (target == NULL) {
        dbg_error(program, "attack", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    program->flags |= PROGRAM_FLAG_0x20;

    Object* self = scr_find_obj_from_program(program);
    if (self == NULL) {
        program->flags &= ~PROGRAM_FLAG_0x20;
        return;
    }

    if (!critter_is_active(self)) {
        dbg_print_com_data(self, target);
        debug_printf("\n   But is already Inactive (Dead/Stunned)");
        program->flags &= ~PROGRAM_FLAG_0x20;
        return;
    }

    if (!critter_is_active(target)) {
        dbg_print_com_data(self, target);
        debug_printf("\n   But target is already dead");
        program->flags &= ~PROGRAM_FLAG_0x20;
        return;
    }

    if ((target->data.critter.combat.maneuver & CRITTER_MANUEVER_FLEEING) != 0) {
        dbg_print_com_data(self, target);
        debug_printf("\n   But target is AFRAID");
        program->flags &= ~PROGRAM_FLAG_0x20;
        return;
    }

    if (dialog_active()) {
        // TODO: Might be an error, program flag is not removed.
        return;
    }

    if (isInCombat()) {
        CritterCombatData* combatData = &(self->data.critter.combat);
        if ((combatData->maneuver & CRITTER_MANEUVER_ENGAGING) == 0) {
            combatData->maneuver |= CRITTER_MANEUVER_ENGAGING;
            combatData->whoHitMe = target;
        }
    } else {
        STRUCT_664980 attack;
        attack.attacker = self;
        attack.defender = target;
        attack.actionPointsBonus = 0;
        attack.accuracyBonus = data[4];
        attack.damageBonus = 0;
        attack.minDamage = data[3];
        attack.maxDamage = data[2];

        // TODO: Something is probably broken here, why it wants
        // flags to be the same? Maybe because both of them
        // are applied to defender because of the bug in 0x422F3C?
        if (data[1] == data[0]) {
            attack.field_1C = 1;
            attack.field_24 = data[0];
            attack.field_20 = data[1];
        } else {
            attack.field_1C = 0;
        }

        dbg_print_com_data(self, target);
        scripts_request_combat(&attack);
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x44DC84
static void op_start_gdialog(Program* program)
{
    int backgroundId = programStackPopInteger(program);
    int headId = programStackPopInteger(program);
    int reactionLevel = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    programStackPopInteger(program);

    if (isInCombat()) {
        return;
    }

    if (obj == NULL) {
        dbg_error(program, "start_gdialog", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    dialogue_head = -1;
    if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
        Proto* proto;
        if (proto_ptr(obj->pid, &proto) == -1) {
            return;
        }
    }

    if (headId != -1) {
        dialogue_head = art_id(OBJ_TYPE_HEAD, headId, 0, 0, 0);
    }

    gdialog_set_background(backgroundId);
    dialogue_mood = reactionLevel;

    if (dialogue_head != -1) {
        int npcReactionValue = reaction_get(dialog_target);
        int npcReactionType = reaction_to_level(npcReactionValue);
        switch (npcReactionType) {
        case NPC_REACTION_BAD:
            dialogue_mood = FIDGET_BAD;
            break;
        case NPC_REACTION_NEUTRAL:
            dialogue_mood = FIDGET_NEUTRAL;
            break;
        case NPC_REACTION_GOOD:
            dialogue_mood = FIDGET_GOOD;
            break;
        }
    }

    dialogue_scr_id = scr_find_sid_from_program(program);
    dialog_target = scr_find_obj_from_program(program);
    scr_dialogue_init(dialogue_head, dialogue_mood);
}

// 0x44DE08
static void op_end_dialogue(Program* program)
{
    if (scr_dialogue_exit() != -1) {
        dialog_target = NULL;
        dialogue_scr_id = -1;
    }
}

// 0x44DE2C
static void op_dialogue_reaction(Program* program)
{
    int value = programStackPopInteger(program);

    dialogue_mood = value;
    talk_to_critter_reacts(value);
}

// 0x44DE74
static void objs_area_turn_on_off(int a1, int a2, int a3, int a4, int enabled)
{
    // 0x44B560
    static Rect rect;

    int temp;
    Object* object;
    Rect object_bounds;

    if (a1 > a2) {
        temp = a1;
        a1 = a2;
        a2 = temp;
    }

    if (a3 > a4) {
        temp = a3;
        a3 = a4;
        a4 = a3;
    }

    while (a1 <= a2) {
        object = obj_find_first_at(a1);
        while (object != NULL) {
            if ((object->flags & OBJECT_HIDDEN) == enabled) {
                if (object->tile >= a3 && object->tile <= a4 && (object->tile - a3) / 200 <= a4 / 200 - a3 / 200) {
                    obj_bound(object, &object_bounds);
                    if (enabled) {
                        object->flags &= ~OBJECT_HIDDEN;
                    } else {
                        object->flags |= OBJECT_HIDDEN;
                    }
                    rect_min_bound(&rect, &object_bounds, &rect);
                }
            }
            object = obj_find_next_at();
        }
        tile_refresh_rect(&rect, a1);
    }
}

// 0x44DF7C
static void op_turn_off_objs_in_area(Program* program)
{
    int data[4];

    for (int arg = 0; arg < 4; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    objs_area_turn_on_off(data[3], data[2], data[1], data[0], 0);
}

// 0x44DFF0
static void op_turn_on_objs_in_area(Program* program)
{
    int data[4];

    for (int arg = 0; arg < 4; arg++) {
        data[arg] = programStackPopInteger(program);
    }

    objs_area_turn_on_off(data[3], data[2], data[1], data[0], 1);
}

// NOTE: Function name is a bit misleading. Last parameter is a boolean value
// where 1 or true makes object invisible, and value 0 (false) makes it visible
// again. So a better name for this function is opSetObjectInvisible.
//
//
// 0x44E064
static void op_set_obj_visibility(Program* program)
{
    int invisible = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (obj == NULL) {
        dbg_error(program, "set_obj_visibility", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (isLoadingGame()) {
        debug_printf("Error: attempt to set_obj_visibility in load/save-game: %s!", program->name);
        return;
    }

    if (invisible != 0) {
        if ((obj->flags & OBJECT_HIDDEN) == 0) {
            Rect rect;
            obj_bound(obj, &rect);

            obj->flags |= OBJECT_HIDDEN;
            if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
                obj->flags |= OBJECT_NO_BLOCK;
            }

            tile_refresh_rect(&rect, obj->elevation);
        }
    } else {
        if ((obj->flags & OBJECT_HIDDEN) != 0) {
            if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
                obj->flags &= ~OBJECT_NO_BLOCK;
            }

            obj->flags &= ~OBJECT_HIDDEN;

            Rect rect;
            obj_bound(obj, &rect);
            tile_refresh_rect(&rect, obj->elevation);
        }
    }
}

// 0x44E178
static void op_load_map(Program* program)
{
    int param = programStackPopInteger(program);
    ProgramValue mapIndexOrName = programStackPopValue(program);

    char* mapName = NULL;

    if ((mapIndexOrName.opcode & VALUE_TYPE_MASK) != VALUE_TYPE_INT) {
        if ((mapIndexOrName.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            mapName = interpretGetString(program, mapIndexOrName.opcode, mapIndexOrName.integerValue);
        } else {
            interpretError("script error: %s: invalid arg 1 to load_map", program->name);
        }
    }

    int mapIndex = -1;

    if (mapName != NULL) {
        game_global_vars[GVAR_LOAD_MAP_INDEX] = param;
        mapIndex = map_match_map_name(mapName);
    } else {
        if (mapIndexOrName.integerValue >= 0) {
            game_global_vars[GVAR_LOAD_MAP_INDEX] = param;
            mapIndex = mapIndexOrName.integerValue;
        }
    }

    if (mapIndex != -1) {
        MapTransition transition;
        transition.map = mapIndex;
        transition.elevation = -1;
        transition.tile = -1;
        transition.rotation = -1;
        map_leave_map(&transition);
    }
}

// 0x44E274
static void op_barter_offer(Program* program)
{
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        data[arg] = programStackPopInteger(program);
    }
}

// 0x44E2D4
static void op_barter_asking(Program* program)
{
    int data[3];

    for (int arg = 0; arg < 3; arg++) {
        data[arg] = programStackPopInteger(program);
    }
}

// 0x44E334
static void op_anim_busy(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int rc = 0;
    if (object != NULL) {
        rc = anim_busy(object);
    } else {
        dbg_error(program, "anim_busy", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, rc);
}

// 0x44E3A8
static void op_critter_heal(Program* program)
{
    int amount = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    int rc = critter_adjust_hits(critter, amount);

    if (critter == obj_dude) {
        intface_update_hit_points(true);
    }

    programStackPushInteger(program, rc);
}

// 0x44E440
static void op_set_light_level(Program* program)
{
    // Maps light level to light intensity.
    //
    // Middle value is mapped one-to-one which corresponds to 50% light level
    // (cavern lighting). Light levels above (51-100%) and below (0-49) is
    // calculated as percentage from two adjacent light values.
    //
    // 0x44B570
    static const int dword_453F90[3] = {
        0x4000,
        0xA000,
        0x10000,
    };

    int data = programStackPopInteger(program);

    int lightLevel = data;

    if (data == 50) {
        light_set_ambient(dword_453F90[1], true);
        return;
    }

    int lightIntensity;
    if (data > 50) {
        lightIntensity = dword_453F90[1] + data * (dword_453F90[2] - dword_453F90[1]) / 100;
    } else {
        lightIntensity = dword_453F90[0] + data * (dword_453F90[1] - dword_453F90[0]) / 100;
    }

    light_set_ambient(lightIntensity, true);
}

// 0x44E4E8
static void op_game_time(Program* program)
{
    int time = game_time();
    programStackPushInteger(program, time);
}

// 0x44E50C
static void op_game_time_in_seconds(Program* program)
{
    int time = game_time();
    programStackPushInteger(program, time / 10);
}

// 0x44E538
static void op_elevation(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int elevation = 0;
    if (object != NULL) {
        elevation = object->elevation;
    } else {
        dbg_error(program, "elevation", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, elevation);
}

// 0x44E5A4
static void op_kill_critter(Program* program)
{
    int deathFrame = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        dbg_error(program, "kill_critter", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (isLoadingGame()) {
        debug_printf("\nError: attempt to destroy critter in load/save-game: %s!", program->name);
    }

    program->flags |= PROGRAM_FLAG_0x20;

    Object* self = scr_find_obj_from_program(program);
    bool isSelf = self == object;

    register_clear(object);
    combat_delete_critter(object);
    critter_kill(object, deathFrame, 1);

    program->flags &= ~PROGRAM_FLAG_0x20;

    if (isSelf) {
        program->flags |= PROGRAM_FLAG_0x0100;
    }
}

// [forceBack] is to force fall back animation, otherwise it's fall front if it's present
//
// 0x0x44E690
int correctDeath(Object* critter, int anim, bool forceBack)
{
    if (anim >= ANIM_BIG_HOLE_SF && anim <= ANIM_FALL_FRONT_BLOOD_SF) {
        int violenceLevel = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
        config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &violenceLevel);

        bool useStandardDeath = false;
        if (violenceLevel < VIOLENCE_LEVEL_MAXIMUM_BLOOD) {
            useStandardDeath = true;
        } else {
            int fid = art_id(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, anim, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
            if (!art_exists(fid)) {
                useStandardDeath = true;
            }
        }

        if (useStandardDeath) {
            if (forceBack) {
                anim = ANIM_FALL_BACK;
            } else {
                int fid = art_id(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, ANIM_FALL_FRONT, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
                if (art_exists(fid)) {
                    anim = ANIM_FALL_FRONT;
                } else {
                    anim = ANIM_FALL_BACK;
                }
            }
        }
    }

    return anim;
}

// 0x44E750
static void op_kill_critter_type(Program* program)
{
    // 0x518ED0
    static int ftList[11] = {
        ANIM_FALL_BACK_BLOOD_SF,
        ANIM_BIG_HOLE_SF,
        ANIM_CHARRED_BODY_SF,
        ANIM_CHUNKS_OF_FLESH_SF,
        ANIM_FALL_FRONT_BLOOD_SF,
        ANIM_FALL_BACK_BLOOD_SF,
        ANIM_DANCING_AUTOFIRE_SF,
        ANIM_SLICED_IN_HALF_SF,
        ANIM_EXPLODED_TO_NOTHING_SF,
        ANIM_FALL_BACK_BLOOD_SF,
        ANIM_FALL_FRONT_BLOOD_SF,
    };

    int deathFrame = programStackPopInteger(program);
    int pid = programStackPopInteger(program);

    if (isLoadingGame()) {
        debug_printf("\nError: attempt to destroy critter in load/save-game: %s!", program->name);
        return;
    }

    program->flags |= PROGRAM_FLAG_0x20;

    Object* previousObj = NULL;
    int count = 0;
    int v3 = 0;

    Object* obj = obj_find_first();
    while (obj != NULL) {
        if (FID_ANIM_TYPE(obj->fid) >= ANIM_FALL_BACK_SF) {
            obj = obj_find_next();
            continue;
        }

        if ((obj->flags & OBJECT_HIDDEN) == 0 && obj->pid == pid && !critter_is_dead(obj)) {
            if (obj == previousObj || count > 200) {
                dbg_error(program, "kill_critter_type", SCRIPT_ERROR_FOLLOWS);
                debug_printf(" Infinite loop destroying critters!");
                program->flags &= ~PROGRAM_FLAG_0x20;
                return;
            }

            register_clear(obj);

            if (deathFrame != 0) {
                combat_delete_critter(obj);
                if (deathFrame == 1) {
                    int anim = correctDeath(obj, ftList[v3], 1);
                    critter_kill(obj, anim, 1);
                    v3 += 1;
                    if (v3 >= 11) {
                        v3 = 0;
                    }
                } else {
                    critter_kill(obj, ANIM_FALL_BACK_SF, 1);
                }
            } else {
                register_clear(obj);

                Rect rect;
                obj_erase_object(obj, &rect);
                tile_refresh_rect(&rect, map_elevation);
            }

            previousObj = obj;
            count += 1;

            obj_find_first();

            map_data.lastVisitTime = game_time();
        }

        obj = obj_find_next();
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x44E918
static void op_critter_damage(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    int damageTypeWithFlags = programStackPopInteger(program);
    int amount = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        dbg_error(program, "critter_damage", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (PID_TYPE(object->pid) != OBJ_TYPE_CRITTER) {
        dbg_error(program, "critter_damage", SCRIPT_ERROR_FOLLOWS);
        debug_printf(" Can't call on non-critters!");
        return;
    }

    Object* self = scr_find_obj_from_program(program);
    if (object->data.critter.combat.whoHitMeCid == -1) {
        object->data.critter.combat.whoHitMe = NULL;
    }

    bool animate = (damageTypeWithFlags & 0x200) == 0;
    bool bypassArmor = (damageTypeWithFlags & 0x100) != 0;
    int damageType = damageTypeWithFlags & ~(0x100 | 0x200);
    action_dmg(object->tile, object->elevation, amount, amount, damageType, animate, bypassArmor);

    program->flags &= ~PROGRAM_FLAG_0x20;

    if (self == object) {
        program->flags |= PROGRAM_FLAG_0x0100;
    }
}

// 0x44EA38
static void op_add_timer_event(Program* program)
{
    int param = programStackPopInteger(program);
    int delay = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        int_debug("\nScript Error: %s: op_add_timer_event: pobj is NULL!", program->name);
        return;
    }

    script_q_add(object->sid, delay, param);
}

// 0x44EAC0
static void op_rm_timer_event(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        // FIXME: Should be op_rm_timer_event.
        int_debug("\nScript Error: %s: op_add_timer_event: pobj is NULL!");
        return;
    }

    queue_remove(object);
}

// Converts seconds into game ticks.
//
// 0x44EB1C
static void op_game_ticks(Program* program)
{
    int ticks = programStackPopInteger(program);

    if (ticks < 0) {
        ticks = 0;
    }

    programStackPushInteger(program, ticks * 10);
}

// NOTE: The name of this function is misleading. It has (almost) nothing to do
// with player's "Traits" as a feature. Instead it's used to query many
// information of the critters using passed parameters. It's like "metarule" but
// for critters.
//
// 0x44EB78
static void op_has_trait(Program* program)
{
    int param = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    int type = programStackPopInteger(program);

    int result = 0;

    if (object != NULL) {
        switch (type) {
        case CRITTER_TRAIT_PERK:
            if (param < PERK_COUNT) {
                result = perk_level(param);
            } else {
                int_debug("\nScript Error: %s: op_has_trait: Perk out of range", program->name);
            }
            break;
        case CRITTER_TRAIT_OBJECT:
            switch (param) {
            case CRITTER_TRAIT_OBJECT_AI_PACKET:
                if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
                    result = object->data.critter.combat.aiPacket;
                }
                break;
            case CRITTER_TRAIT_OBJECT_TEAM:
                if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
                    result = object->data.critter.combat.team;
                }
                break;
            case CRITTER_TRAIT_OBJECT_ROTATION:
                result = object->rotation;
                break;
            case CRITTER_TRAIT_OBJECT_IS_INVISIBLE:
                result = (object->flags & OBJECT_HIDDEN) == 0;
                break;
            case CRITTER_TRAIT_OBJECT_GET_INVENTORY_WEIGHT:
                result = item_total_weight(object);
                break;
            }
            break;
        case CRITTER_TRAIT_TRAIT:
            if (param < TRAIT_COUNT) {
                result = trait_level(param);
            } else {
                int_debug("\nScript Error: %s: op_has_trait: Trait out of range", program->name);
            }
            break;
        default:
            int_debug("\nScript Error: %s: op_has_trait: Trait out of range", program->name);
            break;
        }
    } else {
        dbg_error(program, "has_trait", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, result);
}

// 0x44EC90
static void op_obj_can_hear_obj(Program* program)
{
    Object* object2 = static_cast<Object*>(programStackPopPointer(program));
    Object* object1 = static_cast<Object*>(programStackPopPointer(program));

    bool canHear = false;

    // FIXME: This is clearly an error. If any of the object is NULL
    // dereferencing will crash the game.
    if (object2 == NULL || object1 == NULL) {
        if (object2->elevation == object1->elevation) {
            if (object2->tile != -1 && object1->tile != -1) {
                if (is_within_perception(object1, object2)) {
                    canHear = true;
                }
            }
        }
    }

    programStackPushInteger(program, canHear);
}

// 0x44ED40
static void op_game_time_hour(Program* program)
{
    int value = game_time_hour();
    programStackPushInteger(program, value);
}

// 0x44ED64
static void op_fixed_param(Program* program)
{
    int fixedParam = 0;

    int sid = scr_find_sid_from_program(program);

    Script* script;
    if (scr_ptr(sid, &script) != -1) {
        fixedParam = script->fixedParam;
    } else {
        dbg_error(program, "fixed_param", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushInteger(program, fixedParam);
}

// 0x44EDB8
static void op_tile_is_visible(Program* program)
{
    int data = programStackPopInteger(program);

    int isVisible = 0;
    if (scripts_tile_is_visible(data)) {
        isVisible = 1;
    }

    programStackPushInteger(program, isVisible);
}

// 0x44EE18
static void op_dialogue_system_enter(Program* program)
{
    int sid = scr_find_sid_from_program(program);

    Script* script;
    if (scr_ptr(sid, &script) == -1) {
        return;
    }

    Object* self = scr_find_obj_from_program(program);
    if (PID_TYPE(self->pid) == OBJ_TYPE_CRITTER) {
        if (!critter_is_active(self)) {
            return;
        }
    }

    if (isInCombat()) {
        return;
    }

    if (game_state_request(GAME_STATE_4) == -1) {
        return;
    }

    dialog_target = scr_find_obj_from_program(program);
}

// 0x44EE78
static void op_action_being_used(Program* program)
{
    int action = -1;

    int sid = scr_find_sid_from_program(program);

    Script* script;
    if (scr_ptr(sid, &script) != -1) {
        action = script->actionBeingUsed;
    } else {
        dbg_error(program, "action_being_used", SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID);
    }

    programStackPushInteger(program, action);
}

// 0x44EECC
static void op_critter_state(Program* program)
{
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    int state = CRITTER_STATE_DEAD;
    if (critter != NULL && PID_TYPE(critter->pid) == OBJ_TYPE_CRITTER) {
        if (critter_is_active(critter)) {
            state = CRITTER_STATE_NORMAL;

            int anim = FID_ANIM_TYPE(critter->fid);
            if (anim >= ANIM_FALL_BACK_SF && anim <= ANIM_FALL_FRONT_SF) {
                state = CRITTER_STATE_PRONE;
            }

            state |= (critter->data.critter.combat.results & DAM_CRIP);
        }
    } else {
        dbg_error(program, "critter_state", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, state);
}

// 0x44EF6C
static void op_game_time_advance(Program* program)
{
    int data = programStackPopInteger(program);

    int days = data / GAME_TIME_TICKS_PER_DAY;
    int remainder = data % GAME_TIME_TICKS_PER_DAY;

    for (int day = 0; day < days; day++) {
        inc_game_time(GAME_TIME_TICKS_PER_DAY);
        queue_process();
    }

    inc_game_time(remainder);
    queue_process();
}

// 0x44EFE8
static void op_radiation_inc(Program* program)
{
    int amount = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        dbg_error(program, "radiation_inc", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    critter_adjust_rads(object, amount);
}

// 0x44F06C
static void op_radiation_dec(Program* program)
{
    int amount = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        dbg_error(program, "radiation_dec", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    int radiation = critter_get_rads(object);
    int adjustment = radiation >= 0 ? -amount : 0;

    critter_adjust_rads(object, adjustment);
}

// 0x44F104
static void op_critter_attempt_placement(Program* program)
{
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == NULL) {
        dbg_error(program, "critter_attempt_placement", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (elevation != critter->elevation && PID_TYPE(critter->pid) == OBJ_TYPE_CRITTER) {
        combat_delete_critter(critter);
    }

    obj_move_to_tile(critter, 0, elevation, NULL);

    int rc = obj_attempt_placement(critter, tile, elevation, 1);
    programStackPushInteger(program, rc);
}

// 0x44F1D4
static void op_obj_pid(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int pid = -1;
    if (obj) {
        pid = obj->pid;
    } else {
        dbg_error(program, "obj_pid", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, pid);
}

// 0x44F244
static void op_cur_map_index(Program* program)
{
    int mapIndex = map_get_index_number();
    programStackPushInteger(program, mapIndex);
}

// 0x44F268
static void op_critter_add_trait(Program* program)
{
    int value = programStackPopInteger(program);
    int param = programStackPopInteger(program);
    int kind = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != NULL) {
        if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
            switch (kind) {
            case CRITTER_TRAIT_PERK:
                if (perk_add(param) != 0) {
                    int_debug("\nScript Error: %s: op_critter_add_trait: perk_add failed");
                }
                break;
            case CRITTER_TRAIT_OBJECT:
                switch (param) {
                case CRITTER_TRAIT_OBJECT_AI_PACKET:
                    object->data.critter.combat.aiPacket = value;
                    break;
                case CRITTER_TRAIT_OBJECT_TEAM:
                    if (object->data.critter.combat.team == value) {
                        break;
                    }

                    if (isLoadingGame()) {
                        break;
                    }

                    combatai_switch_team(object, value);
                    break;
                }
                break;
            default:
                int_debug("\nScript Error: %s: op_critter_add_trait: Trait out of range", program->name);
                break;
            }
        }
    } else {
        dbg_error(program, "critter_add_trait", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, -1);
}

// 0x44F390
static void op_critter_rm_trait(Program* program)
{
    int value = programStackPopInteger(program);
    int param = programStackPopInteger(program);
    int kind = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        dbg_error(program, "critter_rm_trait", SCRIPT_ERROR_OBJECT_IS_NULL);
        // FIXME: Ruins stack.
        return;
    }

    if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
        switch (kind) {
        case CRITTER_TRAIT_PERK:
            // while (perk_level(object, param) > 0) {
            //     if (perk_sub(object, param) != 0) {
            //         int_debug("\nScript Error: op_critter_rm_trait: perk_sub failed");
            //     }
            // }
            break;
        default:
            int_debug("\nScript Error: %s: op_critter_rm_trait: Trait out of range", program->name);
            break;
        }
    }

    programStackPushInteger(program, -1);
}

// 0x44F458
static void op_proto_data(Program* program)
{
    int member = programStackPopInteger(program);
    int pid = programStackPopInteger(program);

    ProtoDataMemberValue value;
    value.integerValue = 0;
    int valueType = proto_data_member(pid, member, &value);
    switch (valueType) {
    case PROTO_DATA_MEMBER_TYPE_INT:
        programStackPushInteger(program, value.integerValue);
        break;
    case PROTO_DATA_MEMBER_TYPE_STRING:
        programStackPushString(program, value.stringValue);
        break;
    default:
        programStackPushInteger(program, 0);
        break;
    }
}

// 0x44F510
static void op_message_str(Program* program)
{
    // 0x5054FC
    static char errStr[] = "Error";

    int messageIndex = programStackPopInteger(program);
    int messageListIndex = programStackPopInteger(program);

    char* string;
    if (messageIndex >= 1) {
        string = scr_get_msg_str_speech(messageListIndex, messageIndex, 1);
        if (string == NULL) {
            debug_printf("\nError: No message file EXISTS!: index %d, line %d", messageListIndex, messageIndex);
            string = errStr;
        }
    } else {
        string = errStr;
    }

    programStackPushString(program, string);
}

// 0x44F5D0
static void op_critter_inven_obj(Program* program)
{
    int type = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (PID_TYPE(critter->pid) == OBJ_TYPE_CRITTER) {
        switch (type) {
        case INVEN_TYPE_WORN:
            programStackPushPointer(program, inven_worn(critter));
            break;
        case INVEN_TYPE_RIGHT_HAND:
            if (critter == obj_dude) {
                if (intface_is_item_right_hand() != HAND_LEFT) {
                    programStackPushPointer(program, inven_right_hand(critter));
                } else {
                    programStackPushPointer(program, NULL);
                }
            } else {
                programStackPushPointer(program, inven_right_hand(critter));
            }
            break;
        case INVEN_TYPE_LEFT_HAND:
            if (critter == obj_dude) {
                if (intface_is_item_right_hand() == HAND_LEFT) {
                    programStackPushPointer(program, inven_left_hand(critter));
                } else {
                    programStackPushPointer(program, NULL);
                }
            } else {
                programStackPushPointer(program, inven_left_hand(critter));
            }
            break;
        case INVEN_TYPE_INV_COUNT:
            programStackPushInteger(program, critter->data.inventory.length);
            break;
        default:
            int_debug("script error: %s: Error in critter_inven_obj -- wrong type!", program->name);
            programStackPushInteger(program, 0);
            break;
        }
    } else {
        dbg_error(program, "critter_inven_obj", SCRIPT_ERROR_FOLLOWS);
        debug_printf("  Not a critter!");
        programStackPushInteger(program, 0);
    }
}

// 0x44F6D0
static void op_obj_set_light_level(Program* program)
{
    int lightDistance = programStackPopInteger(program);
    int lightIntensity = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL) {
        dbg_error(program, "obj_set_light_level", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    Rect rect;
    if (lightIntensity != 0) {
        if (obj_set_light(object, lightDistance, (lightIntensity * 65636) / 100, &rect) == -1) {
            return;
        }
    } else {
        if (obj_set_light(object, lightDistance, 0, &rect) == -1) {
            return;
        }
    }
    tile_refresh_rect(&rect, object->elevation);
}

// 0x44F798
static void op_world_map(Program* program)
{
    scripts_request_worldmap();
}

// 0x44F7A0
static void op_town_map(Program* program)
{
    scripts_request_townmap();
}

// 0x44F7E4
static void op_float_msg(Program* program)
{
    // 0x505500
    static int last_color = 1;

    int floatingMessageType = programStackPopInteger(program);
    ProgramValue stringValue = programStackPopValue(program);
    char* string = NULL;
    if ((stringValue.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        string = interpretGetString(program, stringValue.opcode, stringValue.integerValue);
    }
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int color = colorTable[32747];
    int a5 = colorTable[0];
    int font = 101;

    if (obj == NULL) {
        dbg_error(program, "float_msg", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (string == NULL || *string == '\0') {
        int_debug("\nScript Error: %s: op_float_msg: empty or blank string!");
        return;
    }

    if (obj->elevation != map_elevation) {
        return;
    }

    if (floatingMessageType == FLOATING_MESSAGE_TYPE_COLOR_SEQUENCE) {
        floatingMessageType = last_color + 1;
        if (floatingMessageType >= FLOATING_MESSAGE_TYPE_COUNT) {
            floatingMessageType = FLOATING_MESSAGE_TYPE_BLACK;
        }
        last_color = floatingMessageType;
    }

    switch (floatingMessageType) {
    case FLOATING_MESSAGE_TYPE_WARNING:
        color = colorTable[31744];
        a5 = colorTable[0];
        font = 103;
        tile_set_center(obj_dude->tile, TILE_SET_CENTER_REFRESH_WINDOW);
        break;
    case FLOATING_MESSAGE_TYPE_NORMAL:
    case FLOATING_MESSAGE_TYPE_YELLOW:
        color = colorTable[32747];
        break;
    case FLOATING_MESSAGE_TYPE_BLACK:
    case FLOATING_MESSAGE_TYPE_PURPLE:
    case FLOATING_MESSAGE_TYPE_GREY:
        color = colorTable[10570];
        break;
    case FLOATING_MESSAGE_TYPE_RED:
        color = colorTable[31744];
        break;
    case FLOATING_MESSAGE_TYPE_GREEN:
        color = colorTable[992];
        break;
    case FLOATING_MESSAGE_TYPE_BLUE:
        color = colorTable[31];
        break;
    case FLOATING_MESSAGE_TYPE_NEAR_WHITE:
        color = colorTable[21140];
        break;
    case FLOATING_MESSAGE_TYPE_LIGHT_RED:
        color = colorTable[32074];
        break;
    case FLOATING_MESSAGE_TYPE_WHITE:
        color = colorTable[32767];
        break;
    case FLOATING_MESSAGE_TYPE_DARK_GREY:
        color = colorTable[8456];
        break;
    case FLOATING_MESSAGE_TYPE_LIGHT_GREY:
        color = colorTable[15855];
        break;
    }

    Rect rect;
    if (text_object_create(obj, string, font, color, a5, &rect) != -1) {
        tile_refresh_rect(&rect, obj->elevation);
    }
}

// 0x44FA00
static void op_metarule(Program* program)
{
    ProgramValue param = programStackPopValue(program);
    int rule = programStackPopInteger(program);

    switch (rule) {
    case METARULE_SIGNAL_END_GAME:
        game_user_wants_to_quit = 2;
        programStackPushInteger(program, 0);
        break;
    case METARULE_FIRST_RUN:
        programStackPushInteger(program, (map_data.flags & MAP_SAVED) == 0);
        break;
    case METARULE_ELEVATOR:
        scripts_request_elevator(param.integerValue);
        programStackPushInteger(program, 0);
        break;
    case METARULE_PARTY_COUNT:
        programStackPushInteger(program, getPartyMemberCount());
        break;
    case METARULE_IS_LOADGAME:
        programStackPushInteger(program, isLoadingGame());
        break;
    default:
        programStackPushInteger(program, 0);
        break;
    }
}

// 0x44FAD0
static void op_anim(Program* program)
{
    int frame = programStackPopInteger(program);
    int anim = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (obj == NULL) {
        dbg_error(program, "anim", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (anim < ANIM_COUNT) {
        CritterCombatData* combatData = NULL;
        if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
            combatData = &(obj->data.critter.combat);
        }

        anim = correctDeath(obj, anim, true);

        register_begin(ANIMATION_REQUEST_UNRESERVED);

        // TODO: Not sure about the purpose, why it handles knock down flag?
        if (frame == 0) {
            register_object_animate(obj, anim, 0);
            if (anim >= ANIM_FALL_BACK && anim <= ANIM_FALL_FRONT_BLOOD) {
                int fid = art_id(OBJ_TYPE_CRITTER, obj->fid & 0xFFF, anim + 28, (obj->fid & 0xF000) >> 12, (obj->fid & 0x70000000) >> 28);
                register_object_change_fid(obj, fid, -1);
            }

            if (combatData != NULL) {
                combatData->results &= DAM_KNOCKED_DOWN;
            }
        } else {
            int fid = art_id(FID_TYPE(obj->fid), obj->fid & 0xFFF, anim, (obj->fid & 0xF000) >> 12, (obj->fid & 0x70000000) >> 24);
            register_object_animate_reverse(obj, anim, 0);

            if (anim == ANIM_PRONE_TO_STANDING) {
                fid = art_id(FID_TYPE(obj->fid), obj->fid & 0xFFF, ANIM_FALL_FRONT_SF, (obj->fid & 0xF000) >> 12, (obj->fid & 0x70000000) >> 24);
            } else if (anim == ANIM_BACK_TO_STANDING) {
                fid = art_id(FID_TYPE(obj->fid), obj->fid & 0xFFF, ANIM_FALL_BACK_SF, (obj->fid & 0xF000) >> 12, (obj->fid & 0x70000000) >> 24);
            }

            if (combatData != NULL) {
                combatData->results |= DAM_KNOCKED_DOWN;
            }

            register_object_change_fid(obj, fid, -1);
        }

        register_end();
    } else if (anim == 1000) {
        if (frame < ROTATION_COUNT) {
            Rect rect;
            obj_set_rotation(obj, frame, &rect);
            tile_refresh_rect(&rect, map_elevation);
        }
    } else if (anim == 1010) {
        Rect rect;
        obj_set_frame(obj, frame, &rect);
        tile_refresh_rect(&rect, map_elevation);
    } else {
        int_debug("\nScript Error: %s: op_anim: anim out of range", program->name);
    }
}

// 0x44FD00
static void op_obj_carrying_pid_obj(Program* program)
{
    int pid = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    Object* result = NULL;
    if (object != NULL) {
        result = inven_pid_is_carried_ptr(object, pid);
    } else {
        dbg_error(program, "obj_carrying_pid_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushPointer(program, result);
}

// 0x44FD9C
static void op_reg_anim_func(Program* program)
{
    ProgramValue param = programStackPopValue(program);
    int cmd = programStackPopInteger(program);

    if (!isInCombat()) {
        switch (cmd) {
        case OP_REG_ANIM_FUNC_BEGIN:
            register_begin(param.integerValue);
            break;
        case OP_REG_ANIM_FUNC_CLEAR:
            register_clear(static_cast<Object*>(param.pointerValue));
            break;
        case OP_REG_ANIM_FUNC_END:
            register_end();
            break;
        }
    }
}

// 0x44FE34
static void op_reg_anim_animate(Program* program)
{
    int delay = programStackPopInteger(program);
    int anim = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (!isInCombat()) {
        int violenceLevel = VIOLENCE_LEVEL_NONE;
        if (anim != 20 || object == NULL || object->pid != 0x100002F || (config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_VIOLENCE_LEVEL_KEY, &violenceLevel) && violenceLevel >= 2)) {
            if (object != NULL) {
                register_object_animate(object, anim, delay);
            } else {
                dbg_error(program, "reg_anim_animate", SCRIPT_ERROR_OBJECT_IS_NULL);
            }
        }
    }
}

// 0x44FF04
static void op_reg_anim_animate_reverse(Program* program)
{
    int delay = programStackPopInteger(program);
    int anim = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (!isInCombat()) {
        if (object != NULL) {
            register_object_animate_reverse(object, anim, delay);
        } else {
            dbg_error(program, "reg_anim_animate_reverse", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// 0x44FF98
static void op_reg_anim_obj_move_to_obj(Program* program)
{
    int delay = programStackPopInteger(program);
    Object* dest = static_cast<Object*>(programStackPopPointer(program));
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (!isInCombat()) {
        if (object != NULL) {
            register_object_move_to_object(object, dest, -1, delay);
        } else {
            dbg_error(program, "reg_anim_obj_move_to_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// 0x450030
static void op_reg_anim_obj_run_to_obj(Program* program)
{
    int delay = programStackPopInteger(program);
    Object* dest = static_cast<Object*>(programStackPopPointer(program));
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (!isInCombat()) {
        if (object != NULL) {
            register_object_run_to_object(object, dest, -1, delay);
        } else {
            dbg_error(program, "reg_anim_obj_run_to_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// 0x4500C8
static void op_reg_anim_obj_move_to_tile(Program* program)
{
    int delay = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (!isInCombat()) {
        if (object != NULL) {
            register_object_move_to_tile(object, tile, object->elevation, -1, delay);
        } else {
            dbg_error(program, "reg_anim_obj_move_to_tile", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// 0x450164
static void op_reg_anim_obj_run_to_tile(Program* program)
{
    int delay = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (!isInCombat()) {
        if (object != NULL) {
            register_object_run_to_tile(object, tile, object->elevation, -1, delay);
        } else {
            dbg_error(program, "reg_anim_obj_run_to_tile", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// 0x450200
static void op_play_gmovie(Program* program)
{
    // 0x44B57C
    static const unsigned short game_movie_flags[MOVIE_COUNT] = {
        /*   IPLOGO */ GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        /*   MPLOGO */ GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        /*    INTRO */ GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        /*   VEXPLD */ GAME_MOVIE_FADE_IN | GAME_MOVIE_PAUSE_MUSIC,
        /*  CATHEXP */ GAME_MOVIE_FADE_IN | GAME_MOVIE_PAUSE_MUSIC,
        /* OVRINTRO */ GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        /*    BOIL3 */ GAME_MOVIE_FADE_IN | GAME_MOVIE_PAUSE_MUSIC,
        /*   OVRRUN */ GAME_MOVIE_FADE_IN | GAME_MOVIE_PAUSE_MUSIC,
        /*    WALKM */ GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        /*    WALKW */ GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        /*   DIPEDV */ GAME_MOVIE_FADE_IN | GAME_MOVIE_PAUSE_MUSIC,
        /*    BOIL1 */ GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        /*    BOIL2 */ GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC,
        /* RAEKILLS */ GAME_MOVIE_FADE_IN | GAME_MOVIE_PAUSE_MUSIC,
    };

    program->flags |= PROGRAM_FLAG_0x20;

    int movie = programStackPopInteger(program);

    // CE: Disable map updates. Needed to stop animation of objects (dude in
    // particular) when playing movies (the problem can be seen as visual
    // artifacts when playing endgame oilrig explosion).
    bool isoWasDisabled = map_disable_bk_processes();

    gDialogDisableBK();

    unsigned short flags = game_movie_flags[movie];
    if (movie == MOVIE_VEXPLD || movie == MOVIE_CATHEXP) {
        if (map_data.name[0] == '\0') {
            flags |= GAME_MOVIE_FADE_OUT;
        }
    }

    if (gmovie_play(movie, flags) == -1) {
        debug_printf("\nError playing movie %d!", movie);
    }

    gDialogEnableBK();

    if (isoWasDisabled) {
        map_enable_bk_processes();
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x4502AC
static void op_add_mult_objs_to_inven(Program* program)
{
    int quantity = programStackPopInteger(program);
    Object* item = static_cast<Object*>(programStackPopPointer(program));
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object == NULL || item == NULL) {
        return;
    }

    if (item_add_force(object, item, quantity) == 0) {
        Rect rect;
        obj_disconnect(item, &rect);
        tile_refresh_rect(&rect, item->elevation);
    }
}

// 0x450340
static void op_rm_mult_objs_from_inven(Program* program)
{
    int quantityToRemove = programStackPopInteger(program);
    Object* item = static_cast<Object*>(programStackPopPointer(program));
    Object* owner = static_cast<Object*>(programStackPopPointer(program));

    if (owner == NULL || item == NULL) {
        // FIXME: Ruined stack.
        return;
    }

    bool itemWasEquipped = (item->flags & OBJECT_EQUIPPED) != 0;

    int quantity = item_count(owner, item);
    if (quantity > quantityToRemove) {
        quantity = quantityToRemove;
    }

    if (quantity != 0) {
        if (item_remove_mult(owner, item, quantity) == 0) {
            Rect updatedRect;
            obj_connect(item, 1, 0, &updatedRect);
            if (itemWasEquipped) {
                if (owner == obj_dude) {
                    intface_update_items(true);
                    intface_update_ac(false);
                }
            }
        }
    }

    programStackPushInteger(program, quantity);
}

// 0x45044C
static void op_get_month(Program* program)
{
    int month;
    game_time_date(&month, NULL, NULL);

    programStackPushInteger(program, month);
}

// 0x45047C
static void op_get_day(Program* program)
{
    int day;
    game_time_date(NULL, &day, NULL);

    programStackPushInteger(program, day);
}

// 0x4504AC
static void op_explosion(Program* program)
{
    int maxDamage = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);

    if (tile == -1) {
        debug_printf("\nError: explosion: bad tile_num!");
        return;
    }

    int minDamage = 1;
    if (maxDamage == 0) {
        minDamage = 0;
    }

    scripts_request_explosion(tile, elevation, minDamage, maxDamage);
}

// 0x450540
static void op_days_since_visited(Program* program)
{
    int days;

    if (map_data.lastVisitTime != 0) {
        days = (game_time() - map_data.lastVisitTime) / GAME_TIME_TICKS_PER_DAY;
    } else {
        days = -1;
    }

    programStackPushInteger(program, days);
}

// 0x450584
static void op_gsay_start(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    if (gDialogStart() != 0) {
        program->flags &= ~PROGRAM_FLAG_0x20;
        interpretError("Error starting dialog.");
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x4505C8
static void op_gsay_end(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;
    gDialogGo();
    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x4505EC
static void op_gsay_reply(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    ProgramValue msg = programStackPopValue(program);
    int messageListId = programStackPopInteger(program);

    if ((msg.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        char* string = interpretGetString(program, msg.opcode, msg.integerValue);
        gDialogReplyStr(program, messageListId, string);
    } else if (msg.opcode == VALUE_TYPE_INT) {
        gDialogReply(program, messageListId, msg.integerValue);
    } else {
        interpretError("script error: %s: invalid arg %d to gsay_reply", program->name, 0);
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x4506B0
static void op_gsay_option(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    int reaction = programStackPopInteger(program);
    ProgramValue proc = programStackPopValue(program);
    ProgramValue msg = programStackPopValue(program);
    int messageListId = programStackPopInteger(program);

    if ((proc.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        char* procName = interpretGetString(program, proc.opcode, proc.integerValue);
        if ((msg.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            const char* string = interpretGetString(program, msg.opcode, msg.integerValue);
            gDialogOptionStr(messageListId, string, procName, reaction);
        } else if (msg.opcode == VALUE_TYPE_INT) {
            gDialogOption(messageListId, msg.integerValue, procName, reaction);
        } else {
            interpretError("script error: %s: invalid arg %d to gsay_option", program->name, 1);
        }
    } else if ((proc.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_INT) {
        if ((msg.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            const char* string = interpretGetString(program, msg.opcode, msg.integerValue);
            gDialogOptionProcStr(messageListId, string, proc.integerValue, reaction);
        } else if (msg.opcode == VALUE_TYPE_INT) {
            gDialogOptionProc(messageListId, msg.integerValue, proc.integerValue, reaction);
        } else {
            interpretError("script error: %s: invalid arg %d to gsay_option", program->name, 1);
        }
    } else {
        interpretError("Invalid arg 3 to sayOption");
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x450844
static void op_gsay_message(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    int reaction = programStackPopInteger(program);
    ProgramValue msg = programStackPopValue(program);
    int messageListId = programStackPopInteger(program);

    if ((msg.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        char* string = interpretGetString(program, msg.opcode, msg.integerValue);
        gDialogReplyStr(program, messageListId, string);
    } else if (msg.opcode == VALUE_TYPE_INT) {
        gDialogReply(program, messageListId, msg.integerValue);
    } else {
        interpretError("script error: %s: invalid arg %d to gsay_message", program->name, 1);
    }

    gDialogOption(-2, -2, NULL, 50);
    gDialogSayMessage();

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x45092C
static void op_giq_option(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    int reaction = programStackPopInteger(program);
    ProgramValue proc = programStackPopValue(program);
    ProgramValue msg = programStackPopValue(program);
    int messageListId = programStackPopInteger(program);
    int iq = programStackPopInteger(program);

    int intelligence = stat_level(obj_dude, STAT_INTELLIGENCE);
    intelligence += perk_level(PERK_SMOOTH_TALKER);

    if (iq < 0) {
        if (-intelligence < iq) {
            program->flags &= ~PROGRAM_FLAG_0x20;
            return;
        }
    } else {
        if (intelligence < iq) {
            program->flags &= ~PROGRAM_FLAG_0x20;
            return;
        }
    }

    if ((proc.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
        char* procName = interpretGetString(program, proc.opcode, proc.integerValue);
        if ((msg.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            char* string = interpretGetString(program, msg.opcode, msg.integerValue);
            gDialogOptionStr(messageListId, string, procName, reaction);
        } else if (msg.opcode == VALUE_TYPE_INT) {
            gDialogOption(messageListId, msg.integerValue, procName, reaction);
        } else {
            interpretError("script error: %s: invalid arg %d to giq_option", program->name, 1);
        }
    } else if (proc.opcode == VALUE_TYPE_INT) {
        if ((msg.opcode & VALUE_TYPE_MASK) == VALUE_TYPE_STRING) {
            char* string = interpretGetString(program, msg.opcode, msg.integerValue);
            gDialogOptionProcStr(messageListId, string, proc.integerValue, reaction);
        } else if (msg.opcode == VALUE_TYPE_INT) {
            gDialogOptionProc(messageListId, msg.integerValue, proc.integerValue, reaction);
        } else {
            interpretError("script error: %s: invalid arg %d to giq_option", program->name, 1);
        }
    } else {
        interpretError("script error: %s: invalid arg %d to giq_option", program->name, 3);
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x450AE4
static void op_poison(Program* program)
{
    int amount = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (obj == NULL) {
        dbg_error(program, "poison", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (critter_adjust_poison(obj, amount) != 0) {
        debug_printf("\nScript Error: poison: adjust failed!");
    }
}

// 0x450B7C
static void op_get_poison(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    int poison = 0;
    if (obj != NULL) {
        if (PID_TYPE(obj->pid) == OBJ_TYPE_CRITTER) {
            poison = critter_get_poison(obj);
        } else {
            debug_printf("\nScript Error: get_poison: who is not a critter!");
        }
    } else {
        dbg_error(program, "get_poison", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, poison);
}

// 0x450C04
static void op_party_add(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    if (object == NULL) {
        dbg_error(program, "party_add", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    partyMemberAdd(object);
}

// 0x450C78
static void op_party_remove(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    if (object == NULL) {
        dbg_error(program, "party_remove", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    partyMemberRemove(object);
}

// 0x450CEC
static void op_reg_anim_animate_forever(Program* program)
{
    int anim = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (!isInCombat()) {
        if (obj != NULL) {
            register_object_animate_forever(obj, anim, -1);
        } else {
            dbg_error(program, "reg_anim_animate_forever", SCRIPT_ERROR_OBJECT_IS_NULL);
        }
    }
}

// 0x450D80
static void op_critter_injure(Program* program)
{
    int flags = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter == NULL) {
        dbg_error(program, "critter_injure", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    flags &= DAM_CRIP;
    critter->data.critter.combat.results |= flags;

    if (critter == obj_dude) {
        if ((flags & DAM_CRIP_ARM_ANY) != 0) {
            intface_update_items(true);
        }
    }
}

// 0x450E28
static void op_combat_is_initialized(Program* program)
{
    programStackPushInteger(program, isInCombat() ? 1 : 0);
}

// 0x450E4C
static void op_gdialog_barter(Program* program)
{
    int data = programStackPopInteger(program);

    if (gdActivateBarter(data) == -1) {
        debug_printf("\nScript Error: gdialog_barter: failed");
    }
}

// 0x450EA0
static void op_difficulty_level(Program* program)
{
    int gameDifficulty;
    if (!config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_GAME_DIFFICULTY_KEY, &gameDifficulty)) {
        gameDifficulty = GAME_DIFFICULTY_NORMAL;
    }

    programStackPushInteger(program, gameDifficulty);
}

// 0x450EEC
static void op_running_burning_guy(Program* program)
{
    int runningBurningGuy;
    if (!config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_RUNNING_BURNING_GUY_KEY, &runningBurningGuy)) {
        runningBurningGuy = 1;
    }

    programStackPushInteger(program, runningBurningGuy);
}

// 0x450F38
static void op_inven_unwield(Program* program)
{
    Object* obj;
    int v1;

    obj = scr_find_obj_from_program(program);
    v1 = 1;

    if (obj == obj_dude && !intface_is_item_right_hand()) {
        v1 = 0;
    }

    inven_unwield(obj, v1);
}

// 0x450F68
static void op_obj_is_locked(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    bool locked = false;
    if (object != NULL) {
        locked = obj_is_locked(object);
    } else {
        dbg_error(program, "obj_is_locked", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, locked ? 1 : 0);
}

// 0x450FDC
static void op_obj_lock(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != NULL) {
        obj_lock(object);
    } else {
        dbg_error(program, "obj_lock", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// 0x451034
static void op_obj_unlock(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != NULL) {
        obj_unlock(object);
    } else {
        dbg_error(program, "obj_unlock", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// 0x45108C
static void op_obj_is_open(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    bool isOpen = false;
    if (object != NULL) {
        isOpen = obj_is_open(object);
    } else {
        dbg_error(program, "obj_is_open", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, isOpen ? 1 : 0);
}

// 0x451100
static void op_obj_open(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != NULL) {
        obj_open(object);
    } else {
        dbg_error(program, "obj_open", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// 0x451158
static void op_obj_close(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != NULL) {
        obj_close(object);
    } else {
        dbg_error(program, "obj_close", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// 0x4511B0
static void op_game_ui_disable(Program* program)
{
    game_ui_disable(0);
}

// 0x4511B8
static void op_game_ui_enable(Program* program)
{
    game_ui_enable();
}

// 0x4511C0
static void op_game_ui_is_disabled(Program* program)
{
    programStackPushInteger(program, game_ui_is_disabled());
}

// 0x4511E4
static void op_gfade_out(Program* program)
{
    int data = programStackPopInteger(program);

    if (data != 0) {
        palette_fade_to(black_palette);
    } else {
        dbg_error(program, "gfade_out", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// 0x451240
static void op_gfade_in(Program* program)
{
    int data = programStackPopInteger(program);

    if (data != 0) {
        palette_fade_to(cmap);
    } else {
        dbg_error(program, "gfade_in", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// 0x45129C
static void op_item_caps_total(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int amount = 0;
    if (object != NULL) {
        amount = item_caps_total(object);
    } else {
        dbg_error(program, "item_caps_total", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, amount);
}

// 0x451310
static void op_item_caps_adjust(Program* program)
{
    int amount = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int rc = -1;

    if (object != NULL) {
        rc = item_caps_adjust(object, amount);
    } else {
        dbg_error(program, "item_caps_adjust", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, rc);
}

// 0x4513B0
static void op_anim_action_frame(Program* program)
{
    int anim = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int actionFrame = 0;

    if (object != NULL) {
        int fid = art_id(FID_TYPE(object->fid), object->fid & 0xFFF, anim, 0, object->rotation);
        CacheEntry* frmHandle;
        Art* frm = art_ptr_lock(fid, &frmHandle);
        if (frm != NULL) {
            actionFrame = art_frame_action_frame(frm);
            art_ptr_unlock(frmHandle);
        }
    } else {
        dbg_error(program, "anim_action_frame", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, actionFrame);
}

// 0x451480
static void op_reg_anim_play_sfx(Program* program)
{
    int delay = programStackPopInteger(program);
    char* soundEffectName = programStackPopString(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (soundEffectName == NULL) {
        dbg_error(program, "reg_anim_play_sfx", SCRIPT_ERROR_FOLLOWS);
        debug_printf(" Can't match string!");
    }

    if (obj != NULL) {
        register_object_play_sfx(obj, soundEffectName, delay);
    } else {
        dbg_error(program, "reg_anim_play_sfx", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// 0x45155C
static void op_critter_mod_skill(Program* program)
{
    int points = programStackPopInteger(program);
    int skill = programStackPopInteger(program);
    Object* critter = static_cast<Object*>(programStackPopPointer(program));

    if (critter != NULL && points != 0) {
        if (PID_TYPE(critter->pid) == OBJ_TYPE_CRITTER) {
            if (critter == obj_dude) {
                if (stat_pc_set(PC_STAT_UNSPENT_SKILL_POINTS, stat_pc_get(PC_STAT_UNSPENT_SKILL_POINTS) + points) == 0) {
                    for (int it = 0; it < points; it++) {
                        skill_inc_point(obj_dude, skill);
                    }
                }
            } else {
                dbg_error(program, "critter_mod_skill", SCRIPT_ERROR_FOLLOWS);
                debug_printf(" Can't modify anyone except obj_dude!");
            }
        }
    } else {
        dbg_error(program, "critter_mod_skill", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, 0);
}

// 0x45166C
static void op_sfx_build_char_name(Program* program)
{
    int extra = programStackPopInteger(program);
    int anim = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    if (obj != NULL) {
        char soundEffectName[16];
        strcpy(soundEffectName, gsnd_build_character_sfx_name(obj, anim, extra));
        programStackPushString(program, soundEffectName);
    } else {
        dbg_error(program, "sfx_build_char_name", SCRIPT_ERROR_OBJECT_IS_NULL);
        programStackPushString(program, NULL);
    }
}

// 0x451734
static void op_sfx_build_ambient_name(Program* program)
{
    char* baseName = programStackPopString(program);

    char soundEffectName[16];
    strcpy(soundEffectName, gsnd_build_ambient_sfx_name(baseName));
    programStackPushString(program, soundEffectName);
}

// 0x4517C8
static void op_sfx_build_interface_name(Program* program)
{
    const char* baseName = programStackPopString(program);

    char soundEffectName[16];
    strcpy(soundEffectName, gsnd_build_interface_sfx_name(baseName));
    programStackPushString(program, soundEffectName);
}

// 0x45185C
static void op_sfx_build_item_name(Program* program)
{
    const char* baseName = programStackPopString(program);

    char soundEffectName[16];
    strcpy(soundEffectName, gsnd_build_interface_sfx_name(baseName));
    programStackPushString(program, soundEffectName);
}

// 0x4518F0
static void op_sfx_build_weapon_name(Program* program)
{
    Object* target = static_cast<Object*>(programStackPopPointer(program));
    int hitMode = programStackPopInteger(program);
    Object* weapon = static_cast<Object*>(programStackPopPointer(program));
    int weaponSfxType = programStackPopInteger(program);

    char soundEffectName[16];
    strcpy(soundEffectName, gsnd_build_weapon_sfx_name(weaponSfxType, weapon, hitMode, target));
    programStackPushString(program, soundEffectName);
}

// 0x4519A4
static void op_sfx_build_scenery_name(Program* program)
{
    int actionType = programStackPopInteger(program);
    int action = programStackPopInteger(program);
    char* baseName = programStackPopString(program);

    char soundEffectName[16];
    strcpy(soundEffectName, gsnd_build_scenery_sfx_name(actionType, action, baseName));
    programStackPushString(program, soundEffectName);
}

// 0x451A60
static void op_sfx_build_open_name(Program* program)
{
    int action = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != NULL) {
        char soundEffectName[16];
        strcpy(soundEffectName, gsnd_build_open_sfx_name(object, action));
        programStackPushString(program, soundEffectName);
    } else {
        dbg_error(program, "sfx_build_open_name", SCRIPT_ERROR_OBJECT_IS_NULL);
        programStackPushString(program, NULL);
    }
}

// 0x451B20
static void op_attack_setup(Program* program)
{
    Object* defender = static_cast<Object*>(programStackPopPointer(program));
    Object* attacker = static_cast<Object*>(programStackPopPointer(program));

    program->flags |= PROGRAM_FLAG_0x20;

    if (attacker != NULL) {
        if (!critter_is_active(attacker)) {
            dbg_print_com_data(attacker, defender);
            debug_printf("\n   But is already dead");
            program->flags &= ~PROGRAM_FLAG_0x20;
            return;
        }

        if (!critter_is_active(defender)) {
            dbg_print_com_data(attacker, defender);
            debug_printf("\n   But target is already dead");
            program->flags &= ~PROGRAM_FLAG_0x20;
            return;
        }

        if ((defender->data.critter.combat.maneuver & CRITTER_MANUEVER_FLEEING) != 0) {
            dbg_print_com_data(attacker, defender);
            debug_printf("\n   But target is AFRAID");
            program->flags &= ~PROGRAM_FLAG_0x20;
            return;
        }

        if (isInCombat()) {
            if ((attacker->data.critter.combat.maneuver & CRITTER_MANEUVER_ENGAGING) == 0) {
                attacker->data.critter.combat.maneuver |= CRITTER_MANEUVER_ENGAGING;
                attacker->data.critter.combat.whoHitMe = defender;
            }
        } else {
            STRUCT_664980 attack;
            attack.attacker = attacker;
            attack.defender = defender;
            attack.actionPointsBonus = 0;
            attack.accuracyBonus = 0;
            attack.damageBonus = 0;
            attack.minDamage = 0;
            attack.maxDamage = INT_MAX;
            attack.field_1C = 0;

            dbg_print_com_data(attacker, defender);
            scripts_request_combat(&attack);
        }
    }

    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x451CC4
static void op_destroy_mult_objs(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;

    int quantity = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    Object* self = scr_find_obj_from_program(program);
    bool isSelf = self == object;

    int result = 0;

    if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
        combat_delete_critter(object);
    }

    Object* owner = obj_top_environment(object);
    if (owner != NULL) {
        int quantityToDestroy = item_count(owner, object);
        if (quantityToDestroy > quantity) {
            quantityToDestroy = quantity;
        }

        item_remove_mult(owner, object, quantityToDestroy);

        if (owner == obj_dude) {
            intface_update_items(true);
        }

        obj_connect(object, 1, 0, NULL);

        if (isSelf) {
            object->sid = -1;
            object->flags |= (OBJECT_HIDDEN | OBJECT_NO_SAVE);
        } else {
            register_clear(object);
            obj_erase_object(object, NULL);
        }

        result = quantityToDestroy;
    } else {
        register_clear(object);

        Rect rect;
        obj_erase_object(object, &rect);
        tile_refresh_rect(&rect, map_elevation);
    }

    programStackPushInteger(program, result);

    program->flags &= ~PROGRAM_FLAG_0x20;

    if (isSelf) {
        program->flags |= PROGRAM_FLAG_0x0100;
    }
}

// 0x451E30
static void op_use_obj_on_obj(Program* program)
{
    Object* target = static_cast<Object*>(programStackPopPointer(program));
    Object* item = static_cast<Object*>(programStackPopPointer(program));

    if (item == NULL) {
        dbg_error(program, "use_obj_on_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (target == NULL) {
        dbg_error(program, "use_obj_on_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    Script* script;
    int sid = scr_find_sid_from_program(program);
    if (scr_ptr(sid, &script) == -1) {
        // FIXME: Should be SCRIPT_ERROR_CANT_MATCH_PROGRAM_TO_SID.
        dbg_error(program, "use_obj_on_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    Object* self = scr_find_obj_from_program(program);
    if (PID_TYPE(self->pid) == OBJ_TYPE_CRITTER) {
        action_use_an_item_on_object(self, target, item);
    } else {
        obj_use_item_on(self, target, item);
    }
}

// 0x451F2C
static void op_endgame_slideshow(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;
    scripts_request_endgame_slideshow();
    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x451F4C
static void op_move_obj_inven_to_obj(Program* program)
{
    Object* object2 = static_cast<Object*>(programStackPopPointer(program));
    Object* object1 = static_cast<Object*>(programStackPopPointer(program));

    if (object1 == NULL) {
        dbg_error(program, "move_obj_inven_to_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    if (object2 == NULL) {
        dbg_error(program, "move_obj_inven_to_obj", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    Object* oldArmor = NULL;
    Object* item2 = NULL;
    if (object1 == obj_dude) {
        oldArmor = inven_worn(object1);
    } else {
        item2 = inven_right_hand(object1);
    }

    if (object1 != obj_dude && item2 != NULL) {
        int flags = 0;
        if ((item2->flags & OBJECT_IN_LEFT_HAND) != 0) {
            flags |= OBJECT_IN_LEFT_HAND;
        }

        if ((item2->flags & OBJECT_IN_RIGHT_HAND) != 0) {
            flags |= OBJECT_IN_RIGHT_HAND;
        }

        correctFidForRemovedItem(object1, item2, flags);
    }

    item_move_all(object1, object2);

    if (object1 == obj_dude) {
        if (oldArmor != NULL) {
            adjust_ac(obj_dude, oldArmor, NULL);
        }

        proto_dude_update_gender();

        intface_update_items(true);
    }
}

// 0x45207C
static void op_endgame_movie(Program* program)
{
    program->flags |= PROGRAM_FLAG_0x20;
    endgame_movie();
    program->flags &= ~PROGRAM_FLAG_0x20;
}

// 0x45209C
static void op_obj_art_fid(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int fid = 0;
    if (object != NULL) {
        fid = object->fid;
    } else {
        dbg_error(program, "obj_art_fid", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, fid);
}

// 0x452108
static void op_art_anim(Program* program)
{
    int data = programStackPopInteger(program);
    programStackPushInteger(program, (data & 0xFF0000) >> 16);
}

// 0x452160
static void op_party_member_obj(Program* program)
{
    int data = programStackPopInteger(program);

    Object* object = partyMemberFindObjFromPid(data);
    programStackPushPointer(program, object);
}

// 0x4521B4
static void op_rotation_to_tile(Program* program)
{
    int tile2 = programStackPopInteger(program);
    int tile1 = programStackPopInteger(program);

    int rotation = tile_dir(tile1, tile2);
    programStackPushInteger(program, rotation);
}

// 0x452234
static void op_jam_lock(Program* program)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    obj_jam_lock(object);
}

// 0x452274
static void op_gdialog_set_barter_mod(Program* program)
{
    int data = programStackPopInteger(program);

    gdialogSetBarterMod(data);
}

// 0x4522B4
static void op_combat_difficulty(Program* program)
{
    int combatDifficulty;
    if (!config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_DIFFICULTY_KEY, &combatDifficulty)) {
        combatDifficulty = 0;
    }

    programStackPushInteger(program, combatDifficulty);
}

// 0x4522FC
static void op_obj_on_screen(Program* program)
{
    // 0x44B598
    static Rect rect = { 0, 0, 640, 480 };

    Object* object = static_cast<Object*>(programStackPopPointer(program));

    int result = 0;

    if (object != NULL) {
        if (map_elevation == object->elevation) {
            Rect objectRect;
            obj_bound(object, &objectRect);

            if (rect_inside_bound(&objectRect, &rect, &objectRect) == 0) {
                result = 1;
            }
        }
    } else {
        dbg_error(program, "obj_on_screen", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, result);
}

// 0x4523A8
static void op_critter_is_fleeing(Program* program)
{
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    bool fleeing = false;
    if (obj != NULL) {
        fleeing = (obj->data.critter.combat.maneuver & CRITTER_MANUEVER_FLEEING) != 0;
    } else {
        dbg_error(program, "critter_is_fleeing", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushInteger(program, fleeing ? 1 : 0);
}

// 0x452424
static void op_critter_set_flee_state(Program* program)
{
    int fleeing = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    if (object != NULL) {
        if (fleeing != 0) {
            object->data.critter.combat.maneuver |= CRITTER_MANUEVER_FLEEING;
        } else {
            object->data.critter.combat.maneuver &= ~CRITTER_MANUEVER_FLEEING;
        }
    } else {
        dbg_error(program, "critter_set_flee_state", SCRIPT_ERROR_OBJECT_IS_NULL);
    }
}

// 0x4524B0
static void op_terminate_combat(Program* program)
{
    if (isInCombat()) {
        game_user_wants_to_quit = 1;
    }
}

// 0x4524C4
static void op_debug_msg(Program* program)
{
    char* string = programStackPopString(program);

    if (string != NULL) {
        bool showScriptMessages = false;
        configGetBool(&game_config, GAME_CONFIG_DEBUG_KEY, GAME_CONFIG_SHOW_SCRIPT_MESSAGES_KEY, &showScriptMessages);
        if (showScriptMessages) {
            debug_printf("\n");
            debug_printf(string);
        }
    }
}

// 0x452554
static void op_critter_stop_attacking(Program* program)
{
    Object* critter = static_cast<Object*>(programStackPopPointer(program));
    if (critter == NULL) {
        dbg_error(program, "critter_stop_attacking", SCRIPT_ERROR_OBJECT_IS_NULL);
        return;
    }

    critter->data.critter.combat.maneuver |= CRITTER_MANEUVER_DISENGAGING;
    critter->data.critter.combat.whoHitMe = NULL;
}

// 0x4525B8
static void op_tile_contains_pid_obj(Program* program)
{
    int pid = programStackPopInteger(program);
    int elevation = programStackPopInteger(program);
    int tile = programStackPopInteger(program);
    Object* found = NULL;

    if (tile != -1) {
        Object* object = obj_find_first_at(elevation);
        while (object != NULL) {
            if (object->tile == tile && object->pid == pid) {
                found = object;
                break;
            }
            object = obj_find_next_at();
        }
    }

    programStackPushPointer(program, found);
}

// 0x452668
static void op_obj_name(Program* program)
{
    // 0x505504
    static char* strName = _aCritter;

    Object* obj = static_cast<Object*>(programStackPopPointer(program));
    if (obj != NULL) {
        strName = object_name(obj);
    } else {
        dbg_error(program, "obj_name", SCRIPT_ERROR_OBJECT_IS_NULL);
    }

    programStackPushString(program, strName);
}

// 0x4526E8
static void op_get_pc_stat(Program* program)
{
    int data = programStackPopInteger(program);
    programStackPushInteger(program, stat_pc_get(data));
}

// 0x45273C
void intExtraClose()
{
}

// 0x452740
void initIntExtra()
{
    interpretAddFunc(0x80A1, op_give_exp_points);
    interpretAddFunc(0x80A2, op_scr_return);
    interpretAddFunc(0x80A3, op_play_sfx);
    interpretAddFunc(0x80A4, op_obj_name);
    interpretAddFunc(0x80A5, op_sfx_build_open_name);
    interpretAddFunc(0x80A6, op_get_pc_stat);
    interpretAddFunc(0x80A7, op_tile_contains_pid_obj);
    interpretAddFunc(0x80A8, op_set_map_start);
    interpretAddFunc(0x80A9, op_override_map_start);
    interpretAddFunc(0x80AA, op_has_skill);
    interpretAddFunc(0x80AB, op_using_skill);
    interpretAddFunc(0x80AC, op_roll_vs_skill);
    interpretAddFunc(0x80AD, op_skill_contest);
    interpretAddFunc(0x80AE, op_do_check);
    interpretAddFunc(0x80AF, op_is_success);
    interpretAddFunc(0x80B0, op_is_critical);
    interpretAddFunc(0x80B1, op_how_much);
    interpretAddFunc(0x80B2, op_reaction_roll);
    interpretAddFunc(0x80B3, op_reaction_influence);
    interpretAddFunc(0x80B4, op_random);
    interpretAddFunc(0x80B5, op_roll_dice);
    interpretAddFunc(0x80B6, op_move_to);
    interpretAddFunc(0x80B7, op_create_object_sid);
    interpretAddFunc(0x80B8, op_display_msg);
    interpretAddFunc(0x80B9, op_script_overrides);
    interpretAddFunc(0x80BA, op_obj_is_carrying_obj_pid);
    interpretAddFunc(0x80BB, op_tile_contains_obj_pid);
    interpretAddFunc(0x80BC, op_self_obj);
    interpretAddFunc(0x80BD, op_source_obj);
    interpretAddFunc(0x80BE, op_target_obj);
    interpretAddFunc(0x80BF, op_dude_obj);
    interpretAddFunc(0x80C0, op_obj_being_used_with);
    interpretAddFunc(0x80C1, op_local_var);
    interpretAddFunc(0x80C2, op_set_local_var);
    interpretAddFunc(0x80C3, op_map_var);
    interpretAddFunc(0x80C4, op_set_map_var);
    interpretAddFunc(0x80C5, op_global_var);
    interpretAddFunc(0x80C6, op_set_global_var);
    interpretAddFunc(0x80C7, op_script_action);
    interpretAddFunc(0x80C8, op_obj_type);
    interpretAddFunc(0x80C9, op_obj_item_subtype);
    interpretAddFunc(0x80CA, op_get_critter_stat);
    interpretAddFunc(0x80CB, op_set_critter_stat);
    interpretAddFunc(0x80CC, op_animate_stand_obj);
    interpretAddFunc(0x80CD, op_animate_stand_reverse_obj);
    interpretAddFunc(0x80CE, op_animate_move_obj_to_tile);
    interpretAddFunc(0x80CF, op_animate_jump);
    interpretAddFunc(0x80D0, op_attack);
    interpretAddFunc(0x80D1, op_make_daytime);
    interpretAddFunc(0x80D2, op_tile_distance);
    interpretAddFunc(0x80D3, op_tile_distance_objs);
    interpretAddFunc(0x80D4, op_tile_num);
    interpretAddFunc(0x80D5, op_tile_num_in_direction);
    interpretAddFunc(0x80D6, op_pickup_obj);
    interpretAddFunc(0x80D7, op_drop_obj);
    interpretAddFunc(0x80D8, op_add_obj_to_inven);
    interpretAddFunc(0x80D9, op_rm_obj_from_inven);
    interpretAddFunc(0x80DA, op_wield_obj_critter);
    interpretAddFunc(0x80DB, op_use_obj);
    interpretAddFunc(0x80DC, op_obj_can_see_obj);
    interpretAddFunc(0x80DD, op_attack);
    interpretAddFunc(0x80DE, op_start_gdialog);
    interpretAddFunc(0x80DF, op_end_dialogue);
    interpretAddFunc(0x80E0, op_dialogue_reaction);
    interpretAddFunc(0x80E1, op_turn_off_objs_in_area);
    interpretAddFunc(0x80E2, op_turn_on_objs_in_area);
    interpretAddFunc(0x80E3, op_set_obj_visibility);
    interpretAddFunc(0x80E4, op_load_map);
    interpretAddFunc(0x80E5, op_barter_offer);
    interpretAddFunc(0x80E6, op_barter_asking);
    interpretAddFunc(0x80E7, op_anim_busy);
    interpretAddFunc(0x80E8, op_critter_heal);
    interpretAddFunc(0x80E9, op_set_light_level);
    interpretAddFunc(0x80EA, op_game_time);
    interpretAddFunc(0x80EB, op_game_time_in_seconds);
    interpretAddFunc(0x80EC, op_elevation);
    interpretAddFunc(0x80ED, op_kill_critter);
    interpretAddFunc(0x80EE, op_kill_critter_type);
    interpretAddFunc(0x80EF, op_critter_damage);
    interpretAddFunc(0x80F0, op_add_timer_event);
    interpretAddFunc(0x80F1, op_rm_timer_event);
    interpretAddFunc(0x80F2, op_game_ticks);
    interpretAddFunc(0x80F3, op_has_trait);
    interpretAddFunc(0x80F4, op_destroy_object);
    interpretAddFunc(0x80F5, op_obj_can_hear_obj);
    interpretAddFunc(0x80F6, op_game_time_hour);
    interpretAddFunc(0x80F7, op_fixed_param);
    interpretAddFunc(0x80F8, op_tile_is_visible);
    interpretAddFunc(0x80F9, op_dialogue_system_enter);
    interpretAddFunc(0x80FA, op_action_being_used);
    interpretAddFunc(0x80FB, op_critter_state);
    interpretAddFunc(0x80FC, op_game_time_advance);
    interpretAddFunc(0x80FD, op_radiation_inc);
    interpretAddFunc(0x80FE, op_radiation_dec);
    interpretAddFunc(0x80FF, op_critter_attempt_placement);
    interpretAddFunc(0x8100, op_obj_pid);
    interpretAddFunc(0x8101, op_cur_map_index);
    interpretAddFunc(0x8102, op_critter_add_trait);
    interpretAddFunc(0x8103, op_critter_rm_trait);
    interpretAddFunc(0x8104, op_proto_data);
    interpretAddFunc(0x8105, op_message_str);
    interpretAddFunc(0x8106, op_critter_inven_obj);
    interpretAddFunc(0x8107, op_obj_set_light_level);
    interpretAddFunc(0x8108, op_world_map);
    interpretAddFunc(0x8109, op_town_map);
    interpretAddFunc(0x810A, op_float_msg);
    interpretAddFunc(0x810B, op_metarule);
    interpretAddFunc(0x810C, op_anim);
    interpretAddFunc(0x810D, op_obj_carrying_pid_obj);
    interpretAddFunc(0x810E, op_reg_anim_func);
    interpretAddFunc(0x810F, op_reg_anim_animate);
    interpretAddFunc(0x8110, op_reg_anim_animate_reverse);
    interpretAddFunc(0x8111, op_reg_anim_obj_move_to_obj);
    interpretAddFunc(0x8112, op_reg_anim_obj_run_to_obj);
    interpretAddFunc(0x8113, op_reg_anim_obj_move_to_tile);
    interpretAddFunc(0x8114, op_reg_anim_obj_run_to_tile);
    interpretAddFunc(0x8115, op_play_gmovie);
    interpretAddFunc(0x8116, op_add_mult_objs_to_inven);
    interpretAddFunc(0x8117, op_rm_mult_objs_from_inven);
    interpretAddFunc(0x8118, op_get_month);
    interpretAddFunc(0x8119, op_get_day);
    interpretAddFunc(0x811A, op_explosion);
    interpretAddFunc(0x811B, op_days_since_visited);
    interpretAddFunc(0x811C, op_gsay_start);
    interpretAddFunc(0x811D, op_gsay_end);
    interpretAddFunc(0x811E, op_gsay_reply);
    interpretAddFunc(0x811F, op_gsay_option);
    interpretAddFunc(0x8120, op_gsay_message);
    interpretAddFunc(0x8121, op_giq_option);
    interpretAddFunc(0x8122, op_poison);
    interpretAddFunc(0x8123, op_get_poison);
    interpretAddFunc(0x8124, op_party_add);
    interpretAddFunc(0x8125, op_party_remove);
    interpretAddFunc(0x8126, op_reg_anim_animate_forever);
    interpretAddFunc(0x8127, op_critter_injure);
    interpretAddFunc(0x8128, op_combat_is_initialized);
    interpretAddFunc(0x8129, op_gdialog_barter);
    interpretAddFunc(0x812A, op_difficulty_level);
    interpretAddFunc(0x812B, op_running_burning_guy);
    interpretAddFunc(0x812C, op_inven_unwield);
    interpretAddFunc(0x812D, op_obj_is_locked);
    interpretAddFunc(0x812E, op_obj_lock);
    interpretAddFunc(0x812F, op_obj_unlock);
    interpretAddFunc(0x8131, op_obj_open);
    interpretAddFunc(0x8130, op_obj_is_open);
    interpretAddFunc(0x8132, op_obj_close);
    interpretAddFunc(0x8133, op_game_ui_disable);
    interpretAddFunc(0x8134, op_game_ui_enable);
    interpretAddFunc(0x8135, op_game_ui_is_disabled);
    interpretAddFunc(0x8136, op_gfade_out);
    interpretAddFunc(0x8137, op_gfade_in);
    interpretAddFunc(0x8138, op_item_caps_total);
    interpretAddFunc(0x8139, op_item_caps_adjust);
    interpretAddFunc(0x813A, op_anim_action_frame);
    interpretAddFunc(0x813B, op_reg_anim_play_sfx);
    interpretAddFunc(0x813C, op_critter_mod_skill);
    interpretAddFunc(0x813D, op_sfx_build_char_name);
    interpretAddFunc(0x813E, op_sfx_build_ambient_name);
    interpretAddFunc(0x813F, op_sfx_build_interface_name);
    interpretAddFunc(0x8140, op_sfx_build_item_name);
    interpretAddFunc(0x8141, op_sfx_build_weapon_name);
    interpretAddFunc(0x8142, op_sfx_build_scenery_name);
    interpretAddFunc(0x8143, op_attack_setup);
    interpretAddFunc(0x8144, op_destroy_mult_objs);
    interpretAddFunc(0x8145, op_use_obj_on_obj);
    interpretAddFunc(0x8146, op_endgame_slideshow);
    interpretAddFunc(0x8147, op_move_obj_inven_to_obj);
    interpretAddFunc(0x8148, op_endgame_movie);
    interpretAddFunc(0x8149, op_obj_art_fid);
    interpretAddFunc(0x814A, op_art_anim);
    interpretAddFunc(0x814B, op_party_member_obj);
    interpretAddFunc(0x814C, op_rotation_to_tile);
    interpretAddFunc(0x814D, op_jam_lock);
    interpretAddFunc(0x814E, op_gdialog_set_barter_mod);
    interpretAddFunc(0x814F, op_combat_difficulty);
    interpretAddFunc(0x8150, op_obj_on_screen);
    interpretAddFunc(0x8151, op_critter_is_fleeing);
    interpretAddFunc(0x8152, op_critter_set_flee_state);
    interpretAddFunc(0x8153, op_terminate_combat);
    interpretAddFunc(0x8154, op_debug_msg);
    interpretAddFunc(0x8155, op_critter_stop_attacking);
}

// 0x4531E0
void updateIntExtra()
{
}

// 0x4531E0
void intExtraRemoveProgramReferences(Program* program)
{
}

} // namespace fallout
