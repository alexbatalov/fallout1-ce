#include "game/critter.h"

#include <stdio.h>
#include <string.h>

#include "game/anim.h"
#include "game/combat.h"
#include "game/display.h"
#include "game/editor.h"
#include "game/endgame.h"
#include "game/game.h"
#include "game/intface.h"
#include "game/item.h"
#include "game/map.h"
#include "game/message.h"
#include "game/object.h"
#include "game/party.h"
#include "game/proto.h"
#include "game/queue.h"
#include "game/reaction.h"
#include "game/roll.h"
#include "game/scripts.h"
#include "game/skill.h"
#include "game/stat.h"
#include "game/tile.h"
#include "game/trait.h"
#include "game/worldmap.h"
#include "platform_compat.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/memory.h"
#include "plib/gnw/rect.h"

namespace fallout {

static int get_rad_damage_level(Object* obj, void* data);
static int clear_rad_damage(Object* obj, void* data);
static void process_rads(Object* obj, int radiationLevel, bool direction);
static int critter_kill_count_clear();

// TODO: Remove.
// 0x4F0DF4
char _aCorpse[] = "corpse";

// TODO: Remove.
// 0x501494
char byte_501494[] = "";

// List of stats affected by radiation.
//
// The values of this list specify stats that can be affected by radiation.
// The amount of penalty to every stat (identified by index) is stored
// separately in [rad_bonus] per radiation level.
//
// The order of stats is important - primary stats must be at the top. See
// [RADIATION_EFFECT_PRIMARY_STAT_COUNT] for more info.
//
// 0x504D5C
int rad_stat[RADIATION_EFFECT_COUNT] = {
    STAT_STRENGTH,
    STAT_PERCEPTION,
    STAT_ENDURANCE,
    STAT_CHARISMA,
    STAT_INTELLIGENCE,
    STAT_AGILITY,
    STAT_CURRENT_HIT_POINTS,
    STAT_HEALING_RATE,
};

// Denotes how many primary stats at the top of [rad_stat] array.
// These stats are used to determine if critter is alive after applying
// radiation effects.
#define RADIATION_EFFECT_PRIMARY_STAT_COUNT 6

// List of stat modifiers caused by radiation at different radiation levels.
//
// 0x504D7C
int rad_bonus[RADIATION_LEVEL_COUNT][RADIATION_EFFECT_COUNT] = {
    // clang-format off
    {   0,   0,   0,   0,   0,   0,   0,   0 },
    {  -1,   0,   0,   0,   0,   0,   0,   0 },
    {  -1,   0,   0,   0,   0,  -1,   0,  -3 },
    {  -2,   0,  -1,   0,   0,  -2,  -5,  -5 },
    {  -4,  -3,  -3,  -3,  -1,  -5, -15, -10 },
    {  -6,  -5,  -5,  -5,  -3,  -6, -20, -10 },
    // clang-format on
};

// scrname.msg
//
// 0x56BEF4
static MessageList critter_scrmsg_file;

// 0x56BEFC
static char pc_name[DUDE_NAME_MAX_LENGTH];

// 0x56BF1C
static int sneak_working;

// 0x56BF24
static int pc_kill_counts[KILL_TYPE_COUNT];

// 0x56BF20
static int old_rad_level;

// 0x427860
int critter_init()
{
    critter_pc_reset_name();

    // NOTE: Uninline.
    critter_kill_count_clear();

    if (!message_init(&critter_scrmsg_file)) {
        debug_printf("\nError: Initing critter name message file!");
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%sscrname.msg", msg_path);

    if (!message_load(&critter_scrmsg_file, path)) {
        debug_printf("\nError: Loading critter name message file!");
        return -1;
    }

    return 0;
}

// 0x4278F4
void critter_reset()
{
    critter_pc_reset_name();

    // NOTE: Uninline;
    critter_kill_count_clear();
}

// 0x427914
void critter_exit()
{
    message_exit(&critter_scrmsg_file);
}

// 0x42792C
int critter_load(DB_FILE* stream)
{
    if (db_freadInt(stream, &sneak_working) == -1) {
        return -1;
    }

    Proto* proto;
    proto_ptr(obj_dude->pid, &proto);

    return critter_read_data(stream, &(proto->critter.data));
}

// 0x427968
int critter_save(DB_FILE* stream)
{
    if (db_fwriteInt(stream, sneak_working) == -1) {
        return -1;
    }

    Proto* proto;
    proto_ptr(obj_dude->pid, &proto);

    return critter_write_data(stream, &(proto->critter.data));
}

// 0x4279A4
void critter_copy(CritterProtoData* dest, CritterProtoData* src)
{
    memcpy(dest, src, sizeof(CritterProtoData));
}

// 0x4279B8
char* critter_name(Object* critter)
{
    // TODO: Rename.
    // 0x504D40
    static char* _name_critter = _aCorpse;

    if (critter == obj_dude) {
        return pc_name;
    }

    if (critter->field_80 == -1) {
        if (critter->sid != -1) {
            Script* script;
            if (scr_ptr(critter->sid, &script) != -1) {
                critter->field_80 = script->scr_script_idx;
            }
        }
    }

    char* name = NULL;
    if (critter->field_80 != -1) {
        MessageListItem messageListItem;
        messageListItem.num = 101 + critter->field_80;
        if (message_search(&critter_scrmsg_file, &messageListItem)) {
            name = messageListItem.text;
        }
    }

    if (name == NULL || *name == '\0') {
        name = proto_name(critter->pid);
    }

    _name_critter = name;

    return name;
}

// 0x427A48
int critter_pc_set_name(const char* name)
{
    if (strlen(name) <= DUDE_NAME_MAX_LENGTH) {
        strncpy(pc_name, name, DUDE_NAME_MAX_LENGTH);
        return 0;
    }

    return -1;
}

// 0x427A80
void critter_pc_reset_name()
{
    strncpy(pc_name, "None", DUDE_NAME_MAX_LENGTH);
}

// 0x427A9C
int critter_get_hits(Object* critter)
{
    return critter->data.critter.hp;
}

// 0x427AA0
int critter_adjust_hits(Object* critter, int hp)
{
    int maximumHp = stat_level(critter, STAT_MAXIMUM_HIT_POINTS);
    int newHp = critter->data.critter.hp + hp;

    critter->data.critter.hp = newHp;
    if (maximumHp >= newHp) {
        if (newHp <= 0 && (critter->data.critter.combat.results & DAM_DEAD) == 0) {
            critter_kill(critter, -1, true);
        }
    } else {
        critter->data.critter.hp = maximumHp;
    }

    return 0;
}

// 0x427AE8
int critter_get_poison(Object* critter)
{
    return critter->data.critter.poison;
}

// Adjust critter's current poison by specified amount.
//
// For unknown reason this function only works on dude.
//
// The [amount] can either be positive (adds poison) or negative (removes
// poison).
//
// 0x427AEC
int critter_adjust_poison(Object* critter, int amount)
{
    MessageListItem messageListItem;

    if (critter != obj_dude) {
        return -1;
    }

    if (amount > 0) {
        // Take poison resistance into account.
        amount -= amount * stat_level(critter, STAT_POISON_RESISTANCE) / 100;
    }

    critter->data.critter.poison += amount;
    if (critter->data.critter.poison > 0) {
        queue_clear_type(EVENT_TYPE_POISON, NULL);
        queue_add(10 * (505 - 5 * critter->data.critter.poison), obj_dude, NULL, EVENT_TYPE_POISON);

        // You have been poisoned!
        messageListItem.num = 3000;
        if (message_search(&misc_message_file, &messageListItem)) {
            display_print(messageListItem.text);
        }
    } else {
        critter->data.critter.poison = 0;
    }

    return 0;
}

// 0x427BAC
int critter_check_poison(Object* obj, void* data)
{
    MessageListItem messageListItem;

    if (obj != obj_dude) {
        return 0;
    }

    critter_adjust_poison(obj, -2);
    critter_adjust_hits(obj, -1);

    intface_update_hit_points(false);

    // You take damage from poison.
    messageListItem.num = 3001;
    if (message_search(&misc_message_file, &messageListItem)) {
        display_print(messageListItem.text);
    }

    // NOTE: Uninline.
    if (critter_get_hits(obj) > 5) {
        return 0;
    }

    return 1;
}

// 0x427C14
int critter_get_rads(Object* obj)
{
    return obj->data.critter.radiation;
}

// 0x427C18
int critter_adjust_rads(Object* obj, int amount)
{
    MessageListItem messageListItem;
    Proto* proto;

    if (obj != obj_dude) {
        return -1;
    }

    proto_ptr(obj_dude->pid, &proto);

    if (amount > 0) {
        amount -= stat_level(obj, STAT_RADIATION_RESISTANCE) * amount / 100;
    }

    if (amount > 0) {
        proto->critter.data.flags |= CRITTER_BARTER;
    }

    if (amount > 0) {
        Object* item;
        Object* geiger_counter = NULL;

        item = inven_left_hand(obj_dude);
        if (item != NULL) {
            if (item->pid == PROTO_ID_GEIGER_COUNTER_I || item->pid == PROTO_ID_GEIGER_COUNTER_II) {
                geiger_counter = item;
            }
        }

        item = inven_right_hand(obj_dude);
        if (item != NULL) {
            if (item->pid == PROTO_ID_GEIGER_COUNTER_I || item->pid == PROTO_ID_GEIGER_COUNTER_II) {
                geiger_counter = item;
            }
        }

        if (geiger_counter != NULL) {
            if (item_m_on(geiger_counter)) {
                if (amount > 5) {
                    // The geiger counter is clicking wildly.
                    messageListItem.num = 1009;
                } else {
                    // The geiger counter is clicking.
                    messageListItem.num = 1008;
                }

                if (message_search(&misc_message_file, &messageListItem)) {
                    display_print(messageListItem.text);
                }
            }
        }
    }

    if (amount >= 10) {
        // You have received a large dose of radiation.
        messageListItem.num = 1007;

        if (message_search(&misc_message_file, &messageListItem)) {
            display_print(messageListItem.text);
        }
    }

    obj->data.critter.radiation += amount;
    if (obj->data.critter.radiation < 0) {
        obj->data.critter.radiation = 0;
    }

    return 0;
}

// 0x427D58
int critter_check_rads(Object* obj)
{
    // Modifiers to endurance for performing radiation damage check.
    //
    // 0x504D44
    static int bonus[RADIATION_LEVEL_COUNT] = {
        2,
        0,
        -2,
        -4,
        -6,
        -8,
    };

    Proto* proto;
    int radiation;
    int radiation_level;

    if (obj != obj_dude) {
        return 0;
    }

    proto_ptr(obj->pid, &proto);
    if ((proto->critter.data.flags & CRITTER_BARTER) == 0) {
        return 0;
    }

    old_rad_level = 0;

    queue_clear_type(EVENT_TYPE_RADIATION, get_rad_damage_level);

    // NOTE: Uninline
    radiation = critter_get_rads(obj);

    if (radiation > 999) {
        radiation_level = RADIATION_LEVEL_FATAL;
    } else if (radiation > 599) {
        radiation_level = RADIATION_LEVEL_DEADLY;
    } else if (radiation > 399) {
        radiation_level = RADIATION_LEVEL_CRITICAL;
    } else if (radiation > 199) {
        radiation_level = RADIATION_LEVEL_ADVANCED;
    } else if (radiation > 99) {
        radiation_level = RADIATION_LEVEL_MINOR;
    } else {
        radiation_level = RADIATION_LEVEL_NONE;
    }

    if (stat_result(obj, STAT_ENDURANCE, bonus[radiation_level], NULL) <= ROLL_FAILURE) {
        radiation_level++;
    }

    if (radiation_level > old_rad_level) {
        // Create timer event for applying radiation damage.
        RadiationEvent* radiationEvent = (RadiationEvent*)mem_malloc(sizeof(*radiationEvent));
        if (radiationEvent == NULL) {
            return 0;
        }

        radiationEvent->radiationLevel = radiation_level;
        radiationEvent->isHealing = 0;
        queue_add(GAME_TIME_TICKS_PER_HOUR * roll_random(4, 18), obj, radiationEvent, EVENT_TYPE_RADIATION);
    }

    proto->critter.data.flags &= ~(CRITTER_BARTER);

    return 0;
}

// 0x427E6C
static int get_rad_damage_level(Object* obj, void* data)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)data;

    old_rad_level = radiationEvent->radiationLevel;

    return 0;
}

// 0x427E78
static int clear_rad_damage(Object* obj, void* data)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)data;

    if (radiationEvent->isHealing) {
        process_rads(obj, radiationEvent->radiationLevel, true);
    }

    return 1;
}

// Applies radiation.
//
// 0x427E90
static void process_rads(Object* obj, int radiationLevel, bool isHealing)
{
    MessageListItem messageListItem;

    if (radiationLevel == RADIATION_LEVEL_NONE) {
        return;
    }

    int radiationLevelIndex = radiationLevel - 1;
    int modifier = isHealing ? -1 : 1;

    if (obj == obj_dude) {
        // Radiation level message, higher is worse.
        messageListItem.num = 1000 + radiationLevelIndex;
        if (message_search(&misc_message_file, &messageListItem)) {
            display_print(messageListItem.text);
        }
    }

    for (int effect = 0; effect < RADIATION_EFFECT_COUNT; effect++) {
        int value = stat_get_bonus(obj, rad_stat[effect]);
        value += modifier * rad_bonus[radiationLevelIndex][effect];
        stat_set_bonus(obj, rad_stat[effect], value);
    }

    if ((obj->data.critter.combat.results & DAM_DEAD) == 0) {
        // Loop thru effects affecting primary stats. If any of the primary stat
        // dropped below minimal value, kill it.
        for (int effect = 0; effect < RADIATION_EFFECT_PRIMARY_STAT_COUNT; effect++) {
            int base = stat_get_base(obj, rad_stat[effect]);
            int bonus = stat_get_bonus(obj, rad_stat[effect]);
            if (base + bonus < PRIMARY_STAT_MIN) {
                critter_kill(obj, -1, 1);
                break;
            }
        }
    }

    if ((obj->data.critter.combat.results & DAM_DEAD) != 0) {
        if (obj == obj_dude) {
            // You have died from radiation sickness.
            messageListItem.num = 1006;
            if (message_search(&misc_message_file, &messageListItem)) {
                display_print(messageListItem.text);
            }
        }
    }
}

// 0x427F94
int critter_process_rads(Object* obj, void* data)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)data;
    if (!radiationEvent->isHealing) {
        // Schedule healing stats event in 7 days.
        RadiationEvent* newRadiationEvent = (RadiationEvent*)mem_malloc(sizeof(*newRadiationEvent));
        if (newRadiationEvent != NULL) {
            queue_clear_type(EVENT_TYPE_RADIATION, clear_rad_damage);
            newRadiationEvent->radiationLevel = radiationEvent->radiationLevel;
            newRadiationEvent->isHealing = 1;
            queue_add(GAME_TIME_TICKS_PER_DAY * 7, obj, newRadiationEvent, EVENT_TYPE_RADIATION);
        }
    }

    process_rads(obj, radiationEvent->radiationLevel, radiationEvent->isHealing);

    return 1;
}

// 0x427FF4
int critter_load_rads(DB_FILE* stream, void** dataPtr)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)mem_malloc(sizeof(*radiationEvent));
    if (radiationEvent == NULL) {
        return -1;
    }

    if (db_freadInt(stream, &(radiationEvent->radiationLevel)) == -1) goto err;
    if (db_freadInt(stream, &(radiationEvent->isHealing)) == -1) goto err;

    *dataPtr = radiationEvent;
    return 0;

err:

    mem_free(radiationEvent);
    return -1;
}

// 0x428050
int critter_save_rads(DB_FILE* stream, void* data)
{
    RadiationEvent* radiationEvent = (RadiationEvent*)data;

    if (db_fwriteInt(stream, radiationEvent->radiationLevel) == -1) return -1;
    if (db_fwriteInt(stream, radiationEvent->isHealing) == -1) return -1;

    return 0;
}

// 0x428080
static int critter_kill_count_clear()
{
    memset(pc_kill_counts, 0, sizeof(pc_kill_counts));
    return 0;
}

// 0x428098
int critter_kill_count_inc(int critter_type)
{
    if (critter_type == -1) {
        return -1;
    }

    pc_kill_counts[critter_type] += 1;
    return 0;
}

// 0x4280B8
int critter_kill_count(int critter_type)
{
    return pc_kill_counts[critter_type];
}

// 0x4280C0
int critter_kill_count_load(DB_FILE* stream)
{
    if (db_freadIntCount(stream, pc_kill_counts, KILL_TYPE_COUNT) == -1) {
        db_fclose(stream);
        return -1;
    }

    return 0;
}

// 0x4280F0
int critter_kill_count_save(DB_FILE* stream)
{
    if (db_fwriteIntCount(stream, pc_kill_counts, KILL_TYPE_COUNT) == -1) {
        db_fclose(stream);
        return -1;
    }

    return 0;
}

// 0x428120
int critter_kill_count_type(Object* obj)
{
    Proto* proto;

    if (obj == obj_dude) {
        if (stat_level(obj, STAT_GENDER) == GENDER_FEMALE) {
            return KILL_TYPE_WOMAN;
        }
        return KILL_TYPE_MAN;
    }

    if (PID_TYPE(obj->pid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    proto_ptr(obj->pid, &proto);

    return proto->critter.data.killType;
}

// 0x428174
char* critter_kill_name(int critter_type)
{
    MessageListItem messageListItem;

    if (critter_type >= 0 && critter_type < KILL_TYPE_COUNT) {
        return getmsg(&proto_main_msg_file, &messageListItem, 450 + critter_type);
    } else {
        return NULL;
    }
}

// 0x4281A0
char* critter_kill_info(int critter_type)
{
    MessageListItem messageListItem;

    if (critter_type >= 0 && critter_type < KILL_TYPE_COUNT) {
        return getmsg(&proto_main_msg_file, &messageListItem, 465 + critter_type);
    } else {
        return NULL;
    }
}

// 0x4281CC
int critter_heal_hours(Object* critter, int hours)
{
    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    if (critter->data.critter.hp < stat_level(critter, STAT_MAXIMUM_HIT_POINTS)) {
        critter_adjust_hits(critter, 14 * (hours / 3));
    }

    return 0;
}

// 0x428220
void critter_kill(Object* critter, int anim, bool refresh_window)
{
    int elevation = critter->elevation;

    partyMemberRemove(critter);

    // NOTE: Original code uses goto to jump out from nested conditions below.
    bool shouldChangeFid = false;
    int fid;
    if (critter_is_prone(critter)) {
        int current = FID_ANIM_TYPE(critter->fid);
        if (current == ANIM_FALL_BACK || current == ANIM_FALL_FRONT) {
            bool back = false;
            if (current == ANIM_FALL_BACK) {
                back = true;
            } else {
                fid = art_id(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, ANIM_FALL_FRONT_SF, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
                if (!art_exists(fid)) {
                    back = true;
                }
            }

            if (back) {
                fid = art_id(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, ANIM_FALL_BACK_SF, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
            }

            shouldChangeFid = true;
        }
    } else {
        if (anim < 0) {
            anim = LAST_SF_DEATH_ANIM;
        }

        if (anim > LAST_SF_DEATH_ANIM) {
            debug_printf("\nError: Critter Kill: death_frame out of range!");
            anim = LAST_SF_DEATH_ANIM;
        }

        fid = art_id(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, anim, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
        obj_fix_violence_settings(&fid);
        if (!art_exists(fid)) {
            debug_printf("\nError: Critter Kill: Can't match fid!");

            fid = art_id(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, ANIM_FALL_BACK_BLOOD_SF, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
            obj_fix_violence_settings(&fid);
        }

        shouldChangeFid = true;
    }

    Rect updatedRect;
    Rect tempRect;

    if (shouldChangeFid) {
        obj_set_frame(critter, 0, &updatedRect);

        obj_change_fid(critter, fid, &tempRect);
        rect_min_bound(&updatedRect, &tempRect, &updatedRect);
    }

    if (critter->pid != 16777265 && critter->pid != 16777266 && critter->pid != 16777224) {
        critter->flags |= OBJECT_NO_BLOCK;
        if ((critter->flags & OBJECT_FLAT) == 0) {
            obj_toggle_flat(critter, &tempRect);
        }
    }

    // NOTE: using uninitialized updatedRect/tempRect if fid was not set.

    rect_min_bound(&updatedRect, &tempRect, &updatedRect);

    obj_turn_off_light(critter, &tempRect);
    rect_min_bound(&updatedRect, &tempRect, &updatedRect);

    critter->data.critter.hp = 0;
    critter->data.critter.combat.results |= DAM_DEAD;

    if (critter->sid != -1) {
        scr_remove(critter->sid);
        critter->sid = -1;
    }

    if (refresh_window) {
        tile_refresh_rect(&updatedRect, elevation);
    }

    if (critter == obj_dude) {
        game_user_wants_to_quit = 2;
    }
}

// 0x42844C
int critter_kill_exps(Object* critter)
{
    Proto* proto;
    proto_ptr(critter->pid, &proto);
    return proto->critter.data.experience;
}

// 0x428470
bool critter_is_active(Object* critter)
{
    if (critter == NULL) {
        return false;
    }

    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if ((critter->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD)) != 0) {
        return false;
    }

    if ((critter->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_DEAD | DAM_LOSE_TURN)) != 0) {
        return false;
    }

    return (critter->data.critter.combat.results & DAM_DEAD) == 0;
}

// 0x4284AC
bool critter_is_dead(Object* critter)
{
    if (critter == NULL) {
        return false;
    }

    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if (stat_level(critter, STAT_CURRENT_HIT_POINTS) <= 0) {
        return true;
    }

    if ((critter->data.critter.combat.results & DAM_DEAD) != 0) {
        return true;
    }

    return false;
}

// 0x4284EC
bool critter_is_crippled(Object* critter)
{
    if (critter == NULL) {
        return false;
    }

    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    return (critter->data.critter.combat.results & DAM_CRIP) != 0;
}

// 0x428514
bool critter_is_prone(Object* critter)
{
    if (critter == NULL) {
        return false;
    }

    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    int anim = FID_ANIM_TYPE(critter->fid);

    return (critter->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) != 0
        || (anim >= FIRST_KNOCKDOWN_AND_DEATH_ANIM && anim <= LAST_KNOCKDOWN_AND_DEATH_ANIM)
        || (anim >= FIRST_SF_DEATH_ANIM && anim <= LAST_SF_DEATH_ANIM);
}

// 0x428558
int critter_body_type(Object* critter)
{
    Proto* proto;
    proto_ptr(critter->pid, &proto);
    return proto->critter.data.bodyType;
}

// 0x42857C
int critter_load_data(CritterProtoData* critterData, const char* path)
{
    DB_FILE* stream;

    stream = db_fopen(path, "rb");
    if (stream == NULL) {
        return -1;
    }

    if (critter_read_data(stream, critterData) == -1) {
        db_fclose(stream);
        return -1;
    }

    db_fclose(stream);
    return 0;
}

// 0x4285C4
int pc_load_data(const char* path)
{
    DB_FILE* stream = db_fopen(path, "rb");
    if (stream == NULL) {
        return -1;
    }

    Proto* proto;
    proto_ptr(obj_dude->pid, &proto);

    if (critter_read_data(stream, &(proto->critter.data)) == -1) {
        db_fclose(stream);
        return -1;
    }

    db_fread(pc_name, DUDE_NAME_MAX_LENGTH, 1, stream);

    if (skill_load(stream) == -1) {
        db_fclose(stream);
        return -1;
    }

    if (trait_load(stream) == -1) {
        db_fclose(stream);
        return -1;
    }

    if (db_freadInt(stream, &character_points) == -1) {
        db_fclose(stream);
        return -1;
    }

    proto->critter.data.baseStats[STAT_DAMAGE_RESISTANCE_EMP] = 100;
    proto->critter.data.bodyType = 0;
    proto->critter.data.experience = 0;
    proto->critter.data.killType = 0;

    db_fclose(stream);
    return 0;
}

// 0x4286DC
int critter_read_data(DB_FILE* stream, CritterProtoData* critterData)
{
    if (db_freadInt(stream, &(critterData->flags)) == -1) return -1;
    if (db_freadIntCount(stream, critterData->baseStats, SAVEABLE_STAT_COUNT) == -1) return -1;
    if (db_freadIntCount(stream, critterData->bonusStats, SAVEABLE_STAT_COUNT) == -1) return -1;
    if (db_freadIntCount(stream, critterData->skills, SKILL_COUNT) == -1) return -1;
    if (db_freadInt(stream, &(critterData->bodyType)) == -1) return -1;
    if (db_freadInt(stream, &(critterData->experience)) == -1) return -1;
    if (db_freadInt(stream, &(critterData->killType)) == -1) return -1;

    return 0;
}

// 0x42878C
int critter_save_data(CritterProtoData* critterData, const char* path)
{
    DB_FILE* stream;

    stream = db_fopen(path, "wb");
    if (stream == NULL) {
        return -1;
    }

    if (critter_write_data(stream, critterData) == -1) {
        db_fclose(stream);
        return -1;
    }

    db_fclose(stream);
    return 0;
}

// 0x4287D4
int pc_save_data(const char* path)
{
    DB_FILE* stream = db_fopen(path, "wb");
    if (stream == NULL) {
        return -1;
    }

    Proto* proto;
    proto_ptr(obj_dude->pid, &proto);

    if (critter_write_data(stream, &(proto->critter.data)) == -1) {
        db_fclose(stream);
        return -1;
    }

    db_fwrite(pc_name, DUDE_NAME_MAX_LENGTH, 1, stream);

    if (skill_save(stream) == -1) {
        db_fclose(stream);
        return -1;
    }

    if (trait_save(stream) == -1) {
        db_fclose(stream);
        return -1;
    }

    if (db_fwriteInt(stream, character_points) == -1) {
        db_fclose(stream);
        return -1;
    }

    db_fclose(stream);
    return 0;
}

// 0x4288BC
int critter_write_data(DB_FILE* stream, CritterProtoData* critterData)
{
    if (db_fwriteInt(stream, critterData->flags) == -1) return -1;
    if (db_fwriteIntCount(stream, critterData->baseStats, SAVEABLE_STAT_COUNT) == -1) return -1;
    if (db_fwriteIntCount(stream, critterData->bonusStats, SAVEABLE_STAT_COUNT) == -1) return -1;
    if (db_fwriteIntCount(stream, critterData->skills, SKILL_COUNT) == -1) return -1;
    if (db_fwriteInt(stream, critterData->bodyType) == -1) return -1;
    if (db_fwriteInt(stream, critterData->experience) == -1) return -1;
    if (db_fwriteInt(stream, critterData->killType) == -1) return -1;

    return 0;
}

// 0x42895C
void pc_flag_off(int pc_flag)
{
    Proto* proto;
    proto_ptr(obj_dude->pid, &proto);

    proto->critter.data.flags &= ~(1 << pc_flag);

    if (pc_flag == PC_FLAG_SNEAKING) {
        queue_remove_this(obj_dude, EVENT_TYPE_SNEAK);
    }

    refresh_box_bar_win();
}

// 0x4289A8
void pc_flag_on(int pc_flag)
{
    Proto* proto;
    proto_ptr(obj_dude->pid, &proto);

    proto->critter.data.flags |= (1 << pc_flag);

    if (pc_flag == PC_FLAG_SNEAKING) {
        critter_sneak_check(NULL, NULL);
    }

    refresh_box_bar_win();
}

// 0x428A1C
void pc_flag_toggle(int pc_flag)
{
    // NOTE: Uninline.
    if (is_pc_flag(pc_flag)) {
        pc_flag_off(pc_flag);
    } else {
        pc_flag_on(pc_flag);
    }
}

// 0x428A64
bool is_pc_flag(int pc_flag)
{
    Proto* proto;
    proto_ptr(obj_dude->pid, &proto);
    return (proto->critter.data.flags & (1 << pc_flag)) != 0;
}

// 0x428A98
int critter_sneak_check(Object* obj, void* data)
{
    sneak_working = skill_result(obj_dude, SKILL_SNEAK, 0, NULL) >= ROLL_SUCCESS;
    queue_add(600, obj_dude, NULL, EVENT_TYPE_SNEAK);
    return 0;
}

// 0x428ADC
int critter_sneak_clear(Object* obj, void* data)
{
    pc_flag_off(PC_FLAG_SNEAKING);
    return 1;
}

// Returns true if dude is really sneaking.
//
// 0x428AEC
bool is_pc_sneak_working()
{
    // NOTE: Uninline.
    if (is_pc_flag(PC_FLAG_SNEAKING)) {
        return sneak_working;
    }

    return false;
}

// 0x428B1C
int critter_wake_up(Object* obj, void* data)
{
    if ((obj->data.critter.combat.results & DAM_DEAD) != 0) {
        return 0;
    }

    obj->data.critter.combat.results &= ~(DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN);
    obj->data.critter.combat.results |= DAM_KNOCKED_DOWN;

    if (isInCombat()) {
        obj->data.critter.combat.maneuver |= CRITTER_MANEUVER_ENGAGING;
    } else {
        dude_standup(obj);
    }

    return 0;
}

// 0x428B58
int critter_wake_clear(Object* obj, void* data)
{
    if ((obj->data.critter.combat.results & DAM_DEAD) != 0) {
        return 0;
    }

    obj->data.critter.combat.results &= ~(DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN);

    int fid = art_id(FID_TYPE(obj->fid), obj->fid & 0xFFF, ANIM_STAND, (obj->fid & 0xF000) >> 12, obj->rotation + 1);
    obj_change_fid(obj, fid, 0);

    return 0;
}

// 0x428BB0
int critter_set_who_hit_me(Object* critter, Object* who_hit_me)
{
    if (who_hit_me != NULL && FID_TYPE(who_hit_me->fid) != OBJ_TYPE_CRITTER) {
        return -1;
    }

    critter->data.critter.combat.whoHitMe = who_hit_me;

    if (who_hit_me == obj_dude) {
        reaction_set(critter, 1);
    }

    return 0;
}

// 0x428C1C
bool critter_can_obj_dude_rest()
{
    int map_idx;
    int check_team;
    int can_rest;
    Object** critters;
    int critters_count;
    int index;
    Object* critter;

    map_idx = map_get_index_number();
    switch (map_idx) {
    case MAP_HUBOLDTN:
    case MAP_HUBHEIGT:
    case MAP_LARIPPER:
    case MAP_LAGUNRUN:
    case MAP_V13ENT:
        check_team = true;
        break;
    case MAP_CHILDRN1:
        check_team = false;
        break;
    default:
        if (map_idx == MAP_HALLDED && map_elevation != 0) {
            check_team = false;
        } else {
            switch (xlate_mapidx_to_town(map_idx)) {
            case TOWN_VAULT_13:
            case TOWN_SHADY_SANDS:
            case TOWN_JUNKTOWN:
            case TOWN_THE_HUB:
            case TOWN_BROTHERHOOD:
            case TOWN_BONEYARD:
                check_team = false;
                break;
            default:
                check_team = true;
                break;
            }
        }
    }

    can_rest = true;

    critters_count = obj_create_list(-1, map_elevation, OBJ_TYPE_CRITTER, &critters);
    for (index = 0; index < critters_count; index++) {
        critter = critters[index];
        if ((critter->data.critter.combat.results & DAM_DEAD) == 0) {
            if (critter != obj_dude) {
                if (critter->data.critter.combat.whoHitMe == obj_dude) {
                    can_rest = false;
                    break;
                }

                if (check_team) {
                    if (critter->data.critter.combat.team != obj_dude->data.critter.combat.team) {
                        can_rest = false;
                        break;
                    }
                }
            }
        }
    }

    if (critters_count != 0) {
        obj_delete_list(critters);
    }

    return can_rest;
}

// 0x428D28
int critter_compute_ap_from_distance(Object* critter, int distance)
{
    int flags = critter->data.critter.combat.results;
    if ((flags & DAM_CRIP_LEG_LEFT) != 0 && (flags & DAM_CRIP_LEG_RIGHT) != 0) {
        return 8 * distance;
    } else if ((flags & DAM_CRIP_LEG_ANY) != 0) {
        return 4 * distance;
    } else {
        return distance;
    }
}

} // namespace fallout
