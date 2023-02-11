#include "game/skill.h"

#include <stdio.h>
#include <string.h>

#include "game/actions.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/display.h"
#include "game/game.h"
#include "game/gconfig.h"
#include "game/intface.h"
#include "game/item.h"
#include "game/message.h"
#include "game/object.h"
#include "game/palette.h"
#include "game/party.h"
#include "game/perk.h"
#include "game/pipboy.h"
#include "game/proto.h"
#include "game/roll.h"
#include "game/scripts.h"
#include "game/stat.h"
#include "game/trait.h"
#include "platform_compat.h"
#include "plib/color/color.h"
#include "plib/gnw/debug.h"

namespace fallout {

#define SKILL_LEVEL_MAX 200

#define SKILLS_MAX_USES_PER_DAY 3

#define HEALABLE_DAMAGE_FLAGS_LENGTH 5

static void show_skill_use_messages(Object* obj, int skill, Object* a3, int a4, int a5);
static int skill_game_difficulty(int skill);
static int skill_use_slot_available(int skill);
static int skill_use_slot_add(int skill);
static int skill_use_slot_clear();

typedef struct SkillDescription {
    char* name;
    char* description;
    char* attributes;
    int art_num;
    int default_value;
    int stat_modifier;
    int stat1;
    int stat2;
    int points_modifier;
    int experience;
    int field_28;
} SkillDescription;

// 0x507AA4
static SkillDescription skill_data[SKILL_COUNT] = {
    { NULL, NULL, NULL, 28, 35, 1, STAT_AGILITY, STAT_INVALID, 1, 0, 0 },
    { NULL, NULL, NULL, 29, 10, 1, STAT_AGILITY, STAT_INVALID, 1, 0, 0 },
    { NULL, NULL, NULL, 30, 10, 1, STAT_AGILITY, STAT_INVALID, 1, 0, 0 },
    { NULL, NULL, NULL, 31, 65, 1, STAT_AGILITY, STAT_STRENGTH, 1, 0, 0 },
    { NULL, NULL, NULL, 32, 55, 1, STAT_AGILITY, STAT_STRENGTH, 1, 0, 0 },
    { NULL, NULL, NULL, 33, 40, 1, STAT_AGILITY, STAT_INVALID, 1, 0, 0 },
    { NULL, NULL, NULL, 34, 30, 1, STAT_PERCEPTION, STAT_INTELLIGENCE, 1, 25, 0 },
    { NULL, NULL, NULL, 35, 15, 1, STAT_PERCEPTION, STAT_INTELLIGENCE, 1, 50, 0 },
    { NULL, NULL, NULL, 36, 25, 1, STAT_AGILITY, STAT_INVALID, 1, 0, 0 },
    { NULL, NULL, NULL, 37, 20, 1, STAT_PERCEPTION, STAT_AGILITY, 1, 25, 1 },
    { NULL, NULL, NULL, 38, 20, 1, STAT_AGILITY, STAT_INVALID, 1, 25, 1 },
    { NULL, NULL, NULL, 39, 20, 1, STAT_PERCEPTION, STAT_AGILITY, 1, 25, 1 },
    { NULL, NULL, NULL, 40, 25, 2, STAT_INTELLIGENCE, STAT_INVALID, 1, 0, 0 },
    { NULL, NULL, NULL, 41, 20, 1, STAT_INTELLIGENCE, STAT_INVALID, 1, 0, 0 },
    { NULL, NULL, NULL, 42, 25, 2, STAT_CHARISMA, STAT_INVALID, 1, 0, 0 },
    { NULL, NULL, NULL, 43, 20, 2, STAT_CHARISMA, STAT_INVALID, 1, 0, 0 },
    { NULL, NULL, NULL, 44, 20, 3, STAT_LUCK, STAT_INVALID, 1, 0, 0 },
    { NULL, NULL, NULL, 45, 5, 1, STAT_ENDURANCE, STAT_INTELLIGENCE, 1, 100, 0 },
};

// 0x507DBC
int gIsSteal = 0;

// 0x507DC0
int gStealCount = 0;

// 0x507DC4
int gStealSize = 0;

// 0x665008
static int timesSkillUsed[SKILL_COUNT][SKILLS_MAX_USES_PER_DAY];

// 0x664FF0
static int tag_skill[NUM_TAGGED_SKILLS];

// skill.msg
//
// 0x665000
static MessageList skill_message_file;

// 0x498174
int skill_init()
{
    char path[COMPAT_MAX_PATH];
    int index;
    MessageListItem mesg;

    if (!message_init(&skill_message_file)) {
        return -1;
    }

    snprintf(path, sizeof(path), "%s%s", msg_path, "skill.msg");

    if (!message_load(&skill_message_file, path)) {
        return -1;
    }

    for (index = 0; index < SKILL_COUNT; index++) {
        mesg.num = 100 + index;
        if (message_search(&skill_message_file, &mesg)) {
            skill_data[index].name = mesg.text;
        }

        mesg.num = 200 + index;
        if (message_search(&skill_message_file, &mesg)) {
            skill_data[index].description = mesg.text;
        }

        mesg.num = 300 + index;
        if (message_search(&skill_message_file, &mesg)) {
            skill_data[index].attributes = mesg.text;
        }
    }

    for (index = 0; index < NUM_TAGGED_SKILLS; index++) {
        tag_skill[index] = -1;
    }

    // NOTE: Uninline.
    skill_use_slot_clear();

    return 0;
}

// 0x4982A4
void skill_reset()
{
    int index;

    for (index = 0; index < NUM_TAGGED_SKILLS; index++) {
        tag_skill[index] = -1;
    }

    // NOTE: Uninline.
    skill_use_slot_clear();
}

// 0x4982D4
void skill_exit()
{
    message_exit(&skill_message_file);
}

// 0x4982E4
int skill_load(DB_FILE* stream)
{
    return db_freadIntCount(stream, tag_skill, NUM_TAGGED_SKILLS);
}

// 0x498304
int skill_save(DB_FILE* stream)
{
    return db_fwriteIntCount(stream, tag_skill, NUM_TAGGED_SKILLS);
}

// 0x498324
void skill_set_defaults(CritterProtoData* data)
{
    int index;

    for (index = 0; index < SKILL_COUNT; index++) {
        data->skills[index] = 0;
    }
}

// 0x498340
void skill_set_tags(int* skills, int count)
{
    int index;

    for (index = 0; index < count; index++) {
        tag_skill[index] = skills[index];
    }
}

// 0x498364
void skill_get_tags(int* skills, int count)
{
    int index;

    for (index = 0; index < count; index++) {
        skills[index] = tag_skill[index];
    }
}

// 0x498388
int skill_level(Object* critter, int skill)
{
    SkillDescription* skill_description;
    int points;
    int bonus;
    int value;

    if (skill < 0 || skill >= SKILL_COUNT) {
        return -5;
    }

    points = skill_points(critter, skill);
    if (points < 0) {
        return points;
    }

    skill_description = &(skill_data[skill]);

    if (skill_description->stat2 != -1) {
        bonus = (stat_level(critter, skill_description->stat1) + stat_level(critter, skill_description->stat2)) * skill_description->stat_modifier / 2;
    } else {
        bonus = stat_level(critter, skill_description->stat1) * skill_description->stat_modifier;
    }

    value = skill_description->default_value + bonus + points * skill_description->points_modifier;

    if (critter == obj_dude) {
        if (skill == tag_skill[0] || skill == tag_skill[1] || skill == tag_skill[2] || skill == tag_skill[3]) {
            value += 20 + points * skill_description->points_modifier;
        }

        value += trait_adjust_skill(skill);
        value += perk_adjust_skill(skill);
        value += skill_game_difficulty(skill);
    }

    if (value > SKILL_LEVEL_MAX) {
        value = SKILL_LEVEL_MAX;
    }

    return value;
}

// 0x49847C
int skill_base(int skill)
{
    return skill >= 0 && skill < SKILL_COUNT ? skill_data[skill].default_value : -5;
}

// 0x4984A8
int skill_points(Object* obj, int skill)
{
    if (skill < 0 || skill >= SKILL_COUNT) {
        return 0;
    }

    Proto* proto;
    proto_ptr(obj->pid, &proto);

    return proto->critter.data.skills[skill];
}

// 0x4984E4
int skill_inc_point(Object* obj, int skill)
{
    Proto* proto;
    int unspent_skill_points;
    int level;
    int rc;

    if (obj != obj_dude) {
        return -5;
    }

    if (skill < 0 || skill >= SKILL_COUNT) {
        return -5;
    }

    proto_ptr(obj->pid, &proto);

    unspent_skill_points = stat_pc_get(PC_STAT_UNSPENT_SKILL_POINTS);
    if (unspent_skill_points <= 0) {
        return -4;
    }

    level = skill_level(obj, skill);
    if (level >= SKILL_LEVEL_MAX) {
        return -3;
    }

    rc = stat_pc_set(PC_STAT_UNSPENT_SKILL_POINTS, unspent_skill_points - 1);
    if (rc == 0) {
        proto->critter.data.skills[skill] += 1;
    }

    return rc;
}

// 0x498588
int skill_dec_point(Object* critter, int skill)
{
    Proto* proto;
    int unspent_skill_points;
    int rc;

    if (critter != obj_dude) {
        return -5;
    }

    if (skill < 0 || skill >= SKILL_COUNT) {
        return -5;
    }

    proto_ptr(critter->pid, &proto);

    if (proto->critter.data.skills[skill] <= 0) {
        return -2;
    }

    unspent_skill_points = stat_pc_get(PC_STAT_UNSPENT_SKILL_POINTS);

    rc = stat_pc_set(PC_STAT_UNSPENT_SKILL_POINTS, unspent_skill_points + 1);
    if (rc == 0) {
        proto->critter.data.skills[skill] -= 1;
    }

    return 0;
}

// 0x498608
int skill_result(Object* critter, int skill, int modifier, int* how_much)
{
    int level;
    int critical_chance;
    if (skill < 0 || skill >= SKILL_COUNT) {
        return ROLL_FAILURE;
    }

    level = skill_level(critter, skill);
    critical_chance = stat_level(critter, STAT_CRITICAL_CHANCE);
    return roll_check(level + modifier, critical_chance, how_much);
}

// 0x498640
int skill_contest(Object* attacker, Object* defender, int skill, int attackerModifier, int defenderModifier, int* howMuch)
{
    int attackerRoll;
    int attackerHowMuch;
    int defenderRoll;
    int defenderHowMuch;

    attackerRoll = skill_result(attacker, skill, attackerModifier, &attackerHowMuch);
    if (attackerRoll > ROLL_FAILURE) {
        defenderRoll = skill_result(defender, skill, defenderModifier, &defenderHowMuch);
        if (defenderRoll > ROLL_FAILURE) {
            attackerHowMuch -= defenderHowMuch;
        }

        attackerRoll = roll_check_critical(attackerHowMuch, 0);
    }

    if (howMuch != NULL) {
        *howMuch = attackerHowMuch;
    }

    return attackerRoll;
}

// 0x4986A8
char* skill_name(int skill)
{
    return skill >= 0 && skill < SKILL_COUNT ? skill_data[skill].name : NULL;
}

// 0x4986CC
char* skill_description(int skill)
{
    return skill >= 0 && skill < SKILL_COUNT ? skill_data[skill].description : NULL;
}

// 0x4986F0
char* skill_attribute(int skill)
{
    return skill >= 0 && skill < SKILL_COUNT ? skill_data[skill].attributes : NULL;
}

// 0x498714
int skill_pic(int skill)
{
    return skill >= 0 && skill < SKILL_COUNT ? skill_data[skill].art_num : 0;
}

// 0x498738
static void show_skill_use_messages(Object* obj, int skill, Object* a3, int a4, int criticalChanceModifier)
{
    if (obj != obj_dude) {
        return;
    }

    if (a4 <= 0) {
        return;
    }

    SkillDescription* skillDescription = &(skill_data[skill]);

    int baseExperience = skillDescription->experience;
    if (baseExperience == 0) {
        return;
    }

    if (skillDescription->field_28 && criticalChanceModifier < 0) {
        baseExperience += abs(criticalChanceModifier);
    }

    int xpToAdd = a4 * baseExperience;

    int before = stat_pc_get(PC_STAT_EXPERIENCE);

    if (stat_pc_add_experience(xpToAdd) == 0 && a4 > 0) {
        MessageListItem messageListItem;
        messageListItem.num = 505; // You earn %d XP for honing your skills
        if (message_search(&skill_message_file, &messageListItem)) {
            int after = stat_pc_get(PC_STAT_EXPERIENCE);

            char text[60];
            snprintf(text, sizeof(text), messageListItem.text, after - before);
            display_print(text);
        }
    }
}

// 0x498814
int skill_use(Object* obj, Object* a2, int skill, int criticalChanceModifier)
{
    MessageListItem messageListItem;
    char text[60];

    bool giveExp = true;
    int currentHp = stat_level(a2, STAT_CURRENT_HIT_POINTS);
    int maximumHp = stat_level(a2, STAT_MAXIMUM_HIT_POINTS);

    int hpToHeal = 0;
    int maximumHpToHeal = 0;
    int minimumHpToHeal = 0;

    if (obj == obj_dude) {
        if (skill == SKILL_FIRST_AID || skill == SKILL_DOCTOR) {
            int healerRank = perk_level(PERK_HEALER);
            minimumHpToHeal = 2 * healerRank;
            maximumHpToHeal = 5 * healerRank;
        }
    }

    int criticalChance = stat_level(obj, STAT_CRITICAL_CHANCE) + criticalChanceModifier;

    int damageHealingAttempts = 1;
    int v1 = 0;
    int v2 = 0;

    switch (skill) {
    case SKILL_FIRST_AID:
        if (skill_use_slot_available(SKILL_FIRST_AID) == -1) {
            // 590: You've taxed your ability with that skill. Wait a while.
            // 591: You're too tired.
            // 592: The strain might kill you.
            messageListItem.num = 590 + roll_random(0, 2);
            if (message_search(&skill_message_file, &messageListItem)) {
                display_print(messageListItem.text);
            }

            return -1;
        }

        if (critter_is_dead(a2)) {
            // 512: You can't heal the dead.
            // 513: Let the dead rest in peace.
            // 514: It's dead, get over it.
            messageListItem.num = 512 + roll_random(0, 2);
            if (message_search(&skill_message_file, &messageListItem)) {
                debug_printf(messageListItem.text);
            }

            break;
        }

        if (currentHp < maximumHp) {
            palette_fade_to(black_palette);

            int roll;
            if (critter_body_type(a2) == BODY_TYPE_ROBOTIC) {
                roll = ROLL_FAILURE;
            } else {
                roll = skill_result(obj, skill, criticalChance, &hpToHeal);
            }

            if (roll == ROLL_SUCCESS || roll == ROLL_CRITICAL_SUCCESS) {
                hpToHeal = roll_random(minimumHpToHeal + 1, maximumHpToHeal + 5);
                critter_adjust_hits(a2, hpToHeal);

                if (obj == obj_dude) {
                    // You heal %d hit points.
                    messageListItem.num = 500;
                    if (!message_search(&skill_message_file, &messageListItem)) {
                        return -1;
                    }

                    if (maximumHp - currentHp < hpToHeal) {
                        hpToHeal = maximumHp - currentHp;
                    }

                    snprintf(text, sizeof(text), messageListItem.text, hpToHeal);
                    display_print(text);
                }

                skill_use_slot_add(SKILL_FIRST_AID);

                v1 = 1;

                if (a2 == obj_dude) {
                    intface_update_hit_points(true);
                }
            } else {
                // You fail to do any healing.
                messageListItem.num = 503;
                if (!message_search(&skill_message_file, &messageListItem)) {
                    return -1;
                }

                snprintf(text, sizeof(text), messageListItem.text, hpToHeal);
                display_print(text);
            }

            scr_exec_map_update_scripts();
            palette_fade_to(cmap);
        } else {
            if (obj == obj_dude) {
                // 501: You look healty already
                // 502: %s looks healthy already
                messageListItem.num = (a2 == obj_dude ? 501 : 502);
                if (!message_search(&skill_message_file, &messageListItem)) {
                    return -1;
                }

                if (a2 == obj_dude) {
                    strcpy(text, messageListItem.text);
                } else {
                    snprintf(text, sizeof(text), messageListItem.text, object_name(a2));
                }

                display_print(text);
                giveExp = false;
            }
        }

        if (obj == obj_dude) {
            inc_game_time_in_seconds(1800);
        }

        break;
    case SKILL_DOCTOR:
        if (skill_use_slot_available(SKILL_DOCTOR) == -1) {
            // 590: You've taxed your ability with that skill. Wait a while.
            // 591: You're too tired.
            // 592: The strain might kill you.
            messageListItem.num = 590 + roll_random(0, 2);
            if (message_search(&skill_message_file, &messageListItem)) {
                display_print(messageListItem.text);
            }

            return -1;
        }

        if (critter_is_dead(a2)) {
            // 512: You can't heal the dead.
            // 513: Let the dead rest in peace.
            // 514: It's dead, get over it.
            messageListItem.num = 512 + roll_random(0, 2);
            if (message_search(&skill_message_file, &messageListItem)) {
                display_print(messageListItem.text);
            }
            break;
        }

        if (currentHp < maximumHp || critter_is_crippled(a2)) {
            palette_fade_to(black_palette);

            if (critter_body_type(a2) != BODY_TYPE_ROBOTIC && critter_is_crippled(a2)) {
                // Damage flags which can be healed using "Doctor" skill.
                //
                // 0x498160
                static const int flags[HEALABLE_DAMAGE_FLAGS_LENGTH] = {
                    DAM_BLIND,
                    DAM_CRIP_ARM_LEFT,
                    DAM_CRIP_ARM_RIGHT,
                    DAM_CRIP_LEG_RIGHT,
                    DAM_CRIP_LEG_LEFT,
                };

                for (int index = 0; index < HEALABLE_DAMAGE_FLAGS_LENGTH; index++) {
                    if ((a2->data.critter.combat.results & flags[index]) != 0) {
                        damageHealingAttempts++;

                        int roll = skill_result(obj, skill, criticalChance, &hpToHeal);

                        // 530: damaged eye
                        // 531: crippled left arm
                        // 532: crippled right arm
                        // 533: crippled right leg
                        // 534: crippled left leg
                        messageListItem.num = 530 + index;
                        if (!message_search(&skill_message_file, &messageListItem)) {
                            return -1;
                        }

                        MessageListItem prefix;

                        if (roll == ROLL_SUCCESS || roll == ROLL_CRITICAL_SUCCESS) {
                            a2->data.critter.combat.results &= ~flags[index];

                            // 520: You heal your %s.
                            // 521: You heal the %s.
                            prefix.num = (a2 == obj_dude ? 520 : 521);

                            skill_use_slot_add(SKILL_DOCTOR);

                            v1 = 1;
                            v2 = 1;
                        } else {
                            // 525: You fail to heal your %s.
                            // 526: You fail to heal the %s.
                            prefix.num = (a2 == obj_dude ? 525 : 526);
                        }

                        if (!message_search(&skill_message_file, &prefix)) {
                            return -1;
                        }

                        snprintf(text, sizeof(text), prefix.text, messageListItem.text);
                        display_print(text);
                        show_skill_use_messages(obj, skill, a2, v1, criticalChanceModifier);

                        giveExp = false;
                    }
                }
            }

            int roll;
            if (critter_body_type(a2) == BODY_TYPE_ROBOTIC) {
                roll = ROLL_FAILURE;
            } else {
                int skillValue = skill_level(obj, skill);
                roll = roll_check(skillValue, criticalChance, &hpToHeal);
            }

            if (roll == ROLL_SUCCESS || roll == ROLL_CRITICAL_SUCCESS) {
                hpToHeal = roll_random(minimumHpToHeal + 4, maximumHpToHeal + 10);
                critter_adjust_hits(a2, hpToHeal);

                if (obj == obj_dude) {
                    // You heal %d hit points.
                    messageListItem.num = 500;
                    if (!message_search(&skill_message_file, &messageListItem)) {
                        return -1;
                    }

                    if (maximumHp - currentHp < hpToHeal) {
                        hpToHeal = maximumHp - currentHp;
                    }
                    snprintf(text, sizeof(text), messageListItem.text, hpToHeal);
                    display_print(text);
                }

                if (!v2) {
                    skill_use_slot_add(SKILL_DOCTOR);
                }

                if (a2 == obj_dude) {
                    intface_update_hit_points(true);
                }

                v1 = 1;
                show_skill_use_messages(obj, skill, a2, v1, criticalChanceModifier);
                scr_exec_map_update_scripts();
                palette_fade_to(cmap);

                giveExp = false;
            } else {
                // You fail to do any healing.
                messageListItem.num = 503;
                if (!message_search(&skill_message_file, &messageListItem)) {
                    return -1;
                }

                snprintf(text, sizeof(text), messageListItem.text, hpToHeal);
                display_print(text);

                scr_exec_map_update_scripts();
                palette_fade_to(cmap);
            }
        } else {
            if (obj == obj_dude) {
                // 501: You look healty already
                // 502: %s looks healthy already
                messageListItem.num = (a2 == obj_dude ? 501 : 502);
                if (!message_search(&skill_message_file, &messageListItem)) {
                    return -1;
                }

                if (a2 == obj_dude) {
                    strcpy(text, messageListItem.text);
                } else {
                    snprintf(text, sizeof(text), messageListItem.text, object_name(a2));
                }

                display_print(text);

                giveExp = false;
            }
        }

        if (obj == obj_dude) {
            inc_game_time_in_seconds(3600 * damageHealingAttempts);
        }

        break;
    case SKILL_SNEAK:
    case SKILL_LOCKPICK:
        break;
    case SKILL_STEAL:
        scripts_request_steal_container(obj, a2);
        break;
    case SKILL_TRAPS:
        // You fail to find any traps.
        messageListItem.num = 551;
        if (message_search(&skill_message_file, &messageListItem)) {
            display_print(messageListItem.text);
        }

        return -1;
    case SKILL_SCIENCE:
        // You fail to learn anything.
        messageListItem.num = 552;
        if (message_search(&skill_message_file, &messageListItem)) {
            display_print(messageListItem.text);
        }

        return -1;
    case SKILL_REPAIR:
        // You cannot repair that.
        messageListItem.num = 553;
        if (message_search(&skill_message_file, &messageListItem)) {
            display_print(messageListItem.text);
        }
        return -1;
    default:
        // skill_use: invalid skill used.
        messageListItem.num = 510;
        if (message_search(&skill_message_file, &messageListItem)) {
            debug_printf(messageListItem.text);
        }

        return -1;
    }

    if (giveExp) {
        show_skill_use_messages(obj, skill, a2, v1, criticalChanceModifier);
    }

    if (skill == SKILL_FIRST_AID || skill == SKILL_DOCTOR) {
        scr_exec_map_update_scripts();
    }

    return 0;
}

// 0x4991C8
int skill_check_stealing(Object* a1, Object* a2, Object* item, bool isPlanting)
{
    int howMuch;

    int stealModifier = 1 - gStealCount;

    if (a1 != obj_dude || !perk_level(PERK_PICKPOCKET)) {
        // -4% per item size
        stealModifier -= 4 * item_size(item);

        if (FID_TYPE(a2->fid) == OBJ_TYPE_CRITTER) {
            // check facing: -25% if face to face
            if (is_hit_from_front(a1, a2)) {
                stealModifier -= 25;
            }
        }
    }

    if ((a2->data.critter.combat.results & (DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN)) != 0) {
        stealModifier += 20;
    }

    int stealChance = stealModifier + skill_level(a1, SKILL_STEAL);
    if (stealChance > 95) {
        stealChance = 95;
    }

    int stealRoll;
    if (a1 == obj_dude && isPartyMember(a2)) {
        stealRoll = ROLL_CRITICAL_SUCCESS;
    } else {
        int criticalChance = stat_level(a1, STAT_CRITICAL_CHANCE);
        stealRoll = roll_check(stealChance, criticalChance, &howMuch);
    }

    int catchRoll;
    if (stealRoll == ROLL_CRITICAL_SUCCESS) {
        catchRoll = ROLL_CRITICAL_FAILURE;
    } else if (stealRoll == ROLL_CRITICAL_FAILURE) {
        catchRoll = ROLL_SUCCESS;
    } else {
        int catchChance;
        if (PID_TYPE(a2->pid) == OBJ_TYPE_CRITTER) {
            catchChance = skill_level(a2, SKILL_STEAL) - stealModifier;
        } else {
            catchChance = 30 - stealModifier;
        }

        catchRoll = roll_check(catchChance, 0, &howMuch);
    }

    MessageListItem messageListItem;
    char text[60];

    if (catchRoll != ROLL_SUCCESS && catchRoll != ROLL_CRITICAL_SUCCESS) {
        // 571: You steal the %s.
        // 573: You plant the %s.
        messageListItem.num = isPlanting ? 573 : 571;
        if (!message_search(&skill_message_file, &messageListItem)) {
            return -1;
        }

        snprintf(text, sizeof(text), messageListItem.text, object_name(item));
        display_print(text);

        return 1;
    } else {
        // 570: You're caught stealing the %s.
        // 572: You're caught planting the %s.
        messageListItem.num = isPlanting ? 572 : 570;
        if (!message_search(&skill_message_file, &messageListItem)) {
            return -1;
        }

        snprintf(text, sizeof(text), messageListItem.text, object_name(item));
        display_print(text);

        return 0;
    }
}

// 0x4993D0
static int skill_game_difficulty(int skill)
{
    int game_difficulty;

    switch (skill) {
    case SKILL_FIRST_AID:
    case SKILL_DOCTOR:
    case SKILL_SNEAK:
    case SKILL_LOCKPICK:
    case SKILL_STEAL:
    case SKILL_TRAPS:
    case SKILL_SCIENCE:
    case SKILL_REPAIR:
    case SKILL_SPEECH:
    case SKILL_BARTER:
    case SKILL_GAMBLING:
    case SKILL_OUTDOORSMAN:
        game_difficulty = GAME_DIFFICULTY_NORMAL;
        config_get_value(&game_config, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_GAME_DIFFICULTY_KEY, &game_difficulty);

        if (game_difficulty == GAME_DIFFICULTY_HARD) {
            return -10;
        } else if (game_difficulty == GAME_DIFFICULTY_EASY) {
            return 20;
        }
    }

    return 0;
}

// 0x499428
static int skill_use_slot_available(int skill)
{
    for (int slot = 0; slot < SKILLS_MAX_USES_PER_DAY; slot++) {
        if (timesSkillUsed[skill][slot] == 0) {
            return slot;
        }
    }

    int time = game_time();
    int hoursSinceLastUsage = (time - timesSkillUsed[skill][0]) / GAME_TIME_TICKS_PER_HOUR;
    if (hoursSinceLastUsage <= 24) {
        return -1;
    }

    return SKILLS_MAX_USES_PER_DAY - 1;
}

// 0x49949C
static int skill_use_slot_add(int skill)
{
    int slot = skill_use_slot_available(skill);
    if (slot == -1) {
        return -1;
    }

    if (timesSkillUsed[skill][slot] != 0) {
        for (int i = 0; i < slot; i++) {
            timesSkillUsed[skill][i] = timesSkillUsed[skill][i + 1];
        }
    }

    timesSkillUsed[skill][slot] = game_time();

    return 0;
}

// 0x499508
static int skill_use_slot_clear()
{
    memset(timesSkillUsed, 0, sizeof(timesSkillUsed));
    return 0;
}

// 0x499520
int skill_use_slot_save(DB_FILE* stream)
{
    return db_fwriteIntCount(stream, (int*)timesSkillUsed, SKILL_COUNT * SKILLS_MAX_USES_PER_DAY);
}

// 0x499540
int skill_use_slot_load(DB_FILE* stream)
{
    return db_freadIntCount(stream, (int*)timesSkillUsed, SKILL_COUNT * SKILLS_MAX_USES_PER_DAY);
}

} // namespace fallout
