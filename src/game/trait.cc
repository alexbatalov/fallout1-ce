#include "game/trait.h"

#include <stdio.h>

#include "game/game.h"
#include "game/message.h"
#include "game/object.h"
#include "game/scripts.h"
#include "game/skill.h"
#include "game/stat.h"
#include "platform_compat.h"

namespace fallout {

// Provides metadata about traits.
typedef struct TraitDescription {
    // The name of trait.
    char* name;

    // The description of trait.
    //
    // The description is only used in character editor to inform player about
    // effects of this trait.
    char* description;

    // Identifier of art in `intrface.lst`.
    int art_num;
} TraitDescription;

// 0x508564
static TraitDescription trait_data[TRAIT_COUNT] = {
    { NULL, NULL, 55 },
    { NULL, NULL, 56 },
    { NULL, NULL, 57 },
    { NULL, NULL, 58 },
    { NULL, NULL, 59 },
    { NULL, NULL, 60 },
    { NULL, NULL, 61 },
    { NULL, NULL, 62 },
    { NULL, NULL, 63 },
    { NULL, NULL, 64 },
    { NULL, NULL, 65 },
    { NULL, NULL, 66 },
    { NULL, NULL, 67 },
    { NULL, NULL, 68 },
    { NULL, NULL, 69 },
    { NULL, NULL, 70 },
};

// 0x668E58
static MessageList trait_message_file;

// List of selected traits.
//
// 0x668E60
static int pc_trait[PC_TRAIT_MAX];

// 0x4A0490
int trait_init()
{
    char path[COMPAT_MAX_PATH];
    int trait;
    MessageListItem messageListItem;

    if (!message_init(&trait_message_file)) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s%s", msg_path, "trait.msg");

    if (!message_load(&trait_message_file, path)) {
        return -1;
    }

    for (trait = 0; trait < TRAIT_COUNT; trait++) {
        messageListItem.num = 100 + trait;
        if (message_search(&trait_message_file, &messageListItem)) {
            trait_data[trait].name = messageListItem.text;
        }

        messageListItem.num = 200 + trait;
        if (message_search(&trait_message_file, &messageListItem)) {
            trait_data[trait].description = messageListItem.text;
        }
    }

    // NOTE: Uninline.
    trait_reset();

    return true;
}

// 0x4A057C
void trait_reset()
{
    int index;

    for (index = 0; index < PC_TRAIT_MAX; index++) {
        pc_trait[index] = -1;
    }
}

// 0x4A0598
void trait_exit()
{
    message_exit(&trait_message_file);
}

// Loads trait system state from save game.
//
// 0x4A05A8
int trait_load(DB_FILE* stream)
{
    return db_freadIntCount(stream, pc_trait, PC_TRAIT_MAX);
}

// Saves trait system state to save game.
//
// 0x4A05C8
int trait_save(DB_FILE* stream)
{
    return db_fwriteIntCount(stream, pc_trait, PC_TRAIT_MAX);
}

// Sets selected traits.
//
// 0x4A05E8
void trait_set(int trait1, int trait2)
{
    pc_trait[0] = trait1;
    pc_trait[1] = trait2;
}

// Returns selected traits.
//
// 0x4A05F4
void trait_get(int* trait1, int* trait2)
{
    *trait1 = pc_trait[0];
    *trait2 = pc_trait[1];
}

// Returns a name of the specified trait, or `NULL` if the specified trait is
// out of range.
//
// 0x4A0608
char* trait_name(int trait)
{
    return trait >= 0 && trait < TRAIT_COUNT ? trait_data[trait].name : NULL;
}

// Returns a description of the specified trait, or `NULL` if the specified
// trait is out of range.
//
// 0x4A0628
char* trait_description(int trait)
{
    return trait >= 0 && trait < TRAIT_COUNT ? trait_data[trait].description : NULL;
}

// Return an art ID of the specified trait, or `0` if the specified trait is
// out of range.
//
// 0x4A0648
int trait_pic(int trait)
{
    return trait >= 0 && trait < TRAIT_COUNT ? trait_data[trait].art_num : 0;
}

// Returns `true` if the specified trait is selected.
//
// 0x4A0668
int trait_level(int trait)
{
    int index;

    for (index = 0; index < PC_TRAIT_MAX; index++) {
        if (pc_trait[index] == trait) {
            return 1;
        }
    }

    return 0;
}

// Returns stat modifier depending on selected traits.
//
// 0x4A071C
int trait_adjust_stat(int stat)
{
    int modifier = 0;

    switch (stat) {
    case STAT_STRENGTH:
        if (trait_level(TRAIT_GIFTED)) {
            modifier += 1;
        }
        if (trait_level(TRAIT_BRUISER)) {
            modifier += 2;
        }
        break;
    case STAT_PERCEPTION:
        if (trait_level(TRAIT_GIFTED)) {
            modifier += 1;
        }
        if (trait_level(TRAIT_NIGHT_PERSON)) {
            if (game_time_hour() - 600 < 1200) {
                modifier -= 1;
            } else {
                modifier += 1;
            }
        }
        break;
    case STAT_ENDURANCE:
        if (trait_level(TRAIT_GIFTED)) {
            modifier += 1;
        }
        break;
    case STAT_CHARISMA:
        if (trait_level(TRAIT_GIFTED)) {
            modifier += 1;
        }
        break;
    case STAT_INTELLIGENCE:
        if (trait_level(TRAIT_GIFTED)) {
            modifier += 1;
        }
        if (trait_level(TRAIT_NIGHT_PERSON)) {
            if (game_time_hour() - 600 < 1200) {
                modifier -= 1;
            } else {
                modifier += 1;
            }
        }
        break;
    case STAT_AGILITY:
        if (trait_level(TRAIT_GIFTED)) {
            modifier += 1;
        }
        if (trait_level(TRAIT_SMALL_FRAME)) {
            modifier += 1;
        }
        break;
    case STAT_LUCK:
        if (trait_level(TRAIT_GIFTED)) {
            modifier += 1;
        }
        break;
    case STAT_MAXIMUM_ACTION_POINTS:
        if (trait_level(TRAIT_BRUISER)) {
            modifier -= 2;
        }
        break;
    case STAT_ARMOR_CLASS:
        if (trait_level(TRAIT_KAMIKAZE)) {
            modifier -= stat_get_base_direct(obj_dude, STAT_ARMOR_CLASS);
        }
        break;
    case STAT_MELEE_DAMAGE:
        if (trait_level(TRAIT_HEAVY_HANDED)) {
            modifier += 4;
        }
        break;
    case STAT_CARRY_WEIGHT:
        if (trait_level(TRAIT_SMALL_FRAME)) {
            modifier -= 10 * stat_get_base_direct(obj_dude, STAT_STRENGTH);
        }
        break;
    case STAT_SEQUENCE:
        if (trait_level(TRAIT_KAMIKAZE)) {
            modifier += 5;
        }
        break;
    case STAT_HEALING_RATE:
        if (trait_level(TRAIT_FAST_METABOLISM)) {
            modifier += 2;
        }
        break;
    case STAT_CRITICAL_CHANCE:
        if (trait_level(TRAIT_FINESSE)) {
            modifier += 10;
        }
        break;
    case STAT_BETTER_CRITICALS:
        if (trait_level(TRAIT_HEAVY_HANDED)) {
            modifier -= 30;
        }
        break;
    case STAT_RADIATION_RESISTANCE:
        if (trait_level(TRAIT_FAST_METABOLISM)) {
            modifier -= stat_get_base_direct(obj_dude, STAT_RADIATION_RESISTANCE);
        }
        break;
    case STAT_POISON_RESISTANCE:
        if (trait_level(TRAIT_FAST_METABOLISM)) {
            modifier -= stat_get_base_direct(obj_dude, STAT_POISON_RESISTANCE);
        }
        break;
    }

    return modifier;
}

// Returns skill modifier depending on selected traits.
//
// 0x4A0C24
int trait_adjust_skill(int skill)
{
    int modifier = 0;

    if (trait_level(TRAIT_GIFTED)) {
        modifier -= 10;
    }

    if (trait_level(TRAIT_SKILLED)) {
        modifier += 10;
    }

    if (trait_level(TRAIT_GOOD_NATURED)) {
        switch (skill) {
        case SKILL_SMALL_GUNS:
        case SKILL_BIG_GUNS:
        case SKILL_ENERGY_WEAPONS:
        case SKILL_UNARMED:
        case SKILL_MELEE_WEAPONS:
        case SKILL_THROWING:
            modifier -= 10;
            break;
        case SKILL_FIRST_AID:
        case SKILL_DOCTOR:
        case SKILL_SPEECH:
        case SKILL_BARTER:
            modifier += 15;
            break;
        }
    }

    return modifier;
}

} // namespace fallout
