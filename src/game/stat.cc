#include "game/stat.h"

#include <limits.h>
#include <stdio.h>

#include <algorithm>

#include "game/combat.h"
#include "game/critter.h"
#include "game/display.h"
#include "game/game.h"
#include "game/gsound.h"
#include "game/intface.h"
#include "game/item.h"
#include "game/message.h"
#include "game/object.h"
#include "game/perk.h"
#include "game/proto.h"
#include "game/roll.h"
#include "game/scripts.h"
#include "game/skill.h"
#include "game/tile.h"
#include "game/trait.h"
#include "platform_compat.h"
#include "plib/gnw/input.h"
#include "plib/gnw/memory.h"

namespace fallout {

// Provides metadata about stats.
typedef struct StatDescription {
    char* name;
    char* description;
    int art_num;
    int minimumValue;
    int maximumValue;
    int defaultValue;
} StatDescription;

// 0x507EC8
static StatDescription stat_data[STAT_COUNT] = {
    { NULL, NULL, 0, PRIMARY_STAT_MIN, PRIMARY_STAT_MAX, 5 },
    { NULL, NULL, 1, PRIMARY_STAT_MIN, PRIMARY_STAT_MAX, 5 },
    { NULL, NULL, 2, PRIMARY_STAT_MIN, PRIMARY_STAT_MAX, 5 },
    { NULL, NULL, 3, PRIMARY_STAT_MIN, PRIMARY_STAT_MAX, 5 },
    { NULL, NULL, 4, PRIMARY_STAT_MIN, PRIMARY_STAT_MAX, 5 },
    { NULL, NULL, 5, PRIMARY_STAT_MIN, PRIMARY_STAT_MAX, 5 },
    { NULL, NULL, 6, PRIMARY_STAT_MIN, PRIMARY_STAT_MAX, 5 },
    { NULL, NULL, 10, 0, INT_MAX, 0 },
    { NULL, NULL, 75, 0, INT_MAX, 0 },
    { NULL, NULL, 18, 0, INT_MAX, 0 },
    { NULL, NULL, 31, 0, INT_MAX, 0 },
    { NULL, NULL, 32, 0, INT_MAX, 0 },
    { NULL, NULL, 20, 0, INT_MAX, 0 },
    { NULL, NULL, 24, 0, INT_MAX, 0 },
    { NULL, NULL, 25, 0, INT_MAX, 0 },
    { NULL, NULL, 26, 0, 100, 0 },
    // CE: Fix minimal value (on par with Fallout 2). This allows "Better
    // Criticals" to be less than 0 (with Heavy Handed trait it's -30), so it
    // can affect critical effect calculation in `attack_crit_success`.
    { NULL, NULL, 94, -60, 100, 0 },
    { NULL, NULL, 0, 0, 100, 0 },
    { NULL, NULL, 0, 0, 100, 0 },
    { NULL, NULL, 0, 0, 100, 0 },
    { NULL, NULL, 0, 0, 100, 0 },
    { NULL, NULL, 0, 0, 100, 0 },
    { NULL, NULL, 0, 0, 100, 0 },
    { NULL, NULL, 0, 0, 100, 0 },
    { NULL, NULL, 22, 0, 90, 0 },
    { NULL, NULL, 0, 0, 90, 0 },
    { NULL, NULL, 0, 0, 90, 0 },
    { NULL, NULL, 0, 0, 90, 0 },
    { NULL, NULL, 0, 0, 90, 0 },
    { NULL, NULL, 0, 0, 100, 0 },
    { NULL, NULL, 0, 0, 90, 0 },
    { NULL, NULL, 83, 0, 100, 0 },
    { NULL, NULL, 23, 0, 100, 0 },
    { NULL, NULL, 0, 16, 35, 25 },
    { NULL, NULL, 0, 0, 1, 0 },
    { NULL, NULL, 10, 0, 2000, 0 },
    { NULL, NULL, 11, 0, 2000, 0 },
    { NULL, NULL, 12, 0, 2000, 0 },
};

// 0x508258
static StatDescription pc_stat_data[PC_STAT_COUNT] = {
    { NULL, NULL, 0, 0, INT_MAX, 0 },
    { NULL, NULL, 0, 1, INT_MAX, 1 },
    { NULL, NULL, 0, 0, INT_MAX, 0 },
    { NULL, NULL, 0, -20, 20, 0 },
    { NULL, NULL, 0, 0, INT_MAX, 0 },
};

// 0x6651CC
static MessageList stat_message_file;

// 0x6651D4
static char* level_description[PRIMARY_STAT_RANGE];

// 0x6651FC
static int curr_pc_stat[PC_STAT_COUNT];

// 0x49C2F0
int stat_init()
{
    char path[COMPAT_MAX_PATH];
    MessageListItem messageListItem;
    int index;

    // NOTE: Uninline.
    stat_pc_set_defaults();

    if (!message_init(&stat_message_file)) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s%s", msg_path, "stat.msg");

    if (!message_load(&stat_message_file, path)) {
        return -1;
    }

    for (index = 0; index < STAT_COUNT; index++) {
        stat_data[index].name = getmsg(&stat_message_file, &messageListItem, 100 + index);
        stat_data[index].description = getmsg(&stat_message_file, &messageListItem, 200 + index);
    }

    for (index = 0; index < PC_STAT_COUNT; index++) {
        pc_stat_data[index].name = getmsg(&stat_message_file, &messageListItem, 400 + index);
        pc_stat_data[index].description = getmsg(&stat_message_file, &messageListItem, 500 + index);
    }

    for (index = 0; index < PRIMARY_STAT_RANGE; index++) {
        level_description[index] = getmsg(&stat_message_file, &messageListItem, 301 + index);
    }

    return 0;
}

// 0x49C440
int stat_reset()
{
    // NOTE: Uninline.
    stat_pc_set_defaults();

    return 0;
}

// 0x49C464
int stat_exit()
{
    message_exit(&stat_message_file);

    return 0;
}

// 0x49C474
int stat_load(DB_FILE* stream)
{
    int pc_stat;

    for (pc_stat = 0; pc_stat < PC_STAT_COUNT; pc_stat++) {
        if (db_freadInt(stream, &(curr_pc_stat[pc_stat])) == -1) {
            return -1;
        }
    }

    return 0;
}

// 0x49C4A0
int stat_save(DB_FILE* stream)
{
    int pc_stat;

    for (pc_stat = 0; pc_stat < PC_STAT_COUNT; pc_stat++) {
        if (db_fwriteInt(stream, curr_pc_stat[pc_stat]) == -1) {
            return -1;
        }
    }

    return 0;
}

// 0x49C4C8
int stat_level(Object* critter, int stat)
{
    int value;
    if (stat >= 0 && stat < SAVEABLE_STAT_COUNT) {
        value = stat_get_base(critter, stat);
        value += stat_get_bonus(critter, stat);

        switch (stat) {
        case STAT_PERCEPTION:
            if ((critter->data.critter.combat.results & DAM_BLIND) != 0) {
                value -= 5;
            }
            break;
        case STAT_ARMOR_CLASS:
            if (isInCombat()) {
                if (combat_whose_turn() != critter) {
                    value += critter->data.critter.combat.ap;
                }
            }
            break;
        case STAT_AGE:
            value += game_time() / GAME_TIME_TICKS_PER_YEAR;
            break;
        }

        value = std::clamp(value, stat_data[stat].minimumValue, stat_data[stat].maximumValue);
    } else {
        switch (stat) {
        case STAT_CURRENT_HIT_POINTS:
            value = critter_get_hits(critter);
            break;
        case STAT_CURRENT_POISON_LEVEL:
            value = critter_get_poison(critter);
            break;
        case STAT_CURRENT_RADIATION_LEVEL:
            value = critter_get_rads(critter);
            break;
        default:
            value = 0;
            break;
        }
    }

    return value;
}

// Returns base stat value (accounting for traits if critter is dude).
//
// 0x49C5B8
int stat_get_base(Object* critter, int stat)
{
    int value = stat_get_base_direct(critter, stat);

    if (critter == obj_dude) {
        value += trait_adjust_stat(stat);
    }

    return value;
}

// 0x49C5E0
int stat_get_base_direct(Object* critter, int stat)
{
    Proto* proto;

    if (stat >= 0 && stat < SAVEABLE_STAT_COUNT) {
        proto_ptr(critter->pid, &proto);
        return proto->critter.data.baseStats[stat];
    } else {
        switch (stat) {
        case STAT_CURRENT_HIT_POINTS:
            return critter_get_hits(critter);
        case STAT_CURRENT_POISON_LEVEL:
            return critter_get_poison(critter);
        case STAT_CURRENT_RADIATION_LEVEL:
            return critter_get_rads(critter);
        }
    }

    return 0;
}

// 0x49C64C
int stat_get_bonus(Object* critter, int stat)
{
    if (stat >= 0 && stat < SAVEABLE_STAT_COUNT) {
        Proto* proto;
        proto_ptr(critter->pid, &proto);
        return proto->critter.data.bonusStats[stat];
    }

    return 0;
}

// 0x49C694
int stat_set_base(Object* critter, int stat, int value)
{
    Proto* proto;

    if (stat < 0 || stat >= STAT_COUNT) {
        return -5;
    }

    if (stat >= 0 && stat < SAVEABLE_STAT_COUNT) {
        if (stat > STAT_LUCK && stat <= STAT_POISON_RESISTANCE) {
            // Cannot change base value of derived stats.
            return -1;
        }

        if (critter == obj_dude) {
            value -= trait_adjust_stat(stat);
        }

        if (value < stat_data[stat].minimumValue) {
            return -2;
        }

        if (value > stat_data[stat].maximumValue) {
            return -3;
        }

        proto_ptr(critter->pid, &proto);
        proto->critter.data.baseStats[stat] = value;

        if (stat >= STAT_STRENGTH && stat <= STAT_LUCK) {
            stat_recalc_derived(critter);
        }

        return 0;
    }

    switch (stat) {
    case STAT_CURRENT_HIT_POINTS:
        return critter_adjust_hits(critter, value - critter_get_hits(critter));
    case STAT_CURRENT_POISON_LEVEL:
        return critter_adjust_poison(critter, value - critter_get_poison(critter));
    case STAT_CURRENT_RADIATION_LEVEL:
        return critter_adjust_rads(critter, value - critter_get_rads(critter));
    }

    // Should be unreachable
    return 0;
}

// 0x49C7AC
int inc_stat(Object* critter, int stat)
{
    int value = stat_get_base_direct(critter, stat);

    if (critter == obj_dude) {
        value += trait_adjust_stat(stat);
    }

    return stat_set_base(critter, stat, value + 1);
}

// 0x49C7E0
int dec_stat(Object* critter, int stat)
{
    int value = stat_get_base_direct(critter, stat);

    if (critter == obj_dude) {
        value += trait_adjust_stat(stat);
    }

    return stat_set_base(critter, stat, value - 1);
}

// 0x49C814
int stat_set_bonus(Object* critter, int stat, int value)
{
    if (stat < 0 || stat >= STAT_COUNT) {
        return -5;
    }

    if (stat >= 0 && stat < SAVEABLE_STAT_COUNT) {
        Proto* proto;
        proto_ptr(critter->pid, &proto);
        proto->critter.data.bonusStats[stat] = value;

        if (stat >= STAT_STRENGTH && stat <= STAT_LUCK) {
            stat_recalc_derived(critter);
        }

        return 0;
    } else {
        switch (stat) {
        case STAT_CURRENT_HIT_POINTS:
            return critter_adjust_hits(critter, value);
        case STAT_CURRENT_POISON_LEVEL:
            return critter_adjust_poison(critter, value);
        case STAT_CURRENT_RADIATION_LEVEL:
            return critter_adjust_rads(critter, value);
        }
    }

    // Should be unreachable
    return -1;
}

// 0x49C8A4
void stat_set_defaults(CritterProtoData* data)
{
    int stat;

    for (stat = 0; stat < SAVEABLE_STAT_COUNT; stat++) {
        data->baseStats[stat] = stat_data[stat].defaultValue;
        data->bonusStats[stat] = 0;
    }
}

// 0x49C8D4
void stat_recalc_derived(Object* critter)
{
    int strength = stat_level(critter, STAT_STRENGTH);
    int perception = stat_level(critter, STAT_PERCEPTION);
    int endurance = stat_level(critter, STAT_ENDURANCE);
    int intelligence = stat_level(critter, STAT_INTELLIGENCE);
    int agility = stat_level(critter, STAT_AGILITY);
    int luck = stat_level(critter, STAT_LUCK);

    Proto* proto;
    proto_ptr(critter->pid, &proto);
    CritterProtoData* data = &(proto->critter.data);

    data->baseStats[STAT_MAXIMUM_HIT_POINTS] = stat_get_base(critter, STAT_STRENGTH) + stat_get_base(critter, STAT_ENDURANCE) * 2 + 15;
    data->baseStats[STAT_MAXIMUM_ACTION_POINTS] = agility / 2 + 5;
    data->baseStats[STAT_ARMOR_CLASS] = agility;
    data->baseStats[STAT_MELEE_DAMAGE] = std::max(strength - 5, 1);
    data->baseStats[STAT_CARRY_WEIGHT] = 25 * strength + 25;
    data->baseStats[STAT_SEQUENCE] = 2 * perception;
    data->baseStats[STAT_HEALING_RATE] = std::max(endurance / 3, 1);
    data->baseStats[STAT_CRITICAL_CHANCE] = luck;
    data->baseStats[STAT_BETTER_CRITICALS] = 0;
    data->baseStats[STAT_RADIATION_RESISTANCE] = 2 * endurance;
    data->baseStats[STAT_POISON_RESISTANCE] = 5 * endurance;
}

// 0x49CA2C
char* stat_name(int stat)
{
    return stat >= 0 && stat < STAT_COUNT ? stat_data[stat].name : NULL;
}

// 0x49CA70
char* stat_description(int stat)
{
    return stat >= 0 && stat < STAT_COUNT ? stat_data[stat].description : NULL;
}

// 0x49CAB4
char* stat_level_description(int value)
{
    if (value < PRIMARY_STAT_MIN) {
        value = PRIMARY_STAT_MIN;
    } else if (value > PRIMARY_STAT_MAX) {
        value = PRIMARY_STAT_MAX;
    }

    return level_description[value - PRIMARY_STAT_MIN];
}

// 0x49CAD4
int stat_pc_get(int pc_stat)
{
    return pc_stat >= 0 && pc_stat < PC_STAT_COUNT ? curr_pc_stat[pc_stat] : 0;
}

// 0x49CAE8
int stat_pc_set(int pc_stat, int value)
{
    int rc;

    if (pc_stat < 0 && pc_stat >= PC_STAT_COUNT) {
        return -5;
    }

    if (value < pc_stat_data[pc_stat].minimumValue) {
        return -2;
    }

    if (value > pc_stat_data[pc_stat].maximumValue) {
        return -3;
    }

    curr_pc_stat[pc_stat] = value;

    if (pc_stat == PC_STAT_EXPERIENCE) {
        rc = stat_pc_add_experience(0);
    } else {
        rc = 0;
    }

    return rc;
}

// 0x49CB3C
void stat_pc_set_defaults()
{
    int pc_stat;

    for (pc_stat = 0; pc_stat < PC_STAT_COUNT; pc_stat++) {
        curr_pc_stat[pc_stat] = pc_stat_data[pc_stat].defaultValue;
    }
}

// Returns experience to reach next level.
//
// 0x49CB5C
int stat_pc_min_exp()
{
    // 0x5082D0
    static int exp[PC_LEVEL_MAX] = {
        0,
        1,
        3,
        6,
        10,
        15,
        21,
        28,
        36,
        45,
        55,
        66,
        78,
        91,
        105,
        120,
        136,
        153,
        171,
        190,
        210,
    };

    int level = stat_pc_get(PC_STAT_LEVEL);
    return level < PC_LEVEL_MAX ? 1000 * exp[level] : -1;
}

// 0x49CB88
char* stat_pc_name(int pcStat)
{
    return pcStat >= 0 && pcStat < PC_STAT_COUNT ? pc_stat_data[pcStat].name : NULL;
}

// 0x49CBA8
char* stat_pc_description(int pcStat)
{
    return pcStat >= 0 && pcStat < PC_STAT_COUNT ? pc_stat_data[pcStat].description : NULL;
}

// 0x49CBC8
int stat_picture(int stat)
{
    return stat >= 0 && stat < STAT_COUNT ? stat_data[stat].art_num : 0;
}

// Roll D10 against specified stat.
//
// This function is intended to be used with one of SPECIAL stats (which are
// capped at 10, hence d10), not with artitrary stat, but does not enforce it.
//
// An optional [modifier] can be supplied as a bonus (or penalty) to the stat's
// value.
//
// Upon return [howMuch] will be set to difference between stat's value
// (accounting for given [modifier]) and d10 roll, which can be positive (or
// zero) when roll succeeds, or negative when roll fails. Set [howMuch] to
// `NULL` if you're not interested in this value.
//
// 0x49CC0C
int stat_result(Object* critter, int stat, int modifier, int* howMuch)
{
    int value = stat_level(critter, stat) + modifier;
    int chance = roll_random(PRIMARY_STAT_MIN, PRIMARY_STAT_MAX);

    if (howMuch != NULL) {
        *howMuch = value - chance;
    }

    if (chance <= value) {
        return ROLL_SUCCESS;
    }

    return ROLL_FAILURE;
}

// 0x49CC3C
int stat_pc_add_experience(int xp)
{
    xp += perk_level(PERK_SWIFT_LEARNER) * 5 * xp / 100;
    xp += stat_pc_get(PC_STAT_EXPERIENCE);

    if (xp < pc_stat_data[PC_STAT_EXPERIENCE].minimumValue) {
        xp = pc_stat_data[PC_STAT_EXPERIENCE].minimumValue;
    }

    if (xp > pc_stat_data[PC_STAT_EXPERIENCE].maximumValue) {
        xp = pc_stat_data[PC_STAT_EXPERIENCE].maximumValue;
    }

    curr_pc_stat[PC_STAT_EXPERIENCE] = xp;

    while (stat_pc_get(PC_STAT_LEVEL) < PC_LEVEL_MAX && xp >= stat_pc_min_exp()) {
        if (stat_pc_set(PC_STAT_LEVEL, stat_pc_get(PC_STAT_LEVEL) + 1) == 0) {
            MessageListItem messageListItem;
            int hp;

            // You have gone up a level.
            messageListItem.num = 600;
            if (message_search(&stat_message_file, &messageListItem)) {
                display_print(messageListItem.text);
            }

            pc_flag_on(PC_FLAG_LEVEL_UP_AVAILABLE);

            gsound_play_sfx_file("levelup");

            // NOTE: Uninline.
            hp = stat_get_base(obj_dude, STAT_ENDURANCE) / 2 + 2;
            hp += perk_level(PERK_LIFEGIVER) * 4;
            critter_adjust_hits(obj_dude, hp);

            stat_set_bonus(obj_dude, STAT_MAXIMUM_HIT_POINTS, stat_get_bonus(obj_dude, STAT_MAXIMUM_HIT_POINTS) + hp);

            intface_update_hit_points(false);
        }
    }

    return 0;
}

} // namespace fallout
