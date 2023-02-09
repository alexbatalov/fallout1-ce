#include "game/perk.h"

#include <stdio.h>

#include "game/game.h"
#include "game/gconfig.h"
#include "game/message.h"
#include "game/object.h"
#include "game/party.h"
#include "game/skill.h"
#include "game/stat.h"
#include "platform_compat.h"
#include "plib/gnw/debug.h"
#include "plib/gnw/memory.h"

namespace fallout {

typedef struct PerkDescription {
    char* name;
    char* description;
    int max_rank;
    int min_level;
    int stat;
    int stat_modifier;
    int required_skill;
    int required_skill_level;
    int required_stat_levels[PRIMARY_STAT_COUNT];
} PerkDescription;

static bool perk_can_add(int perk);
static void perk_defaults();

// 0x506324
static PerkDescription perk_data[PERK_COUNT] = {
    { NULL, NULL, 1, 3, -1, 0, -1, 0, { 0, 5, 0, 0, 0, 0, 0 } },
    { NULL, NULL, 1, 6, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 6, 0 } },
    { NULL, NULL, 3, 3, 11, 2, -1, 0, { 6, 0, 0, 0, 0, 6, 0 } },
    { NULL, NULL, 3, 6, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 5, 0 } },
    { NULL, NULL, 2, 6, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 6, 6 } },
    { NULL, NULL, 1, 9, -1, 0, -1, 0, { 0, 6, 0, 0, 6, 7, 0 } },
    { NULL, NULL, 3, 3, 13, 2, -1, 0, { 0, 6, 0, 0, 0, 0, 0 } },
    { NULL, NULL, 3, 3, 14, 1, -1, 0, { 0, 0, 6, 0, 0, 0, 0 } },
    { NULL, NULL, 3, 6, 15, 5, -1, 0, { 0, 0, 0, 0, 0, 0, 6 } },
    { NULL, NULL, 3, 3, -1, 0, -1, 0, { 0, 6, 0, 0, 0, 0, 0 } },
    { NULL, NULL, 3, 3, -1, 0, -1, 0, { 0, 0, 0, 6, 0, 0, 0 } },
    { NULL, NULL, 3, 6, 31, 15, -1, 0, { 0, 0, 6, 0, 4, 0, 0 } },
    { NULL, NULL, 3, 3, 24, 10, -1, 0, { 0, 0, 6, 0, 0, 0, 6 } },
    { NULL, NULL, 3, 3, 12, 50, -1, 0, { 6, 0, 6, 0, 0, 0, 0 } },
    { NULL, NULL, 2, 6, -1, 0, -1, 0, { 0, 7, 0, 0, 6, 0, 0 } },
    { NULL, NULL, 1, 6, -1, 0, 8, 50, { 0, 0, 0, 0, 0, 6, 0 } },
    { NULL, NULL, 3, 3, -1, 0, 17, 40, { 0, 0, 6, 0, 6, 0, 0 } },
    { NULL, NULL, 1, 9, -1, 0, 15, 60, { 0, 0, 0, 7, 0, 0, 0 } },
    { NULL, NULL, 3, 6, -1, 0, -1, 0, { 0, 0, 0, 0, 6, 0, 0 } },
    { NULL, NULL, 3, 3, -1, 0, 6, 40, { 0, 7, 0, 0, 5, 6, 0 } },
    { NULL, NULL, 1, 6, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 8 } },
    { NULL, NULL, 1, 9, 16, 20, -1, 0, { 0, 6, 0, 0, 0, 4, 6 } },
    { NULL, NULL, 1, 6, -1, 0, -1, 0, { 0, 7, 0, 0, 5, 0, 0 } },
    { NULL, NULL, 1, 18, -1, 0, 3, 80, { 8, 0, 0, 0, 0, 8, 0 } },
    { NULL, NULL, 1, 18, -1, 0, 0, 80, { 0, 8, 0, 0, 0, 8, 0 } },
    { NULL, NULL, 1, 18, -1, 0, 8, 80, { 0, 0, 0, 0, 0, 10, 0 } },
    { NULL, NULL, 3, 12, 8, 1, -1, 0, { 0, 0, 0, 0, 0, 5, 0 } },
    { NULL, NULL, 1, 15, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, 3, 12, -1, 0, -1, 0, { 0, 0, 4, 0, 0, 0, 0 } },
    { NULL, NULL, 2, 9, 9, 5, -1, 0, { 0, 0, 0, 0, 0, 4, 0 } },
    { NULL, NULL, 1, 6, 32, 25, -1, 0, { 0, 0, 3, 0, 0, 0, 0 } },
    { NULL, NULL, 1, 12, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, 1, 12, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, 1, 12, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, 1, 12, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, 3, 6, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, 1, 6, -1, 0, -1, 0, { 0, 4, 0, 0, 0, 0, 0 } },
    { NULL, NULL, 1, 9, -1, 0, 8, 80, { 0, 0, 0, 0, 0, 8, 0 } },
    { NULL, NULL, 1, 6, -1, 0, 8, 60, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, 1, 12, -1, 0, -1, 0, { 0, 0, 0, 10, 0, 0, 0 } },
    { NULL, NULL, 1, 9, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 8 } },
    { NULL, NULL, 1, 9, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, 1, 9, -1, 0, -1, 0, { 0, 0, 5, 0, 0, 0, 0 } },
    { NULL, NULL, 2, 6, -1, 0, 17, 40, { 0, 0, 6, 0, 0, 0, 0 } },
    { NULL, NULL, 1, 9, -1, 0, 17, 25, { 0, 0, 0, 0, 5, 0, 0 } },
    { NULL, NULL, 1, 3, -1, 0, -1, 0, { 0, 8, 0, 0, 0, 0, 0 } },
    { NULL, NULL, 1, 6, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 7 } },
    { NULL, NULL, 3, 6, -1, 0, -1, 0, { 0, 6, 0, 0, 0, 0, 0 } },
    { NULL, NULL, 3, 3, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 5, 0 } },
    { NULL, NULL, 3, 3, -1, 0, -1, 0, { 0, 0, 0, 0, 4, 0, 0 } },
    { NULL, NULL, 3, 3, -1, 0, -1, 0, { 0, 0, 0, 0, 4, 0, 0 } },
    { NULL, NULL, 1, 12, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, 1, 9, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, -1, 1, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, -1, 1, -1, 0, -1, 0, { -2, 0, -2, 0, 0, -3, 0 } },
    { NULL, NULL, -1, 1, -1, 0, -1, 0, { 0, 0, 0, 0, -3, -2, 0 } },
    { NULL, NULL, -1, 1, -1, 0, -1, 0, { 0, 0, 0, 0, -2, 0, 0 } },
    { NULL, NULL, -1, 1, 31, -20, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, -1, 1, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, -1, 1, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, -1, 1, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, -1, 1, -1, 0, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, -1, 1, 31, 30, -1, 0, { 3, 0, 0, 0, 0, 0, 0 } },
    { NULL, NULL, -1, 1, 31, 20, -1, 0, { 0, 0, 0, 0, 0, 0, 0 } },

};

// 0x662964
static int perk_lev[PERK_COUNT];

// perk.msg
//
// 0x662A64
static MessageList perk_message_file;

// 0x486550
int perk_init()
{
    char path[COMPAT_MAX_PATH];
    int perk;
    MessageListItem messageListItem;

    perk_defaults();

    if (!message_init(&perk_message_file)) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s%s", msg_path, "perk.msg");

    if (!message_load(&perk_message_file, path)) {
        return -1;
    }

    for (perk = 0; perk < PERK_COUNT; perk++) {
        messageListItem.num = 101 + perk;
        if (message_search(&perk_message_file, &messageListItem)) {
            perk_data[perk].name = messageListItem.text;
        }

        messageListItem.num = 201 + perk;
        if (message_search(&perk_message_file, &messageListItem)) {
            perk_data[perk].description = messageListItem.text;
        }
    }

    return 0;
}

// 0x48663C
int perk_reset()
{
    perk_defaults();
    return 0;
}

// 0x486658
int perk_exit()
{
    message_exit(&perk_message_file);
    return 0;
}

// 0x486668
int perk_load(DB_FILE* stream)
{
    int perk;

    for (perk = 0; perk < PERK_COUNT; perk++) {
        if (db_freadInt(stream, &(perk_lev[perk])) == -1) {
            return -1;
        }
    }

    return 0;
}

// 0x486694
int perk_save(DB_FILE* stream)
{
    int perk;

    for (perk = 0; perk < PERK_COUNT; perk++) {
        if (db_fwriteInt(stream, perk_lev[perk]) == -1) {
            return -1;
        }
    }

    return 0;
}

// 0x4866C0
static bool perk_can_add(int perk)
{
    PerkDescription* perkDescription;
    int stat;

    if (perk < 0 || perk >= PERK_COUNT) {
        return false;
    }

    perkDescription = &(perk_data[perk]);

    if (perkDescription->max_rank == -1) {
        return false;
    }

    if (perk_lev[perk] >= perkDescription->max_rank) {
        return false;
    }

    if (stat_pc_get(PC_STAT_LEVEL) < perkDescription->min_level) {
        return false;
    }

    if (perkDescription->required_skill != -1) {
        if (skill_level(obj_dude, perkDescription->required_skill) < perkDescription->required_skill_level) {
            return false;
        }
    }

    for (stat = 0; stat < PRIMARY_STAT_COUNT; stat++) {
        if (stat_level(obj_dude, stat) < perkDescription->required_stat_levels[stat]) {
            return false;
        }
    }

    return true;
}

// Resets party member perks.
//
// 0x486778
static void perk_defaults()
{
    int perk;

    for (perk = 0; perk < PERK_COUNT; perk++) {
        perk_lev[perk] = 0;
    }
}

// 0x486790
int perk_add(int perk)
{
    if (perk < 0 || perk >= PERK_COUNT) {
        return -1;
    }

    if (!perk_can_add(perk)) {
        return -1;
    }

    perk_lev[perk] += 1;

    perk_add_effect(obj_dude, perk);

    return 0;
}

// 0x4867D4
int perk_sub(int perk)
{
    if (perk < 0 || perk >= PERK_COUNT) {
        return -1;
    }

    if (perk_lev[perk] < 1) {
        return -1;
    }

    perk_lev[perk] -= 1;

    perk_remove_effect(obj_dude, perk);

    return 0;
}

// Returns perks available to pick.
//
// 0x486824
int perk_make_list(int* perks)
{
    int perk;
    int count = 0;

    for (perk = 0; perk < PERK_COUNT; perk++) {
        if (perk_can_add(perk)) {
            perks[count] = perk;
            count++;
        }
    }
    return count;
}

// 0x486854
int perk_level(int perk)
{
    return perk >= 0 && perk < PERK_COUNT ? perk_lev[perk] : 0;
}

// 0x486868
char* perk_name(int perk)
{
    return perk >= 0 && perk < PERK_COUNT ? perk_data[perk].name : NULL;
}

// 0x486888
char* perk_description(int perk)
{
    return perk >= 0 && perk < PERK_COUNT ? perk_data[perk].description : NULL;
}

// 0x4868A8
void perk_add_effect(Object* critter, int perk)
{
    PerkDescription* perkDescription;
    int value;
    int stat;

    if (perk < 0 || perk >= PERK_COUNT) {
        return;
    }

    perkDescription = &(perk_data[perk]);

    if (perkDescription->stat != -1) {
        value = stat_get_bonus(critter, perkDescription->stat);
        stat_set_bonus(critter, perkDescription->stat, value + perkDescription->stat_modifier);
    }

    if (perkDescription->max_rank == -1) {
        for (stat = 0; stat < PRIMARY_STAT_COUNT; stat++) {
            value = stat_get_bonus(critter, stat);
            stat_set_bonus(critter, stat, value + perkDescription->required_stat_levels[stat]);
        }
    }
}

// 0x4960x486914CE0
void perk_remove_effect(Object* critter, int perk)
{
    PerkDescription* perkDescription;
    int value;
    int stat;

    if (perk < 0 || perk >= PERK_COUNT) {
        return;
    }

    perkDescription = &(perk_data[perk]);

    if (perkDescription->stat != -1) {
        value = stat_get_bonus(critter, perkDescription->stat);
        stat_set_bonus(critter, perkDescription->stat, value - perkDescription->stat_modifier);
    }

    if (perkDescription->max_rank == -1) {
        for (stat = 0; stat < PRIMARY_STAT_COUNT; stat++) {
            value = stat_get_bonus(critter, stat);
            stat_set_bonus(critter, stat, value - perkDescription->required_stat_levels[stat]);
        }
    }
}

// Returns modifier to specified skill accounting for perks.
//
// 0x4869AC
int perk_adjust_skill(int skill)
{
    int modifier = 0;

    switch (skill) {
    case SKILL_FIRST_AID:
    case SKILL_DOCTOR:
        if (perk_level(PERK_MEDIC)) {
            modifier += 20;
        }
        break;
    case SKILL_SNEAK:
        if (perk_level(PERK_GHOST)) {
            if (obj_get_visible_light(obj_dude) <= 45875) {
                modifier += 20;
            }
        }
        // FALLTHROUGH
    case SKILL_LOCKPICK:
    case SKILL_STEAL:
    case SKILL_TRAPS:
        if (perk_level(PERK_MASTER_THIEF)) {
            modifier += 10;
        }
        break;
    case SKILL_SCIENCE:
    case SKILL_REPAIR:
        if (perk_level(PERK_MR_FIXIT)) {
            modifier += 20;
        }

        break;
    case SKILL_SPEECH:
    case SKILL_BARTER:
        if (perk_level(PERK_SPEAKER)) {
            modifier += 20;
        }
        break;
    }

    return modifier;
}

} // namespace fallout
